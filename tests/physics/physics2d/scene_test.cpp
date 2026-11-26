#include <algorithm>
#include <limits>

#include <gtest/gtest.h>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/scene.hpp>

namespace
{
bool has_contact_normal(const nori::physics2d::scene& world, nori::physics2d::body_handle body, nori::core::vec2 direction)
{
    return std::ranges::any_of(world.contacts_of(body), [&](const auto& contact)
    {
        return contact.normal.x * direction.x + contact.normal.y * direction.y > 0.5f &&
            contact.relative_velocity.x * contact.normal.x + contact.relative_velocity.y * contact.normal.y <= 0.01f &&
            (contact.separation <= 0.0f || contact.normal_impulse > 0.0f);
    });
}

TEST(physics2d_scene_test, body_motion)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle ground{world.create_body({nori::physics2d::body_type::static_body, {{}, 0.0f}, {}})};
    const nori::physics2d::body_handle platform{world.create_body({nori::physics2d::body_type::kinematic_body, {{0.0f, 3.0f}, 0.0f}, {{6.0f, 0.0f}, 0.0f}})};
    const nori::physics2d::body_handle falling{world.create_body({nori::physics2d::body_type::dynamic_body, {{0.0f, 3.0f}, 0.0f}, {}})};
    ASSERT_TRUE(world.contains(ground)) << "all body types can be created";
    ASSERT_TRUE(world.contains(platform)) << "all body types can be created";
    ASSERT_TRUE(world.contains(falling)) << "all body types can be created";

    ASSERT_TRUE(world.set_transform(ground, {{2.0f, 1.0f}, 0.5f})) << "static body accepts a transform";
    ASSERT_FALSE(world.set_velocity(ground, {{6.0f, 0.0f}, 0.0f})) << "static body rejects velocity";
    ASSERT_FALSE(world.apply_force(platform, {60.0f, 0.0f})) << "kinematic body rejects force";
    ASSERT_FALSE(world.apply_linear_impulse(ground, {2.0f, 0.0f})) << "static body rejects impulse";
    ASSERT_TRUE(world.apply_linear_impulse(falling, {2.0f, 0.0f})) << "dynamic body accepts impulse";
    EXPECT_NEAR(world.velocity_of(falling).value().linear.x, 2.0f, 0.001f) << "impulse changes velocity immediately";
    ASSERT_TRUE(world.apply_force(falling, {60.0f, 0.0f})) << "dynamic body accepts force";

    world.step(1.0f / 60.0f);

    const nori::physics2d::body_transform ground_pose{world.transform_of(ground).value()};
    EXPECT_NEAR(ground_pose.position.x, 2.0f, 0.001f) << "static body stays at its assigned transform";
    EXPECT_NEAR(ground_pose.position.y, 1.0f, 0.001f) << "static body stays at its assigned transform";
    EXPECT_NEAR(ground_pose.angle_radians, 0.5f, 0.001f) << "static body stays at its assigned transform";
    const nori::physics2d::body_transform platform_pose{world.transform_of(platform).value()};
    EXPECT_NEAR(platform_pose.position.x, 0.1f, 0.001f) << "kinematic body advances one 60 Hz tick";
    EXPECT_NEAR(platform_pose.position.y, 3.0f, 0.001f) << "kinematic body advances one 60 Hz tick";
    ASSERT_LT(world.transform_of(falling).value().position.y, 3.0f) << "gravity lowers the shapeless dynamic body";
    EXPECT_NEAR(world.velocity_of(falling).value().linear.x, 3.0f, 0.001f) << "force changes velocity over one tick";

    ASSERT_TRUE(world.set_velocity(platform, {{-3.0f, 0.0f}, 3.0f})) << "kinematic body accepts linear and angular velocity";
    world.step(1.0f / 60.0f);
    const nori::physics2d::body_transform updated_pose{world.transform_of(platform).value()};
    EXPECT_NEAR(updated_pose.position.x, 0.05f, 0.001f) << "updated velocity changes position and angle";
    EXPECT_NEAR(updated_pose.angle_radians, 0.05f, 0.001f) << "updated velocity changes position and angle";
}

TEST(physics2d_scene_test, step_interval)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle body{world.create_body({nori::physics2d::body_type::kinematic_body, {{}, 0.0f}, {{6.0f, 0.0f}, 0.0f}})};
    world.step(1.0f / 30.0f);
    EXPECT_NEAR(world.transform_of(body).value().position.x, 0.2f, 0.001f) << "step advances by the supplied interval";

    world.step(0.0f);
    world.step(-1.0f);
    world.step(std::numeric_limits<float>::infinity());
    world.step(std::numeric_limits<float>::quiet_NaN());
    EXPECT_NEAR(world.transform_of(body).value().position.x, 0.2f, 0.001f) << "invalid intervals leave the world unchanged";
}

