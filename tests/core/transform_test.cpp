#include <gtest/gtest.h>

#include <nori/core/transform.hpp>

namespace
{
TEST(transform_test, identity)
{
    const auto matrix{nori::core::transform_matrix(nori::core::transform::identity)};
    const auto composed{nori::core::compose_transform(nori::core::transform::identity, nori::core::transform::identity)};
    static_cast<void>(matrix);
    EXPECT_FLOAT_EQ(composed.position.x, 0.0f);
    EXPECT_FLOAT_EQ(composed.rotation.w, 1.0f);
    EXPECT_FLOAT_EQ(composed.scale.x, 1.0f);
}

TEST(transform_test, relative_round_trip)
{
    auto parent{nori::core::transform::identity};
    parent.position = {2.0f, 3.0f, 4.0f};
    parent.scale = {2.0f, 2.0f, 2.0f};
    parent.rotation = nori::core::axis_rotation({0.0f, 0.0f, 1.0f}, 0.5f);
    auto child{nori::core::transform::identity};
    child.position = {1.0f, -2.0f, 3.0f};
    const auto world{nori::core::compose_transform(parent, child)};
    const auto relative{nori::core::relative_to(world, parent)};
    EXPECT_NEAR(relative.position.x, child.position.x, 0.0001f);
    EXPECT_NEAR(relative.position.y, child.position.y, 0.0001f);
    EXPECT_NEAR(relative.position.z, child.position.z, 0.0001f);
    EXPECT_NEAR(relative.rotation.w, child.rotation.w, 0.0001f);
    EXPECT_NEAR(relative.scale.x, child.scale.x, 0.0001f);
}
} // namespace
