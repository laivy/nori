#include <memory>

#include <gtest/gtest.h>

#include <nori/io/context.hpp>
#include <nori/io/post.hpp>
#include <nori/io/strand.hpp>

#include "io_test_helpers.hpp"

namespace
{
template <typename Executor>
void check_post(nori::io::context_handle context, Executor& executor)
{
    int observed{0};
    EXPECT_TRUE(nori::io::post(executor, [value = std::make_unique<int>(42), &observed]()
    {
        observed = *value;
        return false;
    }));
    EXPECT_EQ(observed, 0);
    nori::io::poll(context);
    EXPECT_EQ(observed, 42);

    nori::io::stop(context);
    bool invoked{false};
    EXPECT_FALSE(nori::io::post(executor, [&invoked]()
    {
        invoked = true;
        return 42;
    }));
    nori::io::poll(context);
    EXPECT_FALSE(invoked);
}
} // namespace

TEST(post_test, context_accepts_move_only_work_and_discards_result)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    check_post(context, context);
}

TEST(post_test, strand_accepts_move_only_work_and_discards_result)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    const auto strand{nori::io::create_strand(context).release()};
    check_post(context, strand);
}
