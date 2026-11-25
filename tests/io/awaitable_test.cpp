#include <atomic>
#include <chrono>
#include <coroutine>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

namespace
{
template <typename T>
bool poll_until_ready(nori::io::context_handle context, std::future<T>& future)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{3}};
    while (std::chrono::steady_clock::now() < deadline)
    {
        nori::io::poll(context);
        if (future.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
        {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

nori::io::awaitable<int> async_lazy_value(std::atomic_bool& started)
{
    started = true;
    co_return 42;
}

nori::io::awaitable<void> async_get_value(std::atomic_bool& started, std::promise<int>& promise)
{
    const int value{co_await async_lazy_value(started)};
    promise.set_value(value);
}

using compute_result = std::tuple<int, std::thread::id, std::thread::id, std::thread::id>;

template <typename Executor>
nori::io::awaitable<void> async_compute(Executor& compute_context, std::promise<compute_result>& promise)
{
    const std::thread::id parent{std::this_thread::get_id()};
    const auto result{co_await nori::io::run_on(compute_context, []()
    {
        return std::pair{42, std::this_thread::get_id()};
    })};
    promise.set_value(std::tuple{result.first, result.second, parent, std::this_thread::get_id()});
}

nori::io::awaitable<void> suspend_with_lifetime(std::shared_ptr<int>)
{
    co_await std::suspend_always{};
}

nori::io::awaitable<void> async_join_work(nori::io::task_handle handle, std::promise<void>& promise)
{
    co_await nori::io::async_join(handle);
    co_await nori::io::async_join(handle);
    promise.set_value();
}

nori::io::awaitable<void> async_join_with_resource(nori::io::task_handle handle, std::shared_ptr<int>)
{
    co_await nori::io::async_join(handle);
}

nori::io::awaitable<void> async_hold_resource(nori::io::steady_timer_handle clock, std::shared_ptr<int>, std::promise<nori::io::result<void>>& promise)
{
    promise.set_value(co_await nori::io::async_wait(clock, std::chrono::hours{1}));
}

nori::io::awaitable<void> async_join_on_executor(nori::io::task_handle handle, std::promise<void>& waiting, std::promise<std::pair<std::thread::id, std::thread::id>>& promise)
{
    const auto before{std::this_thread::get_id()};
    waiting.set_value();
    co_await nori::io::async_join(handle);
    promise.set_value(std::pair{before, std::this_thread::get_id()});
}
} // namespace

TEST(awaitable_test, awaitable_composition)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    std::atomic_bool started{false};
    std::promise<int> promise;
    auto future{promise.get_future()};

    auto work{async_get_value(started, promise)};
    EXPECT_FALSE(started.load());
    ASSERT_TRUE(nori::io::co_spawn(context, std::move(work)));

    ASSERT_TRUE(wait_until_ready(future));
    EXPECT_EQ(future.get(), 42);
    EXPECT_TRUE(started.load());
}

TEST(awaitable_test, join_completed_work)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    std::atomic_bool started{false};
    std::promise<int> value_promise;
    auto value_future{value_promise.get_future()};
    const auto handle{nori::io::co_spawn(context, async_get_value(started, value_promise))};
    ASSERT_TRUE(handle);
    ASSERT_TRUE(poll_until_ready(context, value_future));
    EXPECT_EQ(value_future.get(), 42);

    std::promise<void> joined_promise;
    auto joined_future{joined_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_join_work(handle, joined_promise)));
    ASSERT_TRUE(poll_until_ready(context, joined_future));
    joined_future.get();
}

