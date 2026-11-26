#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include <nori/physics2d/scene.hpp>

namespace
{
bool is_near(float value, float expected)
{
    return std::abs(value - expected) < 0.001f;
}

TEST(physics2d_scene, body_motion)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle ground{world.create_body({nori::physics2d::body_type::static_body, {{}, 0.0f}, {}})};
    const nori::physics2d::body_handle platform{world.create_body({nori::physics2d::body_type::kinematic_body, {{0.0f, 3.0f}, 0.0f}, {{6.0f, 0.0f}, 0.0f}})};
    const nori::physics2d::body_handle falling{world.create_body({nori::physics2d::body_type::dynamic_body, {{0.0f, 3.0f}, 0.0f}, {}})};
    ASSERT_TRUE(world.contains(ground) && world.contains(platform) && world.contains(falling)) << "all body types can be created";

    ASSERT_TRUE(world.set_transform(ground, {{2.0f, 1.0f}, 0.5f})) << "static body accepts a transform";
    ASSERT_TRUE(!world.set_velocity(ground, {{6.0f, 0.0f}, 0.0f})) << "static body rejects velocity";
    ASSERT_TRUE(!world.apply_force(platform, {60.0f, 0.0f})) << "kinematic body rejects force";
    ASSERT_TRUE(!world.apply_linear_impulse(ground, {2.0f, 0.0f})) << "static body rejects impulse";
    ASSERT_TRUE(world.apply_linear_impulse(falling, {2.0f, 0.0f})) << "dynamic body accepts impulse";
    ASSERT_TRUE(is_near(world.get_velocity(falling).value().linear.x, 2.0f)) << "impulse changes velocity immediately";
    ASSERT_TRUE(world.apply_force(falling, {60.0f, 0.0f})) << "dynamic body accepts force";

    world.step(1.0f / 60.0f);

    const nori::physics2d::body_transform ground_pose{world.get_transform(ground).value()};
    ASSERT_TRUE(is_near(ground_pose.position.x, 2.0f) && is_near(ground_pose.position.y, 1.0f) && is_near(ground_pose.angle_radians, 0.5f)) << "static body stays at its assigned transform";
    const nori::physics2d::body_transform platform_pose{world.get_transform(platform).value()};
    ASSERT_TRUE(is_near(platform_pose.position.x, 0.1f) && is_near(platform_pose.position.y, 3.0f)) << "kinematic body advances one 60 Hz tick";
    ASSERT_TRUE(world.get_transform(falling).value().position.y < 3.0f) << "gravity lowers the shapeless dynamic body";
    ASSERT_TRUE(is_near(world.get_velocity(falling).value().linear.x, 3.0f)) << "force changes velocity over one tick";

    ASSERT_TRUE(world.set_velocity(platform, {{-3.0f, 0.0f}, 3.0f})) << "kinematic body accepts linear and angular velocity";
    world.step(1.0f / 60.0f);
    const nori::physics2d::body_transform updated_pose{world.get_transform(platform).value()};
    ASSERT_TRUE(is_near(updated_pose.position.x, 0.05f) && is_near(updated_pose.angle_radians, 0.05f)) << "updated velocity changes position and angle";
}

TEST(physics2d_scene, step_interval)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle body{world.create_body({nori::physics2d::body_type::kinematic_body, {{}, 0.0f}, {{6.0f, 0.0f}, 0.0f}})};
    world.step(1.0f / 30.0f);
    ASSERT_TRUE(is_near(world.get_transform(body).value().position.x, 0.2f)) << "step advances by the supplied interval";

    world.step(0.0f);
    world.step(-1.0f);
    world.step(std::numeric_limits<float>::infinity());
    world.step(std::numeric_limits<float>::quiet_NaN());
    ASSERT_TRUE(is_near(world.get_transform(body).value().position.x, 0.2f)) << "invalid intervals leave the world unchanged";
}

