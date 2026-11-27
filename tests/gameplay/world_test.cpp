#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/physics_body_component.hpp>
#include <nori/gameplay/game_instance/game_instance.hpp>
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

class update_probe_component final : public nori::core::rtti<update_probe_component, nori::gameplay::component>
{
public:
    update_probe_component(nori::gameplay::object& owner, std::vector<int>& calls) :
        rtti{owner},
        calls_{calls}
    {
    }

protected:
    void on_fixed_update(float) override
    {
        calls_.push_back(2);
    }

    void on_update(float) override
    {
        calls_.push_back(4);
    }

    void on_post_update(float) override
    {
        calls_.push_back(6);
    }

private:
    std::vector<int>& calls_;
};

class update_probe_object final : public nori::core::rtti<update_probe_object, nori::gameplay::object>
{
public:
    explicit update_probe_object(std::vector<int>& calls) :
        calls_{calls}
    {
        add_component<update_probe_component>(calls_);
    }

protected:
    void on_fixed_update(float) override
    {
        calls_.push_back(1);
    }

    void on_update(float) override
    {
        calls_.push_back(3);
        if (!added_)
        {
            added_ = true;
            add_component<update_probe_component>(calls_);
            world()->create_object<update_probe_object>(calls_);
        }
    }

    void on_post_update(float) override
    {
        calls_.push_back(5);
    }

private:
    std::vector<int>& calls_;
    bool added_{};
};

TEST(world_test, update_phases)
{
    std::vector<int> calls;
    nori::gameplay::world active;
    active.create_object<update_probe_object>(calls);
    active.create_object<update_probe_object>(calls);

    active.fixed_update(fixed_step);
    EXPECT_EQ(calls, (std::vector<int>{1, 2, 1, 2}));
    calls.clear();
    active.update(fixed_step);
    EXPECT_EQ(calls, (std::vector<int>{3, 4, 3, 4, 5, 6, 5, 6}));

    calls.clear();
    active.fixed_update(fixed_step);
    EXPECT_EQ(calls.size(), 10u);
    EXPECT_EQ(std::ranges::count(calls, 1), 4);
    EXPECT_EQ(std::ranges::count(calls, 2), 6);
}

TEST(world_test, gameplay_trigger_events)
{
    nori::gameplay::world active;
    active.physics_scene().set_gravity({});
    const auto sensor{active.create_object<test_physics_object>(nori::core::transform::identity,
        nori::gameplay::physics_body_config{nori::physics2d::body_type::static_body, {}, false, {}, {{{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true}, {{0.25f, 0.25f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true}}})};
    const auto visitor{active.create_object<test_physics_object>(nori::core::transform::identity,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::dynamic_body, .colliders = {{{0.25f, 0.25f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}}})};
    const nori::physics2d::box_collider_desc raw_shape[]{{{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true}};
    const auto raw_sensor{active.physics_scene().create_body({nori::physics2d::body_type::static_body, {}, {}, false, {}, raw_shape})};
    ASSERT_TRUE(active.physics_scene().contains(raw_sensor)) << "could not create a physics-only sensor";

    std::vector<nori::gameplay::trigger_changed> events;
    std::vector<std::size_t> late_events;
    unsigned once_count{};
    nori::gameplay::event_subscription once{};
    once = active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed&)
    {
        ++once_count;
        EXPECT_TRUE(active.unsubscribe(once));
        EXPECT_FALSE(active.unsubscribe(once));
    });
    static_cast<void>(active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed& event)
    {
        ASSERT_EQ((event.sensor), (sensor)) << "trigger was not mapped to its Gameplay owners";
        ASSERT_EQ((event.visitor), (visitor)) << "trigger was not mapped to its Gameplay owners";
        ASSERT_EQ((event.visitor_collider), (0)) << "trigger was not mapped to its Gameplay owners";
        events.push_back(event);
        if (event.phase == nori::gameplay::trigger_phase::begin)
        {
            if (events.size() == 1)
            {
                ASSERT_TRUE(active.destroy_object(event.visitor)) << "event callback could not remove the visitor";
                static_cast<void>(active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed& later)
                {
                    late_events.push_back(later.sensor_collider);
                }));
            }
            ASSERT_TRUE(active.find_object(event.visitor)) << "event callback did not defer object removal";
        }
    }));
    active.fixed_update(fixed_step);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].phase, nori::gameplay::trigger_phase::begin);
    EXPECT_EQ(events[1].phase, nori::gameplay::trigger_phase::begin);
    EXPECT_NE(events[0].sensor_collider, events[1].sensor_collider);
    EXPECT_EQ(late_events, (std::vector{events[1].sensor_collider}));
    EXPECT_TRUE(active.find_object(visitor));
    active.end_frame();
    EXPECT_FALSE(active.find_object(visitor));
    EXPECT_EQ(once_count, 1u);
    EXPECT_FALSE(active.unsubscribe(once));
    active.fixed_update(0.0f);
    ASSERT_EQ((events.size()), (2)) << "invalid fixed step replayed the previous physics events";
    active.fixed_update(fixed_step);
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[2].phase, nori::gameplay::trigger_phase::end);
    EXPECT_EQ(events[3].phase, nori::gameplay::trigger_phase::end);
    EXPECT_NE(events[2].sensor_collider, events[3].sensor_collider);
    EXPECT_EQ(late_events, (std::vector{events[1].sensor_collider, events[2].sensor_collider, events[3].sensor_collider}));
    active.fixed_update(fixed_step);
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(once_count, 1u);
}