TEST(awaitable_test, join_running_work_on_multiple_executors)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const context_lifetime join_context_scope;
    const auto join_context{join_context_scope.handle()};
    const auto join_strand{nori::io::create_strand(join_context).release()};
    const auto clock{nori::io::create_steady_timer(context).release()};
    auto resource{std::make_shared<int>(42)};
    const std::weak_ptr<int> lifetime{resource};
    std::promise<nori::io::result<void>> work_promise;
    auto work_future{work_promise.get_future()};
    auto handle{nori::io::co_spawn(context, async_hold_resource(clock, std::move(resource), work_promise))};
    ASSERT_TRUE(handle);
    const auto copy{handle};
    EXPECT_EQ(handle, copy);
    nori::io::poll(context);

    std::promise<void> first_promise;
    auto first_future{first_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_join_work(copy, first_promise)));
    std::promise<std::pair<std::thread::id, std::thread::id>> second_promise;
    auto second_future{second_promise.get_future()};
    std::promise<void> waiting_promise;
    auto waiting_future{waiting_promise.get_future()};
    context_runner join_runner{join_context, 1};
    ASSERT_TRUE(nori::io::co_spawn(join_strand, async_join_on_executor(copy, waiting_promise, second_promise)));
    handle = {};
    nori::io::poll(context);
    ASSERT_TRUE(wait_until_ready(waiting_future));
    waiting_future.get();
    EXPECT_EQ(first_future.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    EXPECT_EQ(second_future.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    EXPECT_FALSE(lifetime.expired());

    nori::io::cancel(clock);
    context_runner runner{context, 2};
    ASSERT_TRUE(wait_until_ready(first_future));
    ASSERT_TRUE(wait_until_ready(second_future));
    first_future.get();
    const auto threads{second_future.get()};
    EXPECT_EQ(threads.first, threads.second);
    EXPECT_NE(threads.second, std::this_thread::get_id());
    EXPECT_TRUE(lifetime.expired());
    ASSERT_TRUE(wait_until_ready(work_future));
    const auto result{work_future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

TEST(awaitable_test, discarded_handle_keeps_work_running)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto clock{nori::io::create_steady_timer(context).release()};
    auto resource{std::make_shared<int>(42)};
    const std::weak_ptr<int> lifetime{resource};
    std::promise<nori::io::result<void>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_hold_resource(clock, std::move(resource), promise)));
    nori::io::poll(context);
    EXPECT_FALSE(lifetime.expired());
    nori::io::cancel(clock);
    nori::io::poll(context);
    ASSERT_TRUE(wait_until_ready(future));
    EXPECT_FALSE(future.get());
    EXPECT_TRUE(lifetime.expired());
}

TEST(awaitable_test, pending_join_does_not_keep_context_work_alive)
{
    std::weak_ptr<int> lifetime;
    std::weak_ptr<int> waiting_lifetime;
    {
        const context_lifetime context_scope;
        const auto context{context_scope.handle()};
        auto value{std::make_shared<int>(7)};
        lifetime = value;
        const auto handle{nori::io::co_spawn(context, suspend_with_lifetime(std::move(value)))};
        ASSERT_TRUE(handle);
        auto waiting_value{std::make_shared<int>(9)};
        waiting_lifetime = waiting_value;
        ASSERT_TRUE(nori::io::co_spawn(context, async_join_with_resource(handle, std::move(waiting_value))));
        nori::io::poll(context);
        nori::io::stop(context);
    }
    EXPECT_TRUE(lifetime.expired());
    EXPECT_TRUE(waiting_lifetime.expired());
}

TEST(awaitable_test, context_switch)
{
    const context_lifetime parent_context_scope;
    const auto parent_context{parent_context_scope.handle()};
    const context_lifetime compute_context_scope;
    const auto compute_context{compute_context_scope.handle()};
    context_runner compute_runner{compute_context, 1};
    std::promise<compute_result> promise;
    auto future{promise.get_future()};

    ASSERT_TRUE(nori::io::co_spawn(parent_context, async_compute(compute_context, promise)));

    ASSERT_TRUE(poll_until_ready(parent_context, future));
    const auto result{future.get()};
    const auto& [value, worker, parent, resumed]{result};
    EXPECT_EQ(value, 42);
    EXPECT_NE(worker, parent);
    EXPECT_EQ(resumed, parent);
}

TEST(awaitable_test, strand_switch)
{
    const context_lifetime parent_context_scope;
    const auto parent_context{parent_context_scope.handle()};
    const context_lifetime compute_context_scope;
    const auto compute_context{compute_context_scope.handle()};
    const auto compute_strand{nori::io::create_strand(compute_context).release()};
    std::promise<compute_result> promise;
    auto future{promise.get_future()};
    context_runner compute_runner{compute_context, 1};

    ASSERT_TRUE(nori::io::co_spawn(parent_context, async_compute(compute_strand, promise)));

    ASSERT_TRUE(poll_until_ready(parent_context, future));
    const auto result{future.get()};
    const auto& [value, worker, parent, resumed]{result};
    EXPECT_EQ(value, 42);
    EXPECT_NE(worker, parent);
    EXPECT_EQ(resumed, parent);
}

TEST(awaitable_test, awaitable_release)
{
    std::weak_ptr<int> lifetime;
    {
        const context_lifetime context_scope;
        const auto context{context_scope.handle()};
        auto value{std::make_shared<int>(42)};
        lifetime = value;
        ASSERT_TRUE(nori::io::co_spawn(context, suspend_with_lifetime(std::move(value))));
        nori::io::stop(context);
    }

    EXPECT_TRUE(lifetime.expired());
}