TEST(physics2d_scene, box_collision)
{
    nori::physics2d::scene world;
    const nori::physics2d::body_handle ground{world.create_body({nori::physics2d::body_type::static_body, {{}, 0.0f}, {}})};
    const nori::physics2d::body_handle falling{world.create_body({nori::physics2d::body_type::dynamic_body, {{0.0f, 4.0f}, 0.0f}, {}})};
    ASSERT_TRUE(world.add_box_collider({ground, {5.0f, 0.5f}})) << "ground collider can be created";
    ASSERT_TRUE(world.add_box_collider({falling, {0.5f, 0.5f}})) << "dynamic collider can be created";
    int begins{};
    for (int i{}; i < 180; ++i)
    {
        world.step(1.0f / 60.0f);
        for (const auto& event : world.contact_events())
        {
            begins += event.kind == nori::physics2d::contact_kind::contact && event.phase == nori::physics2d::contact_phase::begin;
        }
    }
    const nori::physics2d::body_transform pose{world.get_transform(falling).value()};
    const nori::physics2d::body_velocity velocity{world.get_velocity(falling).value()};
    ASSERT_TRUE(std::abs(pose.position.y - 1.0f) < 0.05f) << "dynamic box rests on ground";
    ASSERT_TRUE(std::abs(velocity.linear.y) < 0.05f) << "dynamic box stops on ground";
    ASSERT_TRUE(begins == 1) << "collision did not begin exactly once";
}

TEST(physics2d_scene, collider_configuration)
{
    nori::physics2d::scene world;
    const auto with_override{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .mass_override = 8.0f})};
    auto settings{nori::physics2d::default_box_collider_settings};
    settings.density = 2.0f;
    settings.filter = {2, 4};
    ASSERT_TRUE(world.add_box_collider({with_override, {0.5f, 0.5f}, {2.0f, 0.0f}, settings})) << "configured collider rejected";
    ASSERT_TRUE(world.apply_linear_impulse(with_override, {8.0f, 0.0f}) && is_near(world.get_velocity(with_override)->linear.x, 1.0f)) << "adding a collider changed the explicit mass";
    const auto hit{world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {4, 2})};
    ASSERT_TRUE(hit && hit->collider.body == with_override && hit->collider.index == 0 && is_near(hit->point.y, 0.5f) && is_near(hit->normal.y, 1.0f) && is_near(hit->fraction, 0.375f)) << "ray missed the offset collider";
    ASSERT_TRUE(!world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {1, 2})) << "ray ignored the collider mask";
    ASSERT_TRUE(!world.cast_ray({2.0f, 2.0f}, {0.0f, -4.0f}, {4, 1})) << "ray ignored the collider category";

    const auto density_body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{8.0f, 0.0f}, 0.0f}})};
    ASSERT_TRUE(world.add_box_collider({density_body, {0.5f, 0.5f}, {}, settings})) << "density collider rejected";
    ASSERT_TRUE(world.apply_linear_impulse(density_body, {2.0f, 0.0f}) && is_near(world.get_velocity(density_body)->linear.x, 1.0f)) << "density does not determine dynamic mass";

    settings.is_sensor = true;
    const auto sensor_body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{10.0f, 0.0f}, 0.0f}})};
    ASSERT_TRUE(world.add_box_collider({sensor_body, {0.5f, 0.5f}, {}, settings}) &&
            world.apply_linear_impulse(sensor_body, {1.0f, 0.0f}) &&
            is_near(world.get_velocity(sensor_body)->linear.x, 1.0f)) << "sensor-only body lost its default dynamic mass";

    const nori::physics2d::box_collider_config boxes[]{{{0.5f, 0.5f}, {}, {}}, {{}, {}, {}}};
    ASSERT_TRUE(!world.contains(world.create_body({.type = nori::physics2d::body_type::static_body, .transform = {{12.0f, 0.0f}, 0.0f}}, boxes)) &&
            !world.cast_ray({12.0f, 2.0f}, {0.0f, -4.0f})) << "failed body creation left a collider in the scene";
}

