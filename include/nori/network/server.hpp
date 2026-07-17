#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <nori/network/packet.hpp>
#include <nori/network/types.hpp>

namespace nori::network
{
class server
{
public:
    struct specification
    {
        std::string address;
        std::uint16_t port;
        std::size_t thread_count;
        std::size_t packet_queue_capacity;
        std::size_t max_receive_packet_size;
        std::chrono::steady_clock::duration read_timeout;
    };

    struct packet_event
    {
        client_id sender_id;
        in_packet packet;
    };

    using connect_handler = std::function<void(client_id)>;
    using disconnect_handler = std::function<void(client_id)>;

public:
    explicit server(specification spec);
    ~server();

    server(const server&) = delete;
    server(server&&) noexcept;
    server& operator=(const server&) = delete;
    server& operator=(server&&) noexcept;

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
    class impl;
    std::unique_ptr<impl> impl_;
};
} // namespace nori::network
