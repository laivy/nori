#include <optional>
#include <stdexcept>
#include <utility>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include "server_impl.hpp"

namespace nori::network
{
server::impl::impl(specification spec) :
    spec_{std::move(spec)},
    pool_{std::max(spec_.thread_count, 1uz)},
    acceptor_{pool_.context()},
    packet_queue_open_{false},
    next_connection_id_{1},
    started_{false}
{
    if (spec_.port == 0)
    {
        throw std::invalid_argument{"server port must not be 0"};
    }
}

server::impl::~impl()
{
    stop();
}

void server::impl::start()
{
    std::lock_guard lock{mutex_};
    if (started_)
    {
        return;
    }

    open_acceptor();
    started_ = true;
    {
        std::lock_guard received_lock{received_packets_mutex_};
        packet_queue_open_ = true;
    }
    pool_.start();
    boost::asio::co_spawn(pool_.context(), async_run(), boost::asio::detached);
}

void server::impl::stop()
{
    bool was_started{false};
    {
        std::lock_guard lock{mutex_};
        was_started = started_;
        if (was_started)
        {
            started_ = false;
            connections_.clear();
        }
    }
    {
        std::lock_guard received_lock{received_packets_mutex_};
        packet_queue_open_ = false;
        clear_received_packets();
    }
    received_packet_available_.notify_all();
    if (was_started)
    {
        close_acceptor();
        pool_.stop();
        pool_.join();
    }
}

void server::impl::send(client_id id, out_packet packet)
{
    bool should_start_write{false};
    {
        std::lock_guard lock{mutex_};
        if (!started_)
        {
            return;
        }
        auto it{connections_.find(id)};
        if (it == connections_.end())
        {
            return;
        }
        should_start_write = !it->second.writing;
        it->second.pending_packets.push(std::move(packet));
        it->second.writing = true;
    }
    if (should_start_write)
    {
        boost::asio::co_spawn(pool_.context(), async_write_pending_packets(id), boost::asio::detached);
    }
}

void server::impl::disconnect(client_id id)
{
    std::shared_ptr<connection> conn;
    {
        std::lock_guard lock{mutex_};
        if (!started_)
        {
            return;
        }
        auto it{connections_.find(id)};
        if (it == connections_.end())
        {
            return;
        }
        conn = it->second.conn;
    }
    handle_disconnect(std::move(conn));
}

std::unique_ptr<server::packet_event> server::impl::wait_packet(std::stop_token stop_token)
{
    std::unique_lock lock{received_packets_mutex_};
    const auto pred = [this]
    {
        return !packet_queue_open_ || !received_packets_.empty();
    };
    if (!received_packet_available_.wait(lock, stop_token, pred))
    {
        return nullptr;
    }
    if (received_packets_.empty())
    {
        return nullptr;
    }
    auto packet{std::make_unique<packet_event>(std::move(received_packets_.front()))};
    received_packets_.pop();
    return packet;
}

bool server::impl::is_connected(client_id id) const
{
    std::lock_guard lock{mutex_};
    return connections_.contains(id);
}

std::size_t server::impl::connection_count() const
{
    std::lock_guard lock{mutex_};
    return connections_.size();
}

std::string server::impl::remote_endpoint(client_id id) const
{
    std::shared_ptr<connection> conn;
    {
        std::lock_guard lock{mutex_};
        auto it{connections_.find(id)};
        if (it == connections_.end())
        {
            return {};
        }
        conn = it->second.conn;
    }
    return conn->remote_endpoint();
}

void server::impl::set_connect_handler(connect_handler handler)
{
    std::lock_guard lock{mutex_};
    connect_handler_ = std::move(handler);
}

void server::impl::set_disconnect_handler(disconnect_handler handler)
{
    std::lock_guard lock{mutex_};
    disconnect_handler_ = std::move(handler);
}

std::uint16_t server::impl::port() const
{
    return spec_.port;
}

std::string server::impl::address() const
{
    return spec_.address;
}

boost::asio::awaitable<void> server::impl::async_run()
{
    while (acceptor_.is_open())
    {
        boost::system::error_code ec;
        boost::asio::ip::tcp::socket socket{co_await acceptor_.async_accept(boost::asio::redirect_error(boost::asio::use_awaitable, ec))};
        if (ec)
        {
            if (!acceptor_.is_open())
            {
                co_return;
            }
            if (ec == boost::asio::error::operation_aborted)
            {
                co_return;
            }
            continue;
        }
        auto conn{std::make_shared<connection>(
            connection::specification{
                .id = next_connection_id_++,
                .socket = std::move(socket),
                .max_receive_packet_size = spec_.max_receive_packet_size,
                .read_timeout = spec_.read_timeout,
            }
        )};
        handle_connect(conn);
        boost::asio::co_spawn(pool_.context(), async_receive_packets(conn), boost::asio::detached);
    }
}

boost::asio::awaitable<void> server::impl::async_receive_packets(std::shared_ptr<connection> conn)
{
    while (true)
    {
        auto packet{co_await conn->async_receive_packet()};
        if (!packet)
        {
            handle_disconnect(conn);
            co_return;
        }
        handle_packet(conn->id(), std::move(*packet));
    }
}

void server::impl::open_acceptor()
{
    if (acceptor_.is_open())
    {
        return;
    }
    const boost::asio::ip::address address{boost::asio::ip::make_address(spec_.address)};
    const boost::asio::ip::tcp::endpoint endpoint{address, spec_.port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

void server::impl::close_acceptor()
{
    boost::system::error_code ec;
    std::ignore = acceptor_.cancel(ec);
    std::ignore = acceptor_.close(ec);
}

void server::impl::handle_connect(std::shared_ptr<connection> conn)
{
    const client_id id{conn->id()};
    connect_handler handler;
    {
        std::lock_guard lock{mutex_};
        if (started_)
        {
            connections_.try_emplace(id, connection_state{.conn = std::move(conn), .writing = false});
            handler = connect_handler_;
        }
    }
    if (handler)
    {
        handler(id);
    }
}

void server::impl::handle_disconnect(std::shared_ptr<connection> conn)
{
    const client_id id{conn->id()};
    disconnect_handler handler;
    {
        std::lock_guard lock{mutex_};
        if (!started_)
        {
            return;
        }
        if (!connections_.contains(id))
        {
            return;
        }
        connections_.erase(id);
        handler = disconnect_handler_;
    }
    conn->close();
    if (handler)
    {
        handler(id);
    }
}

void server::impl::handle_packet(client_id id, in_packet packet)
{
    {
        std::lock_guard lock{mutex_};
        if (!started_)
        {
            return;
        }
        if (!connections_.contains(id))
        {
            return;
        }
    }

    bool should_disconnect{false};
    {
        std::lock_guard lock{received_packets_mutex_};
        if (!packet_queue_open_)
        {
            return;
        }
        if (received_packets_.size() >= spec_.packet_queue_capacity)
        {
            should_disconnect = true;
        }
        else
        {
            received_packets_.push(packet_event{.sender_id = id, .packet = std::move(packet)});
        }
    }
    if (should_disconnect)
    {
        disconnect(id);
        return;
    }
    received_packet_available_.notify_one();
}

void server::impl::clear_received_packets()
{
    received_packets_ = {};
}

boost::asio::awaitable<void> server::impl::async_write_pending_packets(client_id id)
{
    while (true)
    {
        std::shared_ptr<connection> conn;
        std::optional<out_packet> packet;
        {
            std::lock_guard lock{mutex_};
            if (!started_)
            {
                co_return;
            }
            auto it{connections_.find(id)};
            if (it == connections_.end())
            {
                co_return;
            }
            connection_state& state{it->second};
            if (state.pending_packets.empty())
            {
                state.writing = false;
                co_return;
            }
            conn = state.conn;
            packet.emplace(std::move(state.pending_packets.front()));
            state.pending_packets.pop();
        }

        const boost::system::error_code ec{co_await conn->async_send_packet(std::move(*packet))};
        if (ec)
        {
            handle_disconnect(conn);
            co_return;
        }
    }
}
} // namespace nori::network
