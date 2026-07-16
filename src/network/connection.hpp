#pragma once
#include <chrono>
#include <cstddef>
#include <expected>
#include <string>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include "packet.hpp"
#include "types.hpp"

namespace nori::network
{
class connection
{
public:
    struct specification
    {
        client_id id;
        boost::asio::ip::tcp::socket socket;
        std::size_t max_receive_packet_size;
        std::chrono::steady_clock::duration read_timeout;
    };

public:
    explicit connection(specification spec);
    ~connection();

    connection(const connection&) = delete;
    connection(connection&&) noexcept = default;
    connection& operator=(const connection&) = delete;
    connection& operator=(connection&&) noexcept = default;

    boost::asio::awaitable<boost::system::error_code> async_send_packet(out_packet packet);
    boost::asio::awaitable<std::expected<in_packet, boost::system::error_code>> async_receive_packet();
    void close();

    [[nodiscard]]
    client_id id() const;

    [[nodiscard]]
    std::string remote_endpoint() const;

    [[nodiscard]]
    boost::asio::ip::tcp::socket& socket();

    [[nodiscard]]
    bool is_open() const;

private:
    template <typename MutableBuffers>
    boost::asio::awaitable<boost::system::error_code> async_read_with_timeout(const MutableBuffers& buffers);

private:
    client_id id_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> strand_;

    std::size_t max_receive_packet_size_;
    boost::asio::steady_timer read_timer_;
    std::chrono::steady_clock::duration read_timeout_;
    bool read_timed_out_;
};
} // namespace nori::network
