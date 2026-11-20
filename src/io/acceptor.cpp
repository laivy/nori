#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <asio/error.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

#include <nori/io/acceptor.hpp>
#include <nori/io/awaitable.hpp>

#include "acceptor.hpp"
#include "connection.hpp"
#include "context.hpp"
#include "error.hpp"
#include "io_operation.hpp"
#include "strand.hpp"

namespace
{
nori::io::result<void> configure_acceptor_socket(asio::ip::tcp::acceptor& acceptor)
{
#ifdef _WIN32
    constexpr BOOL enabled{TRUE};
    if (::setsockopt(acceptor.native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR)
    {
        const asio::error_code ec{::WSAGetLastError(), asio::error::get_system_category()};
        return std::unexpected{nori::io::to_error_code(ec)};
    }
#else
    asio::error_code ec;
    acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        return std::unexpected{nori::io::to_error_code(ec)};
    }
#endif
    return {};
}
} // namespace

namespace nori::io
{
acceptor::acceptor(const std::shared_ptr<context>& ctx, acceptor_options options) :
    options_{std::move(options)},
    strand_{std::make_shared<strand>(ctx)},
    acceptor_{strand_->native_executor()},
    port_{options_.local.port},
    open_{false}
{
}

acceptor::~acceptor()
{
    do_close();
}

result<void> acceptor::open()
{
    asio::error_code ec;
    const asio::ip::address address{asio::ip::make_address(options_.local.address, ec)};
    if (ec)
    {
        return std::unexpected{error_code::invalid_address};
    }

    const asio::ip::tcp::endpoint endpoint{address, options_.local.port};
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
        do_close();
        return std::unexpected{to_error_code(ec)};
    }

    if (const auto res{configure_acceptor_socket(acceptor_)}; !res)
    {
        do_close();
        return res;
    }

    acceptor_.bind(endpoint, ec);
    if (!ec)
    {
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    }
    if (ec)
    {
        do_close();
        return std::unexpected{to_error_code(ec)};
    }

    const asio::ip::tcp::endpoint local_endpoint{acceptor_.local_endpoint(ec)};
    if (ec)
    {
        do_close();
        return std::unexpected{to_error_code(ec)};
    }

    port_ = local_endpoint.port();
    open_ = true;
    return {};
}

awaitable<result<unique_connection_handle>> acceptor::async_accept()
{
    return async_accept_impl(shared_from_this());
}

void acceptor::close()
{
    const std::shared_ptr<acceptor> self{shared_from_this()};
    if (!self->strand_->post([self]()
    {
        self->do_close();
    }))
    {
        do_close();
    }
}

std::uint16_t acceptor::port() const
{
    return port_.load();
}

awaitable<result<unique_connection_handle>> acceptor::async_accept_impl(std::shared_ptr<acceptor> self)
{
    using socket_result = result<asio::ip::tcp::socket>;

    if (!co_await schedule_on(self->strand_))
    {
        co_return std::unexpected{error_code::invalid_state};
    }
    if (!self->open_)
    {
        co_return std::unexpected{error_code::invalid_state};
    }

    auto executor{std::make_shared<strand>(self->strand_->root())};
    const asio::any_io_executor native_executor{executor->native_executor()};
    auto start_accept = [self, native_executor](auto completion)
    {
        self->acceptor_.async_accept(native_executor, [completion = std::move(completion)](const asio::error_code& ec, asio::ip::tcp::socket socket) mutable
        {
            if (ec)
            {
                std::move(completion)(std::unexpected{to_error_code(ec)});
                return;
            }
            std::move(completion)(socket_result{std::move(socket)});
        });
    };
    socket_result accepted{co_await async_io_operation<socket_result>(std::move(start_accept))};
    if (!accepted)
    {
        co_return std::unexpected{accepted.error()};
    }

    co_return create_connection(std::move(executor), std::move(*accepted), self->options_.connection);
}

void acceptor::do_close()
{
    if (!open_.exchange(false) && !acceptor_.is_open())
    {
        return;
    }
    asio::error_code ignored;
    acceptor_.cancel(ignored);
    acceptor_.close(ignored);
}
} // namespace nori::io