TEST(physics2d_scene_test, box_collision)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle ground{world.create_body({nori::physics2d::body_type::static_body, {{}, 0.0f}, {}})};
    const nori::physics2d::body_handle falling{world.create_body({nori::physics2d::body_type::dynamic_body, {{0.0f, 4.0f}, 0.0f}, {}})};
    ASSERT_TRUE(world.add_box_collider(ground, {{5.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "ground collider can be created";
    ASSERT_TRUE(world.add_box_collider(falling, {{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "dynamic collider can be created";
    int begins{};
    for (int i{}; i < 180; ++i)
    {
        world.step(1.0f / 60.0f);
        for (const auto& event : world.contact_events())
        {
            begins += event.type == nori::physics2d::contact_type::contact && event.phase == nori::physics2d::contact_phase::begin;
        }
    }
    const nori::physics2d::body_transform pose{world.transform_of(falling).value()};
    const nori::physics2d::body_velocity velocity{world.velocity_of(falling).value()};
    EXPECT_NEAR(pose.position.y, 1.0f, 0.05f) << "dynamic box rests on ground";
    EXPECT_NEAR(velocity.linear.y, 0.0f, 0.05f) << "dynamic box stops on ground";
    ASSERT_EQ(begins, 1) << "collision did not begin exactly once";
}

TEST(physics2d_scene_test, collider_configuration)
{
    nori::physics2d::scene world;
    const auto with_override{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .mass_override = 8.0f})};
    nori::physics2d::box_collider_desc collider{{0.5f, 0.5f}, {2.0f, 0.0f}, 0.6f, 0.0f, 2.0f, {2, 4}, false};
    ASSERT_TRUE(world.add_box_collider(with_override, collider)) << "configured collider rejected";
    ASSERT_TRUE(world.apply_linear_impulse(with_override, {8.0f, 0.0f})) << "adding a collider changed the explicit mass";
    EXPECT_NEAR(world.velocity_of(with_override).value().linear.x, 1.0f, 0.001f) << "adding a collider changed the explicit mass";
    const auto hit{world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {4, 2})};
    ASSERT_TRUE(hit) << "ray missed the offset collider";
    ASSERT_EQ(hit->collider.body, with_override) << "ray missed the offset collider";
    ASSERT_EQ(hit->collider.index, 0) << "ray missed the offset collider";
    EXPECT_NEAR(hit->point.y, 0.5f, 0.001f) << "ray missed the offset collider";
    EXPECT_NEAR(hit->normal.y, 1.0f, 0.001f) << "ray missed the offset collider";
    EXPECT_NEAR(hit->fraction, 0.375f, 0.001f) << "ray missed the offset collider";
    ASSERT_FALSE(world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {1, 2})) << "ray ignored the collider mask";
    ASSERT_FALSE(world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {4, 1})) << "ray ignored the collider category";

    const auto density_body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{8.0f, 0.0f}, 0.0f}})};
    collider.center = {};
    ASSERT_TRUE(world.add_box_collider(density_body, collider)) << "density collider rejected";
    ASSERT_TRUE(world.apply_linear_impulse(density_body, {2.0f, 0.0f})) << "density does not determine dynamic mass";
    EXPECT_NEAR(world.velocity_of(density_body).value().linear.x, 1.0f, 0.001f) << "density does not determine dynamic mass";

    collider.is_sensor = true;
    const auto sensor_body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{10.0f, 0.0f}, 0.0f}})};
    ASSERT_TRUE(world.add_box_collider(sensor_body, collider)) << "sensor-only body lost its default dynamic mass";
    ASSERT_TRUE(world.apply_linear_impulse(sensor_body, {1.0f, 0.0f})) << "sensor-only body lost its default dynamic mass";
    EXPECT_NEAR(world.velocity_of(sensor_body).value().linear.x, 1.0f, 0.001f) << "sensor-only body lost its default dynamic mass";

    const nori::physics2d::box_collider_desc boxes[]{{{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}, {{}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}};
    ASSERT_FALSE(world.contains(world.create_body({.type = nori::physics2d::body_type::static_body, .transform = {{12.0f, 0.0f}, 0.0f}, .colliders = boxes}))) << "failed body creation left a collider in the scene";
    ASSERT_FALSE(world.cast_ray({12.0f, 2.0f}, {0.0f, -4.0f})) << "failed body creation left a collider in the scene";
}