TEST(physics2d_scene, triggers)
{
    nori::physics2d::scene world;
    ASSERT_TRUE(world.set_gravity({})) << "could not disable gravity";
    auto sensor_settings{nori::physics2d::default_box_collider_settings};
    sensor_settings.is_sensor = true;
    const nori::physics2d::box_collider_config boxes[]{
        {{0.3f, 1.0f}, {-0.65f, 0.0f}, sensor_settings},
        {{0.3f, 1.0f}, {0.65f, 0.0f}, sensor_settings}
    };
    const auto sensor{world.create_body({.type = nori::physics2d::body_type::static_body}, boxes)};
    const auto visitor{world.create_body({.type = nori::physics2d::body_type::dynamic_body})};
    ASSERT_TRUE(world.contains(sensor) && world.add_box_collider({visitor, {0.75f, 0.5f}})) << "trigger setup failed";
    constexpr float step{1.0f / 60.0f};
    world.step(step);
    const auto begins{world.contact_events()};
    ASSERT_TRUE(begins.size() == 2 && begins[0].first.index != begins[1].first.index) << "two colliders did not begin independently";
    for (const auto& event : begins)
    {
        ASSERT_TRUE(event.kind == nori::physics2d::contact_kind::trigger && event.phase == nori::physics2d::contact_phase::begin && event.first.body == sensor && event.second.body == visitor) << "invalid trigger begin data";
    }
    ASSERT_TRUE(!world.is_grounded(visitor) && is_near(world.get_transform(visitor)->position.x, 0.0f)) << "sensor affected physical motion";
    ASSERT_TRUE(world.set_transform(visitor, {{0.8f, 0.0f}, 0.0f})) << "could not leave one sensor";
    world.step(step);
    const auto first_end{world.contact_events()};
    ASSERT_TRUE(first_end.size() == 1 && first_end.front().phase == nori::physics2d::contact_phase::end && first_end.front().first.index == 0) << "leaving one collider ended the wrong trigger";

    ASSERT_TRUE(world.destroy_body(visitor)) << "could not delete the visitor";
    world.step(0.0f);
    ASSERT_TRUE(world.contact_events().front().first.index == 0) << "invalid step replaced the event batch";
    world.step(step);
    const auto removal_end{world.contact_events()};
    ASSERT_TRUE(removal_end.size() == 1 && removal_end.front().phase == nori::physics2d::contact_phase::end && removal_end.front().first.index == 1 && removal_end.front().second.body == visitor) << "destroying the visitor did not end its remaining trigger once";
    world.step(step);
    ASSERT_TRUE(world.contact_events().empty()) << "removed visitor continued to produce events";
}

TEST(physics2d_scene, box_collider_validation)
{
    nori::physics2d::scene world;
    nori::physics2d::scene other;
    const nori::physics2d::body_handle body{world.create_body({nori::physics2d::body_type::dynamic_body, {}, {}})};
    const nori::physics2d::body_handle foreign{other.create_body({})};
    ASSERT_TRUE(!world.add_box_collider({{}, {1.0f, 1.0f}})) << "default handle is rejected";
    ASSERT_TRUE(!world.add_box_collider({foreign, {1.0f, 1.0f}})) << "foreign handle is rejected";
    ASSERT_TRUE(!world.add_box_collider({body, {0.0f, 1.0f}})) << "zero extent is rejected";
    ASSERT_TRUE(!world.add_box_collider({body, {1.0f, -1.0f}})) << "negative extent is rejected";
    ASSERT_TRUE(!world.add_box_collider({body, {std::numeric_limits<float>::infinity(), 1.0f}})) << "infinite extent is rejected";
    ASSERT_TRUE(!world.add_box_collider({body, {1.0f, std::numeric_limits<float>::quiet_NaN()}})) << "NaN extent is rejected";
    ASSERT_TRUE(world.add_box_collider({body, {1.0f, 0.5f}})) << "first collider can be added";
    ASSERT_TRUE(world.add_box_collider({body, {1.0f, 0.5f}})) << "multiple colliders can be added";
    ASSERT_TRUE(world.apply_linear_impulse(body, {4.0f, 0.0f})) << "impulse applies to body with colliders";
    ASSERT_TRUE(is_near(world.get_velocity(body).value().linear.x, 1.0f)) << "collider areas determine dynamic mass";
    ASSERT_TRUE(world.destroy_body(body)) << "body with colliders can be destroyed";
    ASSERT_TRUE(!world.add_box_collider({body, {1.0f, 1.0f}})) << "destroyed handle is rejected";
}

