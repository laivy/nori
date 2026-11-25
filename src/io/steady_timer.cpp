#include <chrono>
#include <concepts>
#include <expected>
#include <memory>
#include <utility>

#include <asio/steady_timer.hpp>

#include <nori/io/post.hpp>
#include <nori/io/steady_timer.hpp>
#include <nori/io/strand.hpp>

#include "context.hpp"
#include "error.hpp"
#include "io_operation.hpp"
#include "steady_timer.hpp"
#include "strand.hpp"

namespace nori::io
{
steady_timer::steady_timer(const std::shared_ptr<context>& ctx) :
    strand_{std::make_shared<strand>(ctx)},
    timer_{strand_->native_executor()},
    closed_{false}
{
}

awaitable<result<void>> steady_timer::async_wait(std::chrono::steady_clock::duration duration)
{
    return async_wait_impl(shared_from_this(), duration);
}

awaitable<result<void>> steady_timer::async_wait_until(std::chrono::steady_clock::time_point deadline)
{
    return async_wait_impl(shared_from_this(), deadline);
}

void steady_timer::cancel(bool close)
{
    const std::shared_ptr<steady_timer> self{shared_from_this()};
    if (!strand_->post([self, close]()
    {
        self->do_cancel(close);
    }))
    {
        do_cancel(close);
    }
}

void steady_timer::do_cancel(bool close)
{
    closed_ = closed_ || close;
    timer_.cancel();
}

template <typename Expiry>
awaitable<result<void>> steady_timer::async_wait_impl(std::shared_ptr<steady_timer> self, Expiry expiry)
{
    if (!co_await schedule_on(self->strand_))
    {
        co_return std::unexpected{error_code::invalid_state};
    }
    if (self->closed_)
    {
        co_return std::unexpected{error_code::cancelled};
    }

    if constexpr (std::same_as<Expiry, std::chrono::steady_clock::duration>)
    {
        self->timer_.expires_after(expiry);
    }
    else
    {
        self->timer_.expires_at(expiry);
    }
    const asio::error_code ec{co_await async_io_operation<asio::error_code>([self](auto completion)
    {
        self->timer_.async_wait([self, completion = std::move(completion)](const asio::error_code& ec) mutable
        {
            std::move(completion)(ec);
        });
    })};
    if (ec)
    {
        co_return std::unexpected{to_error_code(ec)};
    }
    co_return result<void>{};
}
} // namespace nori::io
