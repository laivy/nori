#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include <nori/io/connection.hpp>

namespace nori::io
{
struct send_request;
template <typename Result>
class io_operation;

class connection : public std::enable_shared_from_this<connection>
{
public:
    connection(std::shared_ptr<detail::executor> executor, asio::ip::tcp::socket socket, connection_options options);
    ~connection();

    awaitable<result<void>> async_send(opacket packet);
    awaitable<result<ipacket>> async_receive();
    void close();
    void discard_sends();
    endpoint remote_endpoint() const;

private:
    struct send_state
    {
        std::optional<asio::steady_timer> timer;
        bool timed_out;
        std::size_t pending_bytes;
        std::queue<std::shared_ptr<send_request>> pending;
    };

    struct receive_state
    {
        std::optional<asio::steady_timer> timer;
        bool active;
        bool timed_out;
        std::uint64_t generation;
    };

    static awaitable<result<void>> async_send_impl(std::shared_ptr<connection> self, opacket packet);
    result<std::shared_ptr<send_request>> prepare_send(opacket packet);
    void enqueue_send(std::shared_ptr<send_request> request);
    void start_next_send();
    void fail_pending_sends(error_code code);

    static awaitable<result<ipacket>> async_receive_impl(std::shared_ptr<connection> self);
    template <typename MutableBuffers>
    io_operation<result<void>> async_read(MutableBuffers buffers);
    void begin_receive();
    void finish_receive();
    error_code finish_receive(error_code code);

    void do_close();

    const std::shared_ptr<detail::executor> executor_;
    asio::ip::tcp::socket socket_;
    const connection_options options_;
    std::atomic_bool open_;
    send_state send_;
    receive_state receive_;
    const endpoint remote_endpoint_;
};

[[nodiscard]] result<unique_connection_handle> create_connection(std::shared_ptr<detail::executor> target, asio::ip::tcp::socket socket, connection_options options);
} // namespace nori::io
