#pragma once
#include <chrono>
#include <memory>

#include <asio/steady_timer.hpp>

#include <nori/io/steady_timer.hpp>

namespace nori::io
{
class context;
class strand;

class steady_timer : public std::enable_shared_from_this<steady_timer>
{
public:
    explicit steady_timer(const std::shared_ptr<context>& ctx);

    awaitable<result<void>> async_wait(std::chrono::steady_clock::duration duration);
    awaitable<result<void>> async_wait_until(std::chrono::steady_clock::time_point deadline);
    void cancel(bool close);

private:
    void do_cancel(bool close);

    template <typename Expiry>
    static awaitable<result<void>> async_wait_impl(std::shared_ptr<steady_timer> self, Expiry expiry);

    std::shared_ptr<strand> strand_;
    asio::steady_timer timer_;
    bool closed_;
};
} // namespace nori::io
