#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include <nori/io.hpp>

static_assert(std::is_same_v<decltype(nori::io::create_context()), nori::io::unique_context_handle>);

TEST(context_test, handles_submit_work_until_stopped)
{
    const auto ctx{nori::io::create_context()};
    const auto strand{nori::io::create_strand(ctx.get())};
    int completed{0};
    EXPECT_TRUE(nori::io::post(strand.get(), [&]()
    {
        ++completed;
    }));
    nori::io::poll(ctx.get());
    EXPECT_EQ(completed, 1);
    nori::io::stop(ctx.get());
    EXPECT_FALSE(nori::io::post(ctx.get(), [&]()
    {
        ++completed;
    }));
    EXPECT_FALSE(nori::io::post(strand.get(), [&]()
    {
        ++completed;
    }));
}
