#include <chrono>
#include <future>
#include <utility>

#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

namespace
{
nori::io::awaitable<void> complete_accept(nori::io::awaitable<nori::io::result<nori::io::unique_connection_handle>> operation, std::promise<nori::io::result<nori::io::unique_connection_handle>>& promise)
{
    promise.set_value(co_await std::move(operation));
}
} // namespace

TEST(acceptor_test, destruction_cancels_pending_accept)
{
    const context_lifetime lifetime;
    const auto ctx{lifetime.handle()};
    auto listener{nori::io::create_acceptor(ctx, {.local = {"127.0.0.1", 0}})};
    ASSERT_TRUE(listener);
    std::promise<nori::io::result<nori::io::unique_connection_handle>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(ctx, complete_accept(nori::io::async_accept(listener->get()), promise)));
    nori::io::poll(ctx);
    listener->reset();
    nori::io::poll(ctx);
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

TEST(acceptor_test, context_cleanup_releases_port_without_affecting_other_context)
{
    context_lifetime first_scope;
    const context_lifetime second_scope;
    const auto first{first_scope.handle()};
    const auto second{second_scope.handle()};
    auto listener{nori::io::create_acceptor(first, {.local = {"127.0.0.1", 0}})};
    ASSERT_TRUE(listener);
    const auto local_port{nori::io::port(listener->get())};
    std::ignore = listener->release();
    first_scope.reset();
    const auto replacement{nori::io::create_acceptor(second, {.local = {"127.0.0.1", local_port}})};
    ASSERT_TRUE(replacement);
    EXPECT_EQ(nori::io::port(replacement->get()), local_port);
}
