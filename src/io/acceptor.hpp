#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

#include <asio/ip/tcp.hpp>

#include <nori/io/acceptor.hpp>

namespace nori::io
{
class context;
class strand;

class acceptor : public std::enable_shared_from_this<acceptor>
{
public:
    acceptor(const std::shared_ptr<context>& ctx, acceptor_options options);
    ~acceptor();

    result<void> open();
    awaitable<result<unique_connection_handle>> async_accept();
    void close();
    std::uint16_t port() const;

private:
    static awaitable<result<unique_connection_handle>> async_accept_impl(std::shared_ptr<acceptor> self);
    void do_close();

    const acceptor_options options_;
    std::shared_ptr<strand> strand_;
    asio::ip::tcp::acceptor acceptor_;
    std::atomic<std::uint16_t> port_;
    std::atomic<bool> open_;
};
} // namespace nori::io
