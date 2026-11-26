#include <cstddef>

#include <gtest/gtest.h>

#include <nori/core/hash.hpp>

namespace
{
TEST(hash_test, order)
{
    std::size_t first{};
    nori::core::hash_combine(first, 1);
    nori::core::hash_combine(first, 2);

    std::size_t second{};
    nori::core::hash_combine(second, 2);
    nori::core::hash_combine(second, 1);

    std::size_t repeated{};
    nori::core::hash_combine(repeated, 1);
    nori::core::hash_combine(repeated, 2);

    EXPECT_EQ(first, repeated);
    EXPECT_NE(first, second);
}
} // namespace
