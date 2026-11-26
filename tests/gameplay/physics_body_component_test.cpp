#include <cmath>
#include <utility>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/physics_body_component.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/scene.hpp>

namespace
{
constexpr float fixed_step{1.0f / 60.0f};

class test_physics_object final : public nori::core::rtti<test_physics_object, nori::gameplay::object>
{
public:
    test_physics_object(nori::core::transform pose, nori::gameplay::physics_body_config config) :
        rtti{pose}
    {
        add_component<nori::gameplay::physics_body_component>(std::move(config));
    }
};

TEST(physics_body_component_test, physics_body_lifecycle)
{
    nori::gameplay::world active;
    auto pose{nori::core::transform::identity};
    pose.position.z = 5.0f;
    const nori::gameplay::physics_body_config config{.type = nori::physics2d::body_type::kinematic_body, .velocity = {{6.0f, 0.0f}, 3.0f}};
    const auto object_handle{active.create_object<test_physics_object>(pose, config)};
    auto& object{*active.find_object(object_handle)};
    auto& body{*object.find_component<nori::gameplay::physics_body_component>()};
    const auto original{body.body_handle()};
    ASSERT_TRUE((active.physics_scene().contains(original))) << "body was not created during world registration";
    active.fixed_update(fixed_step);
    const auto moved{object.root_component().world_transform()};
    EXPECT_NEAR((moved.position.x), (0.1f), 0.0001f) << "kinematic pose was not reflected in gameplay";
    ASSERT_EQ((moved.position.z), (5.0f)) << "kinematic pose was not reflected in gameplay";
    EXPECT_NEAR((moved.rotation.z), (std::sin(0.025f)), 0.0001f) << "kinematic pose was not reflected in gameplay";
    ASSERT_TRUE((body.teleport({{2.0f, 1.0f}, 0.0f}, true))) << "could not queue velocity-preserving teleport";
    active.fixed_update(fixed_step);
    EXPECT_NEAR((object.root_component().world_transform().position.x), (2.1f), 0.0001f) << "velocity-preserving teleport stopped the kinematic body";

    class removal_probe
    {
    public:
        removal_probe(nori::gameplay::world& active, ::nori::gameplay::object& object, nori::gameplay::physics_body_component& body) :
            active_{active},
            object_{object},
            body_{body}
        {
        }

        void remove(float)
        {
            const auto handle{body_.body_handle()};
            ASSERT_TRUE((object_.remove_component(body_))) << "body was removed inside update";
            ASSERT_TRUE((active_.physics_scene().contains(handle))) << "body was removed inside update";
        }

    private:
        nori::gameplay::world& active_;
        ::nori::gameplay::object& object_;
        nori::gameplay::physics_body_component& body_;
    } probe{active, object, body};

    active.scheduler().register_update(probe, nori::gameplay::update_group::input, &removal_probe::remove);
    active.fixed_update(fixed_step);
    ASSERT_FALSE((active.physics_scene().contains(original))) << "body survived component removal";
}

TEST(physics_body_component_test, static_body_sync)
{
    nori::gameplay::world active;
    auto initial_pose{nori::core::transform::identity};
    initial_pose.position = {2.0f, 1.0f, 3.0f};
    const auto handle{active.create_object<test_physics_object>(initial_pose, nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::static_body, .colliders = {{{0.5f, 0.5f}, {}, {}}}})};
    auto& object{*active.find_object(handle)};
    const auto body{object.find_component<nori::gameplay::physics_body_component>()};
    auto next_pose{initial_pose};
    next_pose.position.x = 4.0f;
    next_pose.rotation = nori::core::axis_rotation({0.0f, 0.0f, 1.0f}, 0.5f);
    ASSERT_TRUE((object.root_component().set_relative_transform(next_pose))) << "static root transform was rejected";
    active.fixed_update(fixed_step);
    const auto current{active.physics_scene().transform(body->body_handle())};
    ASSERT_TRUE((current)) << "static root transform was not applied to physics";
    ASSERT_EQ((current->position.x), (4.0f)) << "static root transform was not applied to physics";
    EXPECT_NEAR(current->angle_radians, 0.5f, 0.001f);
    ASSERT_TRUE((body->teleport({{6.0f, 1.0f}, 0.0f}))) << "could not queue static teleport";
    active.fixed_update(fixed_step);
    active.fixed_update(fixed_step);
    ASSERT_EQ((object.root_component().relative_transform().position.x), (6.0f)) << "static teleport was reverted by the root transform";
    ASSERT_EQ((active.physics_scene().transform(body->body_handle())->position.x), (6.0f)) << "static teleport was reverted by the root transform";
}
TEST(physics_body_component_test, contact_query)
{
    nori::gameplay::world active;
    ASSERT_TRUE(active.physics_scene().set_gravity({0.0f, -10.0f}));
    const auto ground{active.create_object<test_physics_object>(nori::core::transform::identity,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::static_body, .colliders = {{{5.0f, 0.5f}, {}, {}}}})};
    auto pose{nori::core::transform::identity};
    pose.position.y = 2.0f;
    const auto falling{active.create_object<test_physics_object>(pose,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::dynamic_body, .lock_rotation = true, .colliders = {{{0.5f, 0.5f}, {}, {}}}})};
    auto* body{active.find_object(falling)->find_component<nori::gameplay::physics_body_component>()};
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->contacts().empty());
    for (int i{}; i < 120; ++i)
    {
        active.fixed_update(fixed_step);
    }
    const auto contacts{body->contacts()};
    ASSERT_FALSE(contacts.empty());
    EXPECT_EQ(contacts.front().other.body, active.find_object(ground)->find_component<nori::gameplay::physics_body_component>()->body_handle());
    EXPECT_GT(contacts.front().normal.y, 0.5f);
    ASSERT_TRUE(body->teleport({{0.0f, 5.0f}, 0.0f}));
    EXPECT_TRUE(body->contacts().empty());
    active.fixed_update(fixed_step);
    EXPECT_GT(active.find_object(falling)->root_component().world_transform().position.y, 4.0f);
    for (int i{}; i < 4 && !body->contacts().empty(); ++i)
    {
        active.fixed_update(fixed_step);
    }
    EXPECT_TRUE(body->contacts().empty());
}
} // namespace