TEST(awaitable_test, strand)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 4};
    const auto strand{nori::io::create_strand(context).release()};
    std::atomic_int running{0};
    std::atomic_bool serialized{true};
    std::promise<void> promise;
    auto future{promise.get_future()};
    std::atomic_int remaining{32};

    for (int i{0}; i < 32; ++i)
    {
        ASSERT_TRUE(nori::io::post(strand, [&]()
        {
            if (running.fetch_add(1) != 0)
            {
                serialized = false;
            }
            std::this_thread::yield();
            running.fetch_sub(1);
            if (remaining.fetch_sub(1) == 1)
            {
                promise.set_value();
            }
        }));
    }

    ASSERT_TRUE(wait_until_ready(future));
    future.get();
    EXPECT_TRUE(serialized.load());
}

TEST(awaitable_test, stopped_context)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    const auto strand{nori::io::create_strand(context).release()};

    nori::io::stop(context);

    const auto posted{nori::io::post(context, []()
    {
    })};
    ASSERT_FALSE(posted);
    EXPECT_FALSE(nori::io::co_spawn(context, []() -> nori::io::awaitable<void>
    {
        co_return;
    }()));

    EXPECT_FALSE(nori::io::post(strand, []()
    {
        ADD_FAILURE() << "Rejected work must not run";
    }));

    auto value{std::make_shared<int>(7)};
    const std::weak_ptr<int> lifetime{value};
    EXPECT_FALSE(nori::io::co_spawn(strand, suspend_with_lifetime(std::move(value))));
    EXPECT_TRUE(lifetime.expired());
}

TEST(awaitable_test, context_destruction_joins_pending_io_after_frame_cleanup)
{
    context_lifetime source_scope;
    const auto source{source_scope.handle()};
    const context_lifetime observer_scope;
    const auto observer{observer_scope.handle()};
    context_runner runner{observer, 1};
    const auto clock{nori::io::create_steady_timer(source).release()};
    auto resource{std::make_shared<int>(42)};
    const std::weak_ptr<int> lifetime{resource};
    std::promise<nori::io::result<void>> work_promise;
    const auto task{nori::io::co_spawn(source, async_hold_resource(clock, std::move(resource), work_promise))};
    nori::io::poll(source);
    EXPECT_FALSE(lifetime.expired());

    std::promise<void> waiting_promise;
    auto waiting_future{waiting_promise.get_future()};
    std::promise<std::pair<std::thread::id, std::thread::id>> joined_promise;
    auto joined_future{joined_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(observer, async_join_on_executor(task, waiting_promise, joined_promise)));
    ASSERT_TRUE(wait_until_ready(waiting_future));
    nori::io::stop(source);
    EXPECT_EQ(joined_future.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    source_scope.reset();

    ASSERT_TRUE(wait_until_ready(joined_future));
    EXPECT_TRUE(lifetime.expired());
    const auto threads{joined_future.get()};
    EXPECT_EQ(threads.first, threads.second);
}

TEST(awaitable_test, joining_finished_task_does_not_wait_for_later_work)
{
    const context_lifetime source_scope;
    const auto source{source_scope.handle()};
    std::atomic_bool started{false};
    std::promise<int> value_promise;
    const auto finished{nori::io::co_spawn(source, async_get_value(started, value_promise))};
    nori::io::poll(source);

    auto resource{std::make_shared<int>(7)};
    const std::weak_ptr<int> lifetime{resource};
    ASSERT_TRUE(nori::io::co_spawn(source, suspend_with_lifetime(std::move(resource))));
    nori::io::poll(source);
    std::promise<void> joined_promise;
    auto joined_future{joined_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(source, async_join_work(finished, joined_promise)));
    ASSERT_TRUE(poll_until_ready(source, joined_future));
    EXPECT_FALSE(lifetime.expired());
}

TEST(awaitable_test, completed_task_can_be_joined_after_context_destruction)
{
    context_lifetime source_scope;
    const auto source{source_scope.handle()};
    std::atomic_bool started{false};
    std::promise<int> value_promise;
    const auto finished{nori::io::co_spawn(source, async_get_value(started, value_promise))};
    nori::io::poll(source);
    source_scope.reset();

    const context_lifetime observer_scope;
    const auto observer{observer_scope.handle()};
    std::promise<void> joined_promise;
    auto joined_future{joined_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(observer, async_join_work(finished, joined_promise)));
    ASSERT_TRUE(poll_until_ready(observer, joined_future));
}
