#include <array>
#include <atomic>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

TEST(strand_test, copies_share_serialization)
{
    const context_lifetime lifetime;
    const auto ctx{lifetime.handle()};
    const auto strand{nori::io::create_strand(ctx).release()};
    const auto copy{strand};
    const std::array<nori::io::strand_handle, 2> handles{strand, copy};
    std::atomic_int running{0};
    std::atomic_bool serialized{true};
    std::atomic_int remaining{192};
    std::promise<void> promise;
    auto future{promise.get_future()};
    context_runner runner{ctx, 4};

    for (int i{0}; i < 192; ++i)
    {
        ASSERT_TRUE(nori::io::post(handles[i % handles.size()], [&]()
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

TEST(strand_test, dropping_handle_does_not_cancel_submitted_work)
{
    const context_lifetime lifetime;
    const auto ctx{lifetime.handle()};
    auto strand{nori::io::create_strand(ctx).release()};
    const auto copy{strand};
    int completed{0};
    ASSERT_TRUE(nori::io::post(strand, [&]()
    {
        ++completed;
    }));
    strand = {};
    ASSERT_TRUE(nori::io::post(copy, [&]()
    {
        ++completed;
    }));
    nori::io::poll(ctx);
    EXPECT_EQ(completed, 2);
    nori::io::destroy(copy);
}
