#include <gtest/gtest.h>

#include <nori/core/matrix.hpp>

namespace
{
TEST(matrix_test, row_major)
{
    constexpr nori::core::mat4 translation{
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        10.0f,
        20.0f,
        30.0f,
        1.0f,
    };
    constexpr nori::core::mat4 scale{
        2.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        3.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };
    constexpr nori::core::vec4 point{1.0f, 2.0f, 3.0f, 1.0f};

    static_assert(point * nori::core::mat4::identity() == point);
    static_assert(point * translation == nori::core::vec4{11.0f, 22.0f, 33.0f, 1.0f});
    static_assert(point * translation * scale == nori::core::vec4{22.0f, 66.0f, 132.0f, 1.0f});
    static_assert(translation * nori::core::mat4::identity() == translation);
    static_assert(point * (translation * scale) == point * translation * scale);
    static_assert(transpose(transpose(translation)) == translation);
}
} // namespace
