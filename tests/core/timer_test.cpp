#include <gtest/gtest.h>

#include <nori/core/timer.hpp>

namespace
{
TEST(timer_test, tick)
{
    nori::core::timer value;
    EXPECT_GE(value.tick(), 0.0);
    value.reset();
    EXPECT_GE(value.tick(), 0.0);
}
} // namespace