TEST(world_test, trigger_subscription_handles)
{
    nori::gameplay::world first;
    nori::gameplay::world second;
    const auto original{first.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&)
    {
    })};
    const auto foreign{second.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&)
    {
    })};
    const auto object{first.create_object<nori::gameplay::object>()};
    EXPECT_EQ(object.world_id, original.world_id);
    EXPECT_FALSE(first.unsubscribe(foreign));
    EXPECT_TRUE(first.unsubscribe(original));
    EXPECT_FALSE(first.unsubscribe(original));
    const auto replacement{first.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&)
    {
    })};
    EXPECT_FALSE(first.unsubscribe(original));
    EXPECT_TRUE(first.unsubscribe(replacement));
    EXPECT_TRUE(second.unsubscribe(foreign));
}
TEST(world_test, trigger_can_request_a_world_change)
{
    struct initializer
    {
        initializer(nori::gameplay::world& active, float x)
        {
            auto pose{nori::core::transform::identity};
            pose.position.x = x;
            active.create_object<nori::gameplay::object>(pose);
        }
    };

    nori::gameplay::game_instance game;
    auto& active{game.create_world()};
    const auto sensor{active.create_object<test_physics_object>(nori::core::transform::identity,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::static_body, .colliders = {{{0.5f, 0.5f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, true}}})};
    const auto visitor{active.create_object<test_physics_object>(nori::core::transform::identity,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::dynamic_body, .colliders = {{{0.25f, 0.25f}, {}, 0.6f, 0.0f, 1.0f, nori::physics2d::default_collision_filter, false}}})};
    bool requested{};
    bool observed{};
    static_cast<void>(active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed& event)
    {
        EXPECT_EQ(event.sensor, sensor);
        game.request_world_change<initializer>(3.0f);
        requested = true;
    }));
    static_cast<void>(active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed&)
    {
        EXPECT_TRUE(requested);
        EXPECT_EQ(game.world(), &active);
        EXPECT_NE(game.world()->find_object(visitor), nullptr);
        observed = true;
    }));

    game.fixed_update(fixed_step);

    EXPECT_TRUE(observed);
    EXPECT_EQ(game.world(), &active);
    game.end_frame();
    const auto next{game.world()};
    ASSERT_NE(next, nullptr);
    EXPECT_EQ(next->find_object(visitor), nullptr);
    const auto objects{next->objects()};
    ASSERT_EQ(objects.size(), 1u);
    EXPECT_FLOAT_EQ(next->find_object(objects.front())->root_component().world_transform().position.x, 3.0f);
}
} // namespace