TEST(physics2d_scene, grounding_and_rotation)
{
    nori::physics2d::scene world;
    ASSERT_TRUE(world.set_gravity({0.0f, -20.0f})) << "gravity can be configured";
    ASSERT_TRUE(!world.set_gravity({0.0f, std::numeric_limits<float>::infinity()})) << "invalid gravity is rejected";
    const auto ground{world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto body{world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{0.0f, 2.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(world.add_box_collider({ground, {5.0f, 0.5f}}) && world.add_box_collider({body, {0.5f, 0.5f}})) << "colliders created";
    ASSERT_TRUE(!world.is_grounded(body) && !world.is_grounded({})) << "airborne and invalid bodies are not grounded";
    for (int i{}; i < 120; ++i)
    {
        world.step(1.0f / 60.0f);
    }
    ASSERT_TRUE(world.is_grounded(body)) << "ground contact is detected";
    ASSERT_TRUE(world.set_velocity(body, {{0.0f, 5.0f}, 5.0f})) << "velocity can be set";
    for (int i{}; i < 10; ++i)
    {
        world.step(1.0f / 60.0f);
    }
    ASSERT_TRUE(!world.is_grounded(body)) << "jump leaves the ground";
    ASSERT_TRUE(is_near(world.get_transform(body)->angle_radians, 0.0f)) << "locked body does not rotate";
    ASSERT_TRUE(world.destroy_body(body) && !world.is_grounded(body)) << "destroyed body is not grounded";

    nori::physics2d::scene wall_world;
    ASSERT_TRUE(wall_world.set_gravity({})) << "zero gravity is accepted";
    const auto wall{wall_world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto moving{wall_world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{-1.5f, 0.0f}, 0.0f}, .velocity = {{3.0f, 0.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(wall_world.add_box_collider({wall, {0.5f, 5.0f}}) && wall_world.add_box_collider({moving, {0.5f, 0.5f}})) << "wall colliders created";
    for (int i{}; i < 60; ++i)
    {
        wall_world.step(1.0f / 60.0f);
    }
    ASSERT_TRUE(!wall_world.is_grounded(moving)) << "wall contact is not ground";
    nori::physics2d::scene ceiling_world;
    ASSERT_TRUE(ceiling_world.set_gravity({})) << "zero gravity is accepted for ceiling test";
    const auto ceiling{ceiling_world.create_body({.type = nori::physics2d::body_type::static_body})};
    const auto rising{ceiling_world.create_body({.type = nori::physics2d::body_type::dynamic_body, .transform = {{0.0f, -1.5f}, 0.0f}, .velocity = {{0.0f, 3.0f}, 0.0f}, .lock_rotation = true})};
    ASSERT_TRUE(ceiling_world.add_box_collider({ceiling, {5.0f, 0.5f}}) && ceiling_world.add_box_collider({rising, {0.5f, 0.5f}})) << "ceiling colliders created";
    for (int i{}; i < 60; ++i)
    {
        ceiling_world.step(1.0f / 60.0f);
    }
    ASSERT_TRUE(!ceiling_world.is_grounded(rising) && ceiling_world.get_transform(rising)->position.y < -0.95f) << "ceiling contact is not ground";
}

TEST(physics2d_scene, handle_lifetime)
{
    nori::physics2d::scene first;
    nori::physics2d::scene second;
    const nori::physics2d::body_handle original{first.create_body({})};
    const nori::physics2d::body_handle foreign{second.create_body({})};
    ASSERT_TRUE(first.contains(original) && second.contains(foreign)) << "both scenes create bodies";
    ASSERT_TRUE(original != foreign) << "first handles in different scenes differ";
    ASSERT_TRUE(!first.contains(foreign) && !first.destroy_body(foreign) && second.contains(foreign)) << "foreign handle cannot delete its body";
    ASSERT_TRUE(!first.get_velocity(foreign) && !first.set_transform(foreign, {})) << "foreign handle cannot access local body";
    const nori::physics2d::body_handle invalid_slot{original.scene_id, {}};
    ASSERT_TRUE(!first.contains(invalid_slot) && !first.destroy_body(invalid_slot)) << "invalid public slot is rejected";

    ASSERT_TRUE(first.destroy_body(original)) << "first deletion succeeds";
    ASSERT_TRUE(!first.destroy_body(original)) << "repeated deletion fails";
    const nori::physics2d::body_handle replacement{first.create_body({})};
    ASSERT_TRUE(first.contains(replacement) && replacement != original) << "replacement has a fresh handle";
    ASSERT_TRUE(!first.get_transform(original) && !first.set_velocity(original, {})) << "expired handle cannot access replacement";
    ASSERT_TRUE(!first.contains(nori::physics2d::body_handle{}) && !first.apply_force({}, {})) << "default handle is invalid";
    ASSERT_TRUE(first.create_body({nori::physics2d::body_type::dynamic_body, {{}, std::numeric_limits<float>::infinity()}, {}}) == nori::physics2d::body_handle{}) << "nonfinite transform is rejected";
}
} // namespace