TEST(physics2d_scene_test, sensor_events)
{
    nori::physics2d::scene world;
    ASSERT_TRUE(world.set_gravity({})) << "could not disable gravity";
    const nori::physics2d::box_collider_desc boxes[]{
        {{0.3f, 1.0f}, {-0.65f, 0.0f}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true},
        {{0.3f, 1.0f}, {0.65f, 0.0f}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true}
    };
    const auto sensor{world.create_body({.type = nori::physics2d::body_type::static_body, .colliders = boxes})};
    const auto visitor{world.create_body({.type = nori::physics2d::body_type::dynamic_body})};
    ASSERT_TRUE(world.contains(sensor)) << "sensor event setup failed";
    ASSERT_TRUE(world.add_box_collider(visitor, {{0.75f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "sensor event setup failed";
    constexpr float step{1.0f / 60.0f};
    world.step(step);
    const auto begins{world.contact_events()};
    ASSERT_EQ(begins.size(), 2) << "two colliders did not begin independently";
    ASSERT_NE(begins[0].first.index, begins[1].first.index) << "two colliders did not begin independently";
    for (const auto& event : begins)
    {
        ASSERT_EQ(event.type, nori::physics2d::contact_type::sensor) << "invalid sensor event begin data";
        ASSERT_EQ(event.phase, nori::physics2d::contact_phase::begin) << "invalid sensor event begin data";
        ASSERT_EQ(event.first.body, sensor) << "invalid sensor event begin data";
        ASSERT_EQ(event.second.body, visitor) << "invalid sensor event begin data";
    }
    EXPECT_TRUE(world.contacts_of(visitor).empty()) << "sensor affected physical motion";
    EXPECT_NEAR(world.transform_of(visitor).value().position.x, 0.0f, 0.001f) << "sensor affected physical motion";
    ASSERT_TRUE(world.set_transform(visitor, {{0.8f, 0.0f}, 0.0f})) << "could not leave one sensor";
    world.step(step);
    const auto first_end{world.contact_events()};
    ASSERT_EQ(first_end.size(), 1) << "leaving one collider ended the wrong sensor event";
    ASSERT_EQ(first_end.front().type, nori::physics2d::contact_type::sensor);
    ASSERT_EQ(first_end.front().phase, nori::physics2d::contact_phase::end) << "leaving one collider ended the wrong sensor event";
    ASSERT_EQ(first_end.front().first.index, 0) << "leaving one collider ended the wrong sensor event";

    ASSERT_TRUE(world.destroy_body(visitor)) << "could not delete the visitor";
    world.step(0.0f);
    ASSERT_FALSE(world.contact_events().empty());
    ASSERT_EQ(world.contact_events().front().first.index, 0) << "invalid step replaced the event batch";
    world.step(step);
    const auto removal_end{world.contact_events()};
    ASSERT_EQ(removal_end.size(), 1) << "destroying the visitor did not end its remaining sensor event once";
    ASSERT_EQ(removal_end.front().type, nori::physics2d::contact_type::sensor);
    ASSERT_EQ(removal_end.front().phase, nori::physics2d::contact_phase::end) << "destroying the visitor did not end its remaining sensor event once";
    ASSERT_EQ(removal_end.front().first.index, 1) << "destroying the visitor did not end its remaining sensor event once";
    ASSERT_EQ(removal_end.front().second.body, visitor) << "destroying the visitor did not end its remaining sensor event once";
    world.step(step);
    ASSERT_TRUE(world.contact_events().empty()) << "removed visitor continued to produce events";
}

TEST(physics2d_scene_test, box_collider_validation)
{
    nori::physics2d::scene world;
    nori::physics2d::scene other;
    const nori::physics2d::body_handle body{world.create_body({nori::physics2d::body_type::dynamic_body, {}, {}})};
    const nori::physics2d::body_handle foreign{other.create_body({})};
    ASSERT_FALSE(world.add_box_collider({}, {{1.0f, 1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "default handle is rejected";
    ASSERT_FALSE(world.add_box_collider(foreign, {{1.0f, 1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "foreign handle is rejected";
    ASSERT_FALSE(world.add_box_collider(body, {{0.0f, 1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "zero extent is rejected";
    ASSERT_FALSE(world.add_box_collider(body, {{1.0f, -1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "negative extent is rejected";
    ASSERT_FALSE(world.add_box_collider(body, {{std::numeric_limits<float>::infinity(), 1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "infinite extent is rejected";
    ASSERT_FALSE(world.add_box_collider(body, {{1.0f, std::numeric_limits<float>::quiet_NaN()}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "NaN extent is rejected";
    ASSERT_TRUE(world.add_box_collider(body, {{1.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "first collider can be added";
    ASSERT_TRUE(world.add_box_collider(body, {{1.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "multiple colliders can be added";
    ASSERT_TRUE(world.apply_linear_impulse(body, {4.0f, 0.0f})) << "impulse applies to body with colliders";
    EXPECT_NEAR(world.velocity_of(body).value().linear.x, 1.0f, 0.001f) << "collider areas determine dynamic mass";
    ASSERT_TRUE(world.destroy_body(body)) << "body with colliders can be destroyed";
    ASSERT_FALSE(world.add_box_collider(body, {{1.0f, 1.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "destroyed handle is rejected";
}

TEST(physics2d_scene_test, contact_normals_and_rotation)
{
    nori::physics2d::scene world;
    ASSERT_TRUE(world.set_gravity({0.0f, -20.0f})) << "gravity can be configured";
    ASSERT_FALSE(world.set_gravity({0.0f, std::numeric_limits<float>::infinity()})) << "invalid gravity is rejected";
    const auto ground{world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{0.0f, 2.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(world.add_box_collider(ground, {{5.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "colliders created";
    ASSERT_TRUE(world.add_box_collider(body, {{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "colliders created";
    ASSERT_FALSE(has_contact_normal(world, body, {0.0f, 1.0f})) << "airborne and invalid bodies are not grounded";
    EXPECT_TRUE(world.contacts_of({}).empty()) << "airborne and invalid bodies are not grounded";
    for (int i{}; i < 120; ++i)
    {
        world.step(1.0f / 60.0f);
    }
    ASSERT_TRUE(has_contact_normal(world, body, {0.0f, 1.0f}));
    const auto points{world.contacts_of(body)};
    ASSERT_FALSE(points.empty());
    EXPECT_EQ(points.front().self.body, body);
    EXPECT_EQ(points.front().other.body, ground);
    EXPECT_NEAR(points.front().point.y, 0.5f, 0.1f);
    EXPECT_GE(points.front().normal_impulse, 0.0f);
    EXPECT_GE(points.front().total_normal_impulse, 0.0f);
    ASSERT_TRUE(world.set_velocity(body, {{0.0f, 5.0f}, 5.0f})) << "velocity can be set";
    for (int i{}; i < 10; ++i)
    {
        world.step(1.0f / 60.0f);
    }
    ASSERT_FALSE(has_contact_normal(world, body, {0.0f, 1.0f})) << "jump leaves the ground";
    EXPECT_NEAR(world.transform_of(body).value().angle_radians, 0.0f, 0.001f) << "locked body does not rotate";
    ASSERT_TRUE(world.destroy_body(body)) << "destroyed body is not grounded";
    EXPECT_TRUE(world.contacts_of(body).empty());

    nori::physics2d::scene wall_world;
    ASSERT_TRUE(wall_world.set_gravity({})) << "zero gravity is accepted";
    const auto wall{wall_world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto moving{wall_world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{-1.5f, 0.0f}, 0.0f}, .velocity = {{3.0f, 0.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(wall_world.add_box_collider(wall, {{0.5f, 5.0f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "wall colliders created";
    ASSERT_TRUE(wall_world.add_box_collider(moving, {{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "wall colliders created";
    for (int i{}; i < 60; ++i)
    {
        wall_world.step(1.0f / 60.0f);
    }
    EXPECT_TRUE(std::ranges::any_of(wall_world.contacts_of(moving), [](const auto& point) { return point.normal.x < -0.5f; }));
    EXPECT_FALSE(has_contact_normal(wall_world, moving, {0.0f, 1.0f}));
    nori::physics2d::scene ceiling_world;
    ASSERT_TRUE(ceiling_world.set_gravity({})) << "zero gravity is accepted for ceiling test";
    const auto ceiling{ceiling_world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto rising{ceiling_world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{0.0f, -1.5f}, 0.0f}, .velocity = {{0.0f, 3.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(ceiling_world.add_box_collider(ceiling, {{5.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "ceiling colliders created";
    ASSERT_TRUE(ceiling_world.add_box_collider(rising, {{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false})) << "ceiling colliders created";
    for (int i{}; i < 60; ++i)
    {
        ceiling_world.step(1.0f / 60.0f);
    }
    EXPECT_TRUE(std::ranges::any_of(ceiling_world.contacts_of(rising), [](const auto& point) { return point.normal.y < -0.5f; }));
    EXPECT_FALSE(has_contact_normal(ceiling_world, rising, {0.0f, 1.0f}));
    ASSERT_LT(ceiling_world.transform_of(rising).value().position.y, -0.95f) << "ceiling contact is not ground";
}

TEST(physics2d_scene_test, move_sleeping_contacts)
{
    nori::physics2d::scene world;
    ASSERT_TRUE(world.set_gravity({0.0f, -10.0f}));
    const auto ground{world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{0.0f, 2.0f}, 0.0f}})};
    ASSERT_TRUE(world.add_box_collider(ground, {{5.0f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}));
    ASSERT_TRUE(world.add_box_collider(body, {{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}));
    constexpr float step{1.0f / 60.0f};
    for (int i{}; i < 180; ++i)
    {
        world.step(step);
    }
    ASSERT_FALSE(world.contacts_of(body).empty());
    ASSERT_TRUE(world.set_transform(body, {{0.0f, 5.0f}, 0.0f}));
    world.step(step);
    EXPECT_TRUE(world.contacts_of(body).empty());
    EXPECT_LT(world.transform_of(body)->position.y, 5.0f);
    for (int i{}; i < 240; ++i)
    {
        world.step(step);
    }
    ASSERT_FALSE(world.contacts_of(body).empty());
    ASSERT_TRUE(world.set_transform(ground, {{0.0f, -10.0f}, 0.0f}));
    world.step(step);
    EXPECT_TRUE(world.contacts_of(body).empty());
    EXPECT_LT(world.velocity_of(body)->linear.y, 0.0f);
}

TEST(physics2d_scene_test, handle_lifetime)
{
    nori::physics2d::scene first;
    nori::physics2d::scene second;
    const nori::physics2d::body_handle original{first.create_body({})};
    const nori::physics2d::body_handle foreign{second.create_body({})};
    ASSERT_TRUE(first.contains(original)) << "both scenes create bodies";
    ASSERT_TRUE(second.contains(foreign)) << "both scenes create bodies";
    ASSERT_NE(original, foreign) << "first handles in different scenes differ";
    const nori::physics2d::collider_handle collider{original, 0};
    EXPECT_EQ(collider, (nori::physics2d::collider_handle{original, 0}));
    EXPECT_NE(collider, (nori::physics2d::collider_handle{original, 1}));
    EXPECT_NE(collider, (nori::physics2d::collider_handle{foreign, 0}));
    ASSERT_FALSE(first.contains(foreign)) << "foreign handle cannot delete its body";
    ASSERT_FALSE(first.destroy_body(foreign)) << "foreign handle cannot delete its body";
    ASSERT_TRUE(second.contains(foreign)) << "foreign handle cannot delete its body";
    ASSERT_FALSE(first.velocity_of(foreign)) << "foreign handle cannot access local body";
    ASSERT_FALSE(first.set_transform(foreign, {})) << "foreign handle cannot access local body";
    const nori::physics2d::body_handle invalid_slot{original.scene_id, {}};
    ASSERT_FALSE(first.contains(invalid_slot)) << "invalid public slot is rejected";
    ASSERT_FALSE(first.destroy_body(invalid_slot)) << "invalid public slot is rejected";

    ASSERT_TRUE(first.destroy_body(original)) << "first deletion succeeds";
    ASSERT_FALSE(first.destroy_body(original)) << "repeated deletion fails";
    const nori::physics2d::body_handle replacement{first.create_body({})};
    ASSERT_TRUE(first.contains(replacement)) << "replacement has a fresh handle";
    ASSERT_NE(replacement, original) << "replacement has a fresh handle";
    ASSERT_FALSE(first.transform_of(original)) << "expired handle cannot access replacement";
    ASSERT_FALSE(first.set_velocity(original, {})) << "expired handle cannot access replacement";
    ASSERT_FALSE(first.contains(nori::physics2d::body_handle{})) << "default handle is invalid";
    ASSERT_FALSE(first.apply_force({}, {})) << "default handle is invalid";
    ASSERT_EQ(first.create_body({nori::physics2d::body_type::dynamic_body, {{}, std::numeric_limits<float>::infinity()}, {}}), nori::physics2d::body_handle{}) << "nonfinite transform is rejected";
}
} // namespace
