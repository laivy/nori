#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <utility>
#include <vector>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>

#include <nori/core/endian.hpp>
#include <nori/io/connection.hpp>

#include "connection.hpp"
#include "context.hpp"
#include "error.hpp"
#include "executor.hpp"
#include "io_operation.hpp"
#include "registry.hpp"

namespace
{
using packet_size_type = std::uint32_t;

class pending_send_guard
{
public:
    explicit pending_send_guard(std::shared_ptr<nori::io::connection> state);
    ~pending_send_guard();

    pending_send_guard(const pending_send_guard&) = delete;
    pending_send_guard(pending_send_guard&& other) noexcept;

    void release() noexcept;

private:
    std::shared_ptr<nori::io::connection> state_;
};

pending_send_guard::pending_send_guard(std::shared_ptr<nori::io::connection> state) :
    state_{std::move(state)}
{
}

pending_send_guard::~pending_send_guard()
{
    if (state_)
    {
        // backend 종료로 callback이 폐기되면 송신 완료 콜백의 루트 참조 해제
        state_->discard_sends();
    }
}

pending_send_guard::pending_send_guard(pending_send_guard&& other) noexcept :
    state_{std::move(other.state_)}
{
}

void pending_send_guard::release() noexcept
{
    state_.reset();
}

nori::io::endpoint get_remote_endpoint(asio::ip::tcp::socket& socket)
{
    asio::error_code ec;
    const asio::ip::tcp::endpoint remote{socket.remote_endpoint(ec)};
    if (ec)
    {
        return nori::io::endpoint{};
    }
    return nori::io::endpoint{.address = remote.address().to_string(), .port = remote.port()};
}
} // namespace

