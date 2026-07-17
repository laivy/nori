#pragma once
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "connection.hpp"
#include "io_context_pool.hpp"
#include "server.hpp"

namespace nori::network
{
class server::impl
{
public:
    explicit impl(specification spec);
    ~impl();

    void start();
    void stop();
    void send(client_id id, out_packet packet);
    void disconnect(client_id id);

    [[nodiscard]]
    std::unique_ptr<packet_event> wait_packet(std::stop_token stop_token);

    [[nodiscard]]
    bool is_connected(client_id id) const;

    [[nodiscard]]
    std::size_t connection_count() const;

    [[nodiscard]]
    std::string remote_endpoint(client_id id) const;

    void set_connect_handler(connect_handler handler);
    void set_disconnect_handler(disconnect_handler handler);

    [[nodiscard]]
    std::uint16_t port() const;

    [[nodiscard]]
    std::string address() const;

private:
    struct connection_state
    {
        std::shared_ptr<connection> conn;
        std::queue<out_packet> pending_packets;
        bool writing;
    };

    boost::asio::awaitable<void> async_run();
    boost::asio::awaitable<void> async_receive_packets(std::shared_ptr<connection> conn);

    void open_acceptor();
    void close_acceptor();
    void handle_connect(std::shared_ptr<connection> conn);
    void handle_disconnect(std::shared_ptr<connection> conn);
    void handle_packet(client_id id, in_packet packet);
    void clear_received_packets();

    boost::asio::awaitable<void> async_write_pending_packets(client_id id);

private:
    specification spec_;
    io_context_pool pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::queue<packet_event> received_packets_;
    std::mutex received_packets_mutex_;
    std::condition_variable_any received_packet_available_;
    bool packet_queue_open_;

    client_id next_connection_id_;
    connect_handler connect_handler_;
    disconnect_handler disconnect_handler_;

    mutable std::mutex mutex_;
    bool started_;
    std::unordered_map<client_id, connection_state> connections_;
};
} // namespace nori::network
