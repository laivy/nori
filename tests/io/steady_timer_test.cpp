#include <chrono>
#include <future>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

namespace
{
nori::io::awaitable<void> async_wait_steady_timer(nori::io::steady_timer_handle clock, std::chrono::steady_clock::duration duration, std::promise<nori::io::result<void>>& promise)
{
    promise.set_value(co_await nori::io::async_wait(clock, duration));
}

nori::io::awaitable<void> async_complete_wait(nori::io::awaitable<nori::io::result<void>> operation, std::promise<nori::io::result<void>>& promise)
{
    promise.set_value(co_await std::move(operation));
}

nori::io::awaitable<void> async_observe_wait(nori::io::awaitable<nori::io::result<void>> operation, std::promise<nori::io::result<std::chrono::steady_clock::time_point>>& promise)
{
    const auto res{co_await std::move(operation)};
    promise.set_value(res.transform([]()
    {
        return std::chrono::steady_clock::now();
    }));
}

class steady_timer_lifetime_test : public testing::TestWithParam<bool>
{
protected:
    nori::io::awaitable<nori::io::result<void>> make_wait(nori::io::steady_timer_handle clock, std::chrono::steady_clock::duration duration);
};

nori::io::awaitable<nori::io::result<void>> steady_timer_lifetime_test::make_wait(nori::io::steady_timer_handle clock, std::chrono::steady_clock::duration duration)
{
    if (GetParam())
    {
        return nori::io::async_wait_until(clock, std::chrono::steady_clock::now() + duration);
    }
    return nori::io::async_wait(clock, duration);
}
} // namespace

TEST(steady_timer_test, wait_expires)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context).release()};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_wait_steady_timer(clock, std::chrono::milliseconds{1}, promise)));
    context_runner runner{context, 1};

    ASSERT_TRUE(wait_until_ready(future));
    EXPECT_TRUE(future.get());
}

TEST(steady_timer_test, wait_until_expires_at_deadline)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context)};
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds{30}};
    std::promise<nori::io::result<std::chrono::steady_clock::time_point>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_observe_wait(nori::io::async_wait_until(clock.get(), deadline), promise)));
    context_runner runner{context, 1};

    ASSERT_TRUE(wait_until_ready(future));
    const auto completed{future.get()};
    ASSERT_TRUE(completed);
    EXPECT_GE(*completed, deadline);
}

TEST(steady_timer_test, wait_until_past_deadline_completes)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context)};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    const auto deadline{std::chrono::steady_clock::now() - std::chrono::hours{1}};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(nori::io::async_wait_until(clock.get(), deadline), promise)));
    context_runner runner{context, 1};

    ASSERT_TRUE(wait_until_ready(future));
    EXPECT_TRUE(future.get());
}

TEST(steady_timer_test, wait_until_keeps_deadline_when_execution_is_delayed)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context)};
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{1}};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(nori::io::async_wait_until(clock.get(), deadline), promise)));
    std::this_thread::sleep_until(deadline);
    context_runner runner{context, 1};

    ASSERT_EQ(future.wait_for(std::chrono::milliseconds{200}), std::future_status::ready);
    EXPECT_TRUE(future.get());
}

TEST_P(steady_timer_lifetime_test, cancel_and_reuse)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context).release()};
    std::promise<nori::io::result<void>> cancelled_promise;
    auto cancelled_future{cancelled_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(make_wait(clock, std::chrono::hours{1}), cancelled_promise)));
    nori::io::poll(context);
    EXPECT_EQ(cancelled_future.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    nori::io::cancel(clock);
    nori::io::poll(context);
    ASSERT_TRUE(wait_until_ready(cancelled_future));
    const auto cancelled{cancelled_future.get()};
    ASSERT_FALSE(cancelled);
    EXPECT_EQ(cancelled.error(), nori::io::error_code::cancelled);

    std::promise<nori::io::result<void>> completed_promise;
    auto completed_future{completed_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(make_wait(clock, std::chrono::seconds{0}), completed_promise)));
    context_runner runner{context, 2};
    ASSERT_TRUE(wait_until_ready(completed_future));
    EXPECT_TRUE(completed_future.get());
}

TEST_P(steady_timer_lifetime_test, destruction_cancels_pending_wait)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    {
        const auto clock{nori::io::create_steady_timer(context)};
        ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(make_wait(clock.get(), std::chrono::hours{1}), promise)));
        nori::io::poll(context);
    }
    nori::io::poll(context);
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

TEST_P(steady_timer_lifetime_test, destruction_before_wait_registration)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    {
        const auto clock{nori::io::create_steady_timer(context).release()};
        ASSERT_TRUE(nori::io::co_spawn(context, async_complete_wait(make_wait(clock, std::chrono::hours{1}), promise)));
        nori::io::destroy(clock);
    }
    nori::io::poll(context);
    ASSERT_EQ(future.wait_for(std::chrono::seconds{0}), std::future_status::ready);
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

INSTANTIATE_TEST_SUITE_P(relative_and_absolute, steady_timer_lifetime_test, testing::Bool());
