#include <gtest/gtest.h>

#include <nori/core/vector.hpp>

namespace
{
TEST(vector_test, vec2)
{
    constexpr nori::core::vec2 lhs{3.0f, 4.0f};
    constexpr nori::core::vec2 rhs{1.0f, 2.0f};

    static_assert(+lhs == lhs);
    static_assert(-lhs == nori::core::vec2{-3.0f, -4.0f});
    static_assert(lhs + rhs == nori::core::vec2{4.0f, 6.0f});
    static_assert(lhs - rhs == nori::core::vec2{2.0f, 2.0f});
    static_assert(lhs * 2.0f == nori::core::vec2{6.0f, 8.0f});
    static_assert(2.0f * lhs == nori::core::vec2{6.0f, 8.0f});
    static_assert(lhs / 2.0f == nori::core::vec2{1.5f, 2.0f});
    static_assert(dot(lhs, rhs) == 11.0f);
    static_assert(lhs.length_squared() == 25.0f);

    EXPECT_FLOAT_EQ(lhs.length(), 5.0f);
    constexpr nori::core::vec2 expected{0.6f, 0.8f};
    EXPECT_EQ(lhs.normalized(), expected);
}

TEST(vector_test, vec3)
{
    constexpr nori::core::vec3 lhs{1.0f, 2.0f, 3.0f};
    constexpr nori::core::vec3 rhs{4.0f, 5.0f, 6.0f};

    static_assert(+lhs == lhs);
    static_assert(-lhs == nori::core::vec3{-1.0f, -2.0f, -3.0f});
    static_assert(lhs + rhs == nori::core::vec3{5.0f, 7.0f, 9.0f});
    static_assert(rhs - lhs == nori::core::vec3{3.0f, 3.0f, 3.0f});
    static_assert(lhs * 2.0f == nori::core::vec3{2.0f, 4.0f, 6.0f});
    static_assert(lhs / 2.0f == nori::core::vec3{0.5f, 1.0f, 1.5f});
    static_assert(dot(lhs, rhs) == 32.0f);
    static_assert(cross(lhs, rhs) == nori::core::vec3{-3.0f, 6.0f, -3.0f});
    constexpr nori::core::vec3 length_value{2.0f, 3.0f, 6.0f};
    static_assert(length_value.length_squared() == 49.0f);

    EXPECT_FLOAT_EQ(length_value.length(), 7.0f);
    constexpr nori::core::vec3 expected{0.0f, 0.6f, 0.8f};
    EXPECT_EQ((nori::core::vec3{0.0f, 3.0f, 4.0f}.normalized()), expected);
}

TEST(vector_test, vec4)
{
    constexpr nori::core::vec4 lhs{1.0f, 2.0f, 3.0f, 4.0f};
    constexpr nori::core::vec4 rhs{4.0f, 3.0f, 2.0f, 1.0f};

    static_assert(+lhs == lhs);
    static_assert(-lhs == nori::core::vec4{-1.0f, -2.0f, -3.0f, -4.0f});
    static_assert(lhs + rhs == nori::core::vec4{5.0f, 5.0f, 5.0f, 5.0f});
    static_assert(lhs - rhs == nori::core::vec4{-3.0f, -1.0f, 1.0f, 3.0f});
    static_assert(lhs * 2.0f == nori::core::vec4{2.0f, 4.0f, 6.0f, 8.0f});
    static_assert(lhs / 2.0f == nori::core::vec4{0.5f, 1.0f, 1.5f, 2.0f});
    static_assert(dot(lhs, rhs) == 20.0f);
    constexpr nori::core::vec4 length_value{1.0f, 2.0f, 2.0f, 0.0f};
    static_assert(length_value.length_squared() == 9.0f);

    EXPECT_FLOAT_EQ(length_value.length(), 3.0f);
    constexpr nori::core::vec4 expected{0.0f, 0.0f, 0.0f, 1.0f};
    EXPECT_EQ((nori::core::vec4{0.0f, 0.0f, 0.0f, 2.0f}.normalized()), expected);
}

TEST(vector_test, zero_normalization)
{
    EXPECT_EQ(nori::core::vec2{}.normalized(), nori::core::vec2{});
    EXPECT_EQ(nori::core::vec3{}.normalized(), nori::core::vec3{});
    EXPECT_EQ(nori::core::vec4{}.normalized(), nori::core::vec4{});
}
} // namespace
