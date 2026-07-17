#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <nori/network/packet.hpp>

namespace nori::network
{
class client
{
public:
    struct specification
    {
        std::string address;
        std::uint16_t port;
        std::size_t thread_count;
        std::size_t max_receive_packet_size;
        std::chrono::steady_clock::duration read_timeout;
    };

    using disconnect_handler = std::function<void()>;
    using packet_handler = std::function<void(in_packet)>;

public:
    explicit client(specification spec);
    ~client();

    client(const client&) = delete;
    client(client&&) noexcept;
    client& operator=(const client&) = delete;
    client& operator=(client&&) noexcept;

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
    class impl;
    std::unique_ptr<impl> impl_;
};
} // namespace nori::network
