#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <boost/asio/awaitable.hpp>
#include "client.hpp"
#include "connection.hpp"
#include "io_context_pool.hpp"

namespace nori::network
{
class client::impl
{
public:
    explicit impl(specification spec);
    ~impl();

    bool start();
    void stop();
    void send(out_packet packet);

    void set_disconnect_handler(disconnect_handler handler);
    void set_packet_handler(packet_handler handler);

    [[nodiscard]]
    bool is_connected() const;

    [[nodiscard]]
    std::string address() const;

    [[nodiscard]]
    std::uint16_t port() const;

private:
    boost::asio::awaitable<void> async_receive_packets(std::shared_ptr<connection> conn);
    boost::asio::awaitable<void> async_drain_writes();
    void disconnect(const std::shared_ptr<connection>& conn);

private:
    specification spec_;
    io_context_pool pool_;
    disconnect_handler disconnect_handler_;
    packet_handler packet_handler_;

    mutable std::mutex mutex_;
    std::shared_ptr<connection> connection_;
    std::queue<out_packet> pending_packets_;
    bool is_writing_;
};
} // namespace nori::network