namespace nori::io
{
struct send_request
{
    std::uint32_t wire_size;
    nori::io::opacket packet;
    std::size_t byte_count;
    std::move_only_function<void(nori::io::result<void>)> completion;
};

connection::connection(std::shared_ptr<detail::executor> executor, asio::ip::tcp::socket socket, connection_options options) :
    executor_{std::move(executor)},
    socket_{std::move(socket)},
    options_{options},
    open_{socket_.is_open()},
    send_{},
    receive_{},
    remote_endpoint_{get_remote_endpoint(socket_)}
{
}

connection::~connection()
{
    do_close();
}

awaitable<result<void>> connection::async_send(opacket packet)
{
    return async_send_impl(shared_from_this(), std::move(packet));
}

awaitable<result<ipacket>> connection::async_receive()
{
    return async_receive_impl(shared_from_this());
}

void connection::close()
{
    const std::shared_ptr<connection> self{shared_from_this()};
    if (!self->executor_->post([self]()
    {
        self->do_close();
    }))
    {
        do_close();
    }
}

void connection::discard_sends()
{
    std::queue<std::shared_ptr<send_request>> pending;
    pending.swap(send_.pending);
    send_.pending_bytes = 0;
    while (!pending.empty())
    {
        const auto request{std::move(pending.front())};
        pending.pop();
        request->completion = nullptr;
    }
}

endpoint connection::remote_endpoint() const
{
    return remote_endpoint_;
}

awaitable<result<void>> connection::async_send_impl(std::shared_ptr<connection> self, opacket packet)
{
    if (!co_await schedule_on(self->executor_))
    {
        co_return std::unexpected{error_code::invalid_state};
    }
    if (!self->open_)
    {
        co_return std::unexpected{error_code::not_connected};
    }

    const auto request{self->prepare_send(std::move(packet))};
    if (!request)
    {
        co_return std::unexpected{request.error()};
    }

    co_return co_await async_io_operation<result<void>>([self, request = *request](auto completion) mutable
    {
        request->completion = std::move(completion);
        self->enqueue_send(std::move(request));
    });
}

result<std::shared_ptr<send_request>> connection::prepare_send(opacket packet)
{
    const std::span<const std::byte> body{packet.bytes()};
    if (body.size() > std::numeric_limits<packet_size_type>::max())
    {
        return std::unexpected{error_code::message_too_large};
    }

    const packet_size_type packet_size{static_cast<packet_size_type>(body.size())};
    if (packet_size > options_.max_packet_size)
    {
        return std::unexpected{error_code::message_too_large};
    }

    const std::size_t request_size{sizeof(packet_size_type) + packet_size};
    if (request_size > options_.max_pending_send_bytes)
    {
        return std::unexpected{error_code::message_too_large};
    }
    if (send_.pending_bytes > options_.max_pending_send_bytes - request_size)
    {
        return std::unexpected{error_code::send_queue_full};
    }

    const packet_size_type wire_size{nori::core::to_endian(packet_size, std::endian::big)};
    return std::make_shared<send_request>(send_request{.wire_size = wire_size, .packet = std::move(packet), .byte_count = request_size});
}

void connection::enqueue_send(std::shared_ptr<send_request> request)
{
    send_.pending_bytes += request->byte_count;
    send_.pending.push(std::move(request));
    if (send_.pending.size() == 1)
    {
        start_next_send();
    }
}

void connection::start_next_send()
{
    if (send_.pending.empty())
    {
        return;
    }
    const std::shared_ptr<send_request> request{send_.pending.front()};
    if (!open_)
    {
        fail_pending_sends(error_code::not_connected);
        return;
    }

    send_.timed_out = false;
    send_.timer.emplace(socket_.get_executor(), std::chrono::steady_clock::now() + options_.send_timeout);

    const std::shared_ptr<connection> self{shared_from_this()};
    send_.timer->async_wait([self, request](const asio::error_code& ec)
    {
        if (ec || self->send_.pending.empty() || self->send_.pending.front() != request)
        {
            return;
        }

        self->send_.timed_out = true;
        self->do_close();
    });

    const std::span<const std::byte> body{request->packet.bytes()};
    const std::array<asio::const_buffer, 2> buffers{asio::buffer(&request->wire_size, sizeof(request->wire_size)), asio::buffer(body.data(), body.size())};
    asio::async_write(socket_, buffers, [self, request, guard = pending_send_guard{self}](const asio::error_code& ec, std::size_t) mutable
    {
        guard.release();
        self->send_.timer.reset();
        self->send_.pending_bytes -= request->byte_count;
        self->send_.pending.pop();

        result<void> send_result;
        if (ec)
        {
            const error_code code{self->send_.timed_out ? error_code::timed_out : to_error_code(ec)};
            self->send_.timed_out = false;
            send_result = std::unexpected{code};
            self->do_close();
            std::move(request->completion)(std::move(send_result));
            self->fail_pending_sends(code == error_code::cancelled ? error_code::cancelled : error_code::not_connected);
            return;
        }

        self->send_.timed_out = false;
        self->start_next_send();
        std::move(request->completion)(std::move(send_result));
    });
}

void connection::fail_pending_sends(error_code code)
{
    std::queue<std::shared_ptr<send_request>> pending;
    pending.swap(send_.pending);
    send_.pending_bytes = 0;

    while (!pending.empty())
    {
        const std::shared_ptr<send_request> request{std::move(pending.front())};
        pending.pop();
        std::move(request->completion)(std::unexpected{code});
    }
}

awaitable<result<ipacket>> connection::async_receive_impl(std::shared_ptr<connection> self)
{
    if (!co_await schedule_on(self->executor_))
    {
        co_return std::unexpected{error_code::invalid_state};
    }
    if (!self->open_)
    {
        co_return std::unexpected{error_code::not_connected};
    }

    self->begin_receive();

    packet_size_type packet_size{};
    result<void> read_result{co_await self->async_read(asio::buffer(&packet_size, sizeof(packet_size)))};
    if (!read_result)
    {
        co_return std::unexpected{self->finish_receive(read_result.error())};
    }

    packet_size = nori::core::to_endian(packet_size, std::endian::big);
    if (packet_size > self->options_.max_packet_size)
    {
        self->finish_receive();
        self->do_close();
        co_return std::unexpected{error_code::message_too_large};
    }

    std::vector<std::byte> body;
    body.resize(packet_size);
    if (!body.empty())
    {
        read_result = co_await self->async_read(asio::buffer(body));
        if (!read_result)
        {
            co_return std::unexpected{self->finish_receive(read_result.error())};
        }
    }

    self->finish_receive();
    co_return ipacket{std::move(body)};
}

template <typename MutableBuffers>
io_operation<result<void>> connection::async_read(MutableBuffers buffers)
{
    const std::shared_ptr<connection> self{shared_from_this()};
    return async_io_operation<result<void>>([self, buffers = std::move(buffers)](auto completion) mutable
    {
        asio::async_read(self->socket_, buffers, [self, completion = std::move(completion)](const asio::error_code& ec, std::size_t) mutable
        {
            if (ec)
            {
                const error_code code{to_error_code(ec)};
                self->do_close();
                std::move(completion)(std::unexpected{code});
                return;
            }
            std::move(completion)(result<void>{});
        });
    });
}

void connection::begin_receive()
{
    receive_.active = true;
    receive_.timed_out = false;
    const std::uint64_t generation{++receive_.generation};
    receive_.timer.emplace(socket_.get_executor(), std::chrono::steady_clock::now() + options_.receive_timeout);

    const std::shared_ptr<connection> self{shared_from_this()};
    receive_.timer->async_wait([self, generation](const asio::error_code& ec)
    {
        if (ec || !self->receive_.active || generation != self->receive_.generation)
        {
            return;
        }

        self->receive_.timed_out = true;
        self->do_close();
    });
}

void connection::finish_receive()
{
    receive_.active = false;
    receive_.timed_out = false;
    receive_.timer.reset();
}

error_code connection::finish_receive(error_code code)
{
    receive_.active = false;
    receive_.timer.reset();
    if (receive_.timed_out)
    {
        receive_.timed_out = false;
        return error_code::timed_out;
    }
    return code;
}

void connection::do_close()
{
    if (!open_.exchange(false))
    {
        return;
    }

    receive_.timer.reset();
    send_.timer.reset();
    asio::error_code ignored;
    socket_.cancel(ignored);
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
}

result<unique_connection_handle> create_connection(std::shared_ptr<detail::executor> target, asio::ip::tcp::socket socket, connection_options options)
{
    asio::error_code ec;
    socket.set_option(asio::ip::tcp::no_delay(options.no_delay), ec);
    if (ec)
    {
        return std::unexpected{to_error_code(ec)};
    }
    const std::shared_ptr<context> ctx{target->root()};
    return unique_connection_handle{insert(ctx->handle(), std::make_shared<connection>(std::move(target), std::move(socket), options)), {}};
}
} // namespace nori::io
