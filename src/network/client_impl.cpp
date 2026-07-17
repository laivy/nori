#include <optional>
#include <utility>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "client_impl.hpp"

namespace nori::network
{
client::impl::impl(specification spec) :
    spec_{std::move(spec)},
    pool_{std::max(spec_.thread_count, 1uz)},
    is_writing_{false}
{
}

client::impl::~impl()
{
    stop();
}

bool client::impl::start()
{
    {
        std::lock_guard lock{mutex_};
        if (connection_)
        {
            return true;
        }
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver{pool_.context()};
    const auto endpoints{resolver.resolve(spec_.address, std::to_string(spec_.port), ec)};
    if (ec)
    {
        return false;
    }

    boost::asio::ip::tcp::socket socket{pool_.context()};
    boost::asio::connect(socket, endpoints, ec);
    if (ec)
    {
        return false;
    }

    auto conn{std::make_shared<connection>(
        connection::specification{
            .id = 0,
            .socket = std::move(socket),
            .max_receive_packet_size = spec_.max_receive_packet_size,
            .read_timeout = spec_.read_timeout,
        }
    )};
    {
        std::lock_guard lock{mutex_};
        if (connection_)
        {
            conn->close();
            return true;
        }
        connection_ = conn;
    }
    pool_.start();
    boost::asio::co_spawn(pool_.context(), async_receive_packets(conn), boost::asio::detached);
    return true;
}

void client::impl::stop()
{
    std::shared_ptr<connection> conn;
    {
        std::lock_guard lock{mutex_};
        if (!connection_)
        {
            return;
        }
        conn = std::move(connection_);
        pending_packets_ = {};
        is_writing_ = false;
    }
    if (conn)
    {
        conn->close();
    }
    pool_.stop();
    pool_.join();
}

void client::impl::send(out_packet packet)
{
    bool should_start_write{false};
    {
        std::lock_guard lock{mutex_};
        if (!connection_)
        {
            return;
        }
        pending_packets_.push(std::move(packet));
        if (!is_writing_)
        {
            should_start_write = true;
        }
        is_writing_ = true;
    }
    if (should_start_write)
    {
        boost::asio::co_spawn(pool_.context(), async_drain_writes(), boost::asio::detached);
    }
}

void client::impl::set_disconnect_handler(disconnect_handler handler)
{
    std::lock_guard lock{mutex_};
    disconnect_handler_ = std::move(handler);
}

void client::impl::set_packet_handler(packet_handler handler)
{
    std::lock_guard lock{mutex_};
    packet_handler_ = std::move(handler);
}

bool client::impl::is_connected() const
{
    std::lock_guard lock{mutex_};
    return connection_ != nullptr;
}

std::string client::impl::address() const
{
    return spec_.address;
}

std::uint16_t client::impl::port() const
{
    return spec_.port;
}

boost::asio::awaitable<void> client::impl::async_receive_packets(std::shared_ptr<connection> conn)
{
    while (true)
    {
        auto packet{co_await conn->async_receive_packet()};
        if (!packet)
        {
            disconnect(conn);
            co_return;
        }
        packet_handler handler;
        {
            std::lock_guard lock{mutex_};
            if (connection_ != conn)
            {
                co_return;
            }
            handler = packet_handler_;
        }
        if (handler)
        {
            handler(std::move(*packet));
        }
    }
}

boost::asio::awaitable<void> client::impl::async_drain_writes()
{
    while (true)
    {
        std::shared_ptr<connection> conn;
        std::optional<out_packet> packet;
        {
            std::lock_guard lock{mutex_};
            if (!connection_ || pending_packets_.empty())
            {
                is_writing_ = false;
                co_return;
            }
            conn = connection_;
            packet = std::move(pending_packets_.front());
            pending_packets_.pop();
        }
        const boost::system::error_code ec{co_await conn->async_send_packet(std::move(*packet))};
        if (ec)
        {
            disconnect(conn);
            co_return;
        }
    }
}

void client::impl::disconnect(const std::shared_ptr<connection>& conn)
{
    disconnect_handler handler;
    {
        std::lock_guard lock{mutex_};
        if (connection_ != conn)
        {
            return;
        }
        connection_.reset();
        pending_packets_ = {};
        is_writing_ = false;
        handler = disconnect_handler_;
    }
    conn->close();
    if (handler)
    {
        handler();
    }
}
} // namespace nori::network
