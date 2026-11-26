#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object.hpp>

namespace
{
class test_attachment final : public nori::core::rtti<test_attachment, nori::gameplay::scene_component>
{
public:
    using rtti::rtti;
};

class test_mesh final : public nori::core::rtti<test_mesh, nori::gameplay::scene_component>
{
public:
    using rtti::rtti;
};

TEST(scene_component_test, components)
{
    nori::gameplay::object value;
    auto& root{value.root_component()};
    auto root_pose{nori::core::transform::identity};
    root_pose.position = {4.0f, 0.0f, 0.0f};
    root_pose.scale = {2.0f, 2.0f, 2.0f};
    ASSERT_EQ((&root.owner()), (&value)) << "unattached component owner or registration incorrect";
    ASSERT_FALSE((root.is_registered())) << "unattached component owner or registration incorrect";
    ASSERT_TRUE((root.set_relative_transform(root_pose))) << "valid root transform was rejected";

    auto hand_pose{nori::core::transform::identity};
    hand_pose.position.x = 1.0f;
    hand_pose.rotation = nori::core::axis_rotation({0.0f, 0.0f, 1.0f}, 0.4f);
    auto& hand{value.add_component<test_attachment>(hand_pose)};
    ASSERT_TRUE((hand.attach_to_component(root, nori::gameplay::keep_relative_transform))) << "could not attach hand to root";

    auto mesh_pose{nori::core::transform::identity};
    mesh_pose.position.x = -3.0f;
    auto& body{value.add_component<test_mesh>(mesh_pose)};
    auto& weapon{value.add_component<test_mesh>()};
    ASSERT_TRUE((body.attach_to_component(root, nori::gameplay::keep_world_transform))) << "keep-world attachment changed the mesh position";
    EXPECT_NEAR((body.world_transform().position.x), (-3.0f), 0.0001f) << "keep-world attachment changed the mesh position";
    ASSERT_TRUE((weapon.attach_to_component(hand, nori::gameplay::snap_to_target_not_including_scale))) << "weapon attachment failed";
    EXPECT_NEAR((weapon.world_transform().position.x), (6.0f), 0.0001f) << "weapon did not snap with its world scale preserved";
    EXPECT_NEAR((weapon.world_transform().scale.x), (1.0f), 0.0001f) << "weapon did not snap with its world scale preserved";
    EXPECT_NEAR((weapon.world_transform().rotation.z), (hand.world_transform().rotation.z), 0.0001f) << "weapon did not snap with its world scale preserved";
    ASSERT_FALSE((hand.attach_to_component(weapon, nori::gameplay::keep_relative_transform))) << "attachment cycle was accepted";
    const auto matches{value.find_components<test_mesh>()};
    ASSERT_EQ((matches.size()), (2)) << "duplicate component lookup did not find both components";
    ASSERT_TRUE((((matches[0] == &body && matches[1] == &weapon) || (matches[0] == &weapon && matches[1] == &body)))) << "duplicate component lookup did not find both components";
    ASSERT_TRUE(((value.find_component<test_mesh>() == &body || value.find_component<test_mesh>() == &weapon))) << "duplicate component lookup did not find both components";
    ASSERT_EQ((value.find_components<nori::gameplay::scene_component>().size()), (4)) << "component lookup excluded derived types";
    ASSERT_TRUE((value.remove_component(hand))) << "removing a parent failed to preserve its child's transform";
    ASSERT_EQ((weapon.parent()), (&root)) << "removing a parent failed to preserve its child's transform";
    EXPECT_NEAR((weapon.world_transform().position.x), (6.0f), 0.0001f) << "removing a parent failed to preserve its child's transform";
    ASSERT_TRUE((weapon.detach_from_component(nori::gameplay::detachment_rule::keep_world))) << "detachment changed the weapon position";
    EXPECT_NEAR((weapon.world_transform().position.x), (6.0f), 0.0001f) << "detachment changed the weapon position";
    ASSERT_TRUE((value.remove_component(body))) << "component removal failed to isolate the requested instance";
    ASSERT_FALSE((value.remove_component(body))) << "component removal failed to isolate the requested instance";
    ASSERT_EQ((value.find_component<test_mesh>()), (&weapon)) << "component removal failed to isolate the requested instance";
    ASSERT_FALSE((value.remove_component(root))) << "component removal failed to isolate the requested instance";
}
} // namespace
