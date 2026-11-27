#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/gameplay/game_instance/game_instance.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace
{
constexpr float fixed_step{1.0f / 60.0f};

class tracked_object final : public nori::core::rtti<tracked_object, nori::gameplay::object>
{
public:
    explicit tracked_object(int& destroyed) :
        destroyed_{destroyed}
    {
    }

    ~tracked_object() override
    {
        ++destroyed_;
    }

private:
    int& destroyed_;
};

class tracked_component final : public nori::core::rtti<tracked_component, nori::gameplay::component>
{
public:
    tracked_component(nori::gameplay::object& owner, int& destroyed) :
        rtti{owner},
        destroyed_{destroyed}
    {
    }

    ~tracked_component() override
    {
        ++destroyed_;
    }

private:
    int& destroyed_;
};

class unique_component final : public nori::core::rtti<unique_component, nori::gameplay::component>
{
public:
    static constexpr bool is_unique{true};

    unique_component(nori::gameplay::object& owner, int& constructed) :
        rtti{owner}
    {
        ++constructed;
    }
};

TEST(object_test, world_access)
{
    nori::gameplay::object detached;
    static_assert(std::same_as<decltype(detached.world()), nori::gameplay::world*>);
    static_assert(std::same_as<decltype(std::as_const(detached).world()), const nori::gameplay::world*>);
    EXPECT_EQ(detached.world(), nullptr);
    EXPECT_EQ(std::as_const(detached).world(), nullptr);

    nori::gameplay::world active;
    auto& attached{*active.find_object(active.create_object<nori::gameplay::object>())};
    EXPECT_EQ(attached.world(), &active);
    EXPECT_EQ(std::as_const(attached).world(), &active);
}

TEST(object_test, root_component_access)
{
    nori::gameplay::object value;
    static_assert(std::same_as<decltype(value.root_component()), nori::gameplay::scene_component&>);
    static_assert(std::same_as<decltype(std::as_const(value).root_component()), const nori::gameplay::scene_component&>);
    EXPECT_EQ(&value.root_component(), &std::as_const(value).root_component());
}

TEST(object_test, component_multiplicity)
{
    int destroyed{};
    nori::gameplay::object value;
    EXPECT_NE(value.add_component<tracked_component>(destroyed), nullptr);
    EXPECT_NE(value.add_component<tracked_component>(destroyed), nullptr);

    int constructed{};
    EXPECT_NE(value.add_component<unique_component>(constructed), nullptr);
    EXPECT_EQ(value.add_component<unique_component>(constructed), nullptr);
    EXPECT_EQ(constructed, 1);
}

class frame_component final : public nori::core::rtti<frame_component, nori::gameplay::component>
{
public:
    frame_component(nori::gameplay::object& owner, int& fixed, int& variable, int& destroyed) :
        rtti{owner}, fixed_{fixed}, variable_{variable}, destroyed_{destroyed}
    {
    }

    ~frame_component() override
    {
        ++destroyed_;
    }

protected:
    void on_register() override
    {
        EXPECT_FALSE(owner().remove_component(*this));
    }

    void on_fixed_update(float) override
    {
        if (++fixed_ == 1)
        {
            EXPECT_TRUE(owner().remove_component(*this));
            EXPECT_FALSE(owner().remove_component(*this));
        }
    }

    void on_update(float) override
    {
        ++variable_;
    }

private:
    int& fixed_;
    int& variable_;
    int& destroyed_;
};

TEST(object_test, component_removal_waits_for_frame_end)
{
    int fixed{};
    int variable{};
    int destroyed{};
    nori::gameplay::world active;
    auto& value{*active.find_object(active.create_object<nori::gameplay::object>())};
    const auto comp{value.add_component<frame_component>(fixed, variable, destroyed)};
    active.fixed_update(fixed_step);
    active.fixed_update(fixed_step);
    active.update(fixed_step);
    EXPECT_EQ(fixed, 2);
    EXPECT_EQ(variable, 1);
    EXPECT_EQ(value.find_component<frame_component>(), comp);
    EXPECT_EQ(destroyed, 0);
    active.end_frame();
    EXPECT_EQ(value.find_component<frame_component>(), nullptr);
    EXPECT_EQ(destroyed, 1);
    active.end_frame();
    EXPECT_EQ(destroyed, 1);
}

TEST(object_test, object_lifecycle)
{
    int destroyed{};
    int components_destroyed{};
    nori::gameplay::world active;
    nori::gameplay::world foreign;
    const auto first{active.create_object<tracked_object>(destroyed)};
    auto* value{active.find_object(first)};
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(foreign.find_object(first), nullptr);
    EXPECT_EQ(active.find_object({}), nullptr);
    auto& comp{*value->add_component<tracked_component>(components_destroyed)};
    ASSERT_EQ((&comp.owner()), (value)) << "component owner or registration was not assigned";
    ASSERT_TRUE(comp.is_registered()) << "component owner or registration was not assigned";
    for (int i{}; i < 128; ++i)
    {
        active.create_object<nori::gameplay::object>();
    }
    ASSERT_EQ((active.find_object(first)), (value)) << "object address changed after insertion";
    ASSERT_TRUE(value->remove_component(comp)) << "component removal was not deferred";
    ASSERT_TRUE(comp.is_registered()) << "component removal was not deferred";
    ASSERT_TRUE(value->find_component<tracked_component>()) << "component removal was not deferred";
    ASSERT_EQ((components_destroyed), (0)) << "component removal was not deferred";
    active.end_frame();
    ASSERT_EQ((components_destroyed), (1)) << "component was not destroyed at the boundary";
    value->add_component<tracked_component>(components_destroyed);
    ASSERT_TRUE(active.destroy_object(first));
    EXPECT_EQ(active.find_object(first), value);
    EXPECT_FALSE(active.destroy_object(first));
    EXPECT_EQ(destroyed, 0);
    EXPECT_TRUE(std::ranges::contains(active.objects(), first));
    active.end_frame();
    ASSERT_EQ((destroyed), (1)) << "object or owned component leaked";
    ASSERT_EQ((components_destroyed), (2)) << "object or owned component leaked";
    const auto next{active.create_object<tracked_object>(destroyed)};
    ASSERT_EQ((next.slot.index), (first.slot.index)) << "slot reuse accepted a stale handle";
    ASSERT_NE(next.slot.generation, first.slot.generation) << "slot reuse accepted a stale handle";
    ASSERT_FALSE(active.find_object(first)) << "slot reuse accepted a stale handle";
    ASSERT_FALSE(active.destroy_object(first)) << "stale handle removed a live object";
    ASSERT_TRUE(active.find_object(next)) << "stale handle removed a live object";
}

class lifecycle_update_component final : public nori::core::rtti<lifecycle_update_component, nori::gameplay::component>
{
public:
    lifecycle_update_component(nori::gameplay::object& owner, std::vector<int>& calls, bool remove_self) :
        rtti{owner},
        calls_{calls},
        remove_self_{remove_self}
    {
    }

protected:
    void on_register() override
    {
        calls_.push_back(10);
    }

    void on_unregister() override
    {
        calls_.push_back(20);
    }

    void on_fixed_update(float) override
    {
        calls_.push_back(1);
        if (remove_self_)
        {
            ASSERT_TRUE(owner().remove_component(*this)) << "component disappeared inside snapshot";
            ASSERT_EQ((owner().find_component<lifecycle_update_component>()), (this)) << "component disappeared inside snapshot";
            ASSERT_TRUE(is_registered()) << "component disappeared inside snapshot";
        }
    }

    void on_post_update(float) override
    {
        calls_.push_back(2);
    }

private:
    std::vector<int>& calls_;
    bool remove_self_;
};

class lifecycle_update_object final : public nori::core::rtti<lifecycle_update_object, nori::gameplay::object>
{
public:
    explicit lifecycle_update_object(std::vector<int>& calls) :
        calls_{calls}
    {
        add_component<lifecycle_update_component>(calls_, true);
    }

protected:
    void on_fixed_update(float) override
    {
        calls_.push_back(3);
    }

    void on_post_update(float) override
    {
        calls_.push_back(4);
    }

private:
    std::vector<int>& calls_;
};

TEST(object_test, update_lifecycle)
{
    std::vector<int> calls;
    nori::gameplay::world active;
    const auto handle{active.create_object<lifecycle_update_object>(calls)};
    auto* object{active.find_object(handle)};
    ASSERT_EQ((calls), (std::vector<int>{10})) << "initial component not registered";
    calls.clear();
    active.fixed_update(fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{3, 1}));
    active.update(fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{3, 1, 4, 2}));
    active.end_frame();
    ASSERT_EQ((calls), (std::vector<int>{3, 1, 4, 2, 20})) << "component removal did not wait until cycle boundary";
    ASSERT_FALSE(object->find_component<lifecycle_update_component>()) << "component removal did not wait until cycle boundary";
    auto& added{*object->add_component<lifecycle_update_component>(calls, false)};
    ASSERT_EQ((&added.owner()), (object)) << "dynamic component registration failed";
    ASSERT_EQ((calls.back()), (10)) << "dynamic component registration failed";
    calls.clear();
    active.fixed_update(fixed_step);
    active.update(fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{3, 1, 4, 2})) << "component multi-phase update incorrect";
    calls.clear();
    ASSERT_TRUE(active.destroy_object(handle));
    EXPECT_TRUE(calls.empty());
    active.end_frame();
    ASSERT_EQ((calls), (std::vector<int>{20})) << "object removal did not unregister components";
    calls.clear();
    active.fixed_update(fixed_step);
    ASSERT_TRUE(calls.empty()) << "removed object's update executed";
}

class variable_removal_object final : public nori::core::rtti<variable_removal_object, nori::gameplay::object>
{
public:
    explicit variable_removal_object(std::vector<int>& calls) :
        calls_{calls}
    {
    }

protected:
    void on_unregister() override
    {
        calls_.push_back(30);
    }

    void on_fixed_update(float delta_seconds) override
    {
        EXPECT_NEAR(delta_seconds, fixed_step, 0.0001f) << "fixed delta seconds incorrect";
        calls_.push_back(7);
    }

    void on_update(float delta_seconds) override
    {
        EXPECT_NEAR(delta_seconds, 0.005f, 0.0001f) << "variable delta seconds incorrect";
        calls_.push_back(5);
        ASSERT_TRUE(world()->destroy_object(handle())) << "object removal was not deferred";
        ASSERT_EQ((world()->find_object(handle())), (this)) << "object removal was not deferred";
        ASSERT_FALSE(world()->destroy_object(handle())) << "duplicate object destruction accepted";
    }

    void on_post_update(float) override
    {
        ASSERT_EQ((world()->find_object(handle())), (this)) << "object detached during snapshot";
        calls_.push_back(6);
    }

private:
    std::vector<int>& calls_;
};

TEST(object_test, variable_update_lifecycle)
{
    std::vector<int> calls;
    nori::gameplay::game_instance game;
    auto& active{game.create_world()};
    const auto handle{active.create_object<variable_removal_object>(calls)};
    game.fixed_update(fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{7})) << "fixed update invoked variable callbacks";
    calls.clear();
    game.update(0.005f);
    EXPECT_EQ(calls, (std::vector<int>{5, 6}));
    EXPECT_NE(active.find_object(handle), nullptr);
    game.fixed_update(fixed_step);
    EXPECT_EQ(calls, (std::vector<int>{5, 6, 7}));
    game.end_frame();
    EXPECT_EQ(calls, (std::vector<int>{5, 6, 7, 30}));
    EXPECT_EQ(active.find_object(handle), nullptr);
    calls.clear();
    game.fixed_update(fixed_step);
    game.update(0.005f);
    ASSERT_TRUE(calls.empty()) << "removed object's callback executed in another category";
}

class removal_callback_component final : public nori::core::rtti<removal_callback_component, nori::gameplay::component>
{
public:
    removal_callback_component(nori::gameplay::object& owner, std::move_only_function<void()> callback, bool remove_self) :
        rtti{owner},
        callback_{std::move(callback)},
        remove_self_{remove_self}
    {
    }

protected:
    void on_unregister() override
    {
        callback_();
    }

    void on_fixed_update(float) override
    {
        if (remove_self_)
        {
            EXPECT_TRUE(owner().remove_component(*this));
        }
    }

private:
    std::move_only_function<void()> callback_;
    bool remove_self_;
};

TEST(object_test, cascading_removals)
{
    int destroyed{};
    nori::gameplay::world active;
    active.create_object<nori::gameplay::object>();
    active.create_object<nori::gameplay::object>();
    const auto handles{active.objects()};
    ASSERT_EQ(handles.size(), 2u);
    auto& target{*active.find_object(handles.front())};
    auto& source{*active.find_object(handles.back())};
    const auto victim{active.create_object<tracked_object>(destroyed)};
    auto& target_component{*target.add_component<removal_callback_component>([&]
    {
        EXPECT_TRUE(active.destroy_object(victim));
    }, false)};
    source.add_component<removal_callback_component>([&]
    {
        EXPECT_TRUE(target.remove_component(target_component));
    }, true);
    active.fixed_update(fixed_step);
    active.end_frame();
    EXPECT_EQ(source.find_component<removal_callback_component>(), nullptr);
    EXPECT_EQ(target.find_component<removal_callback_component>(), nullptr);
    EXPECT_EQ(active.find_object(victim), nullptr);
    EXPECT_EQ(destroyed, 1);
}

TEST(object_test, duplicate_cascading_destruction)
{
    int destroyed{};
    nori::gameplay::world active;
    const auto source{active.create_object<tracked_object>(destroyed)};
    const auto target{active.create_object<tracked_object>(destroyed)};
    active.find_object(source)->add_component<removal_callback_component>([&]
    {
        EXPECT_FALSE(active.destroy_object(target));
    }, false);
    EXPECT_TRUE(active.destroy_object(source));
    EXPECT_TRUE(active.destroy_object(target));
    active.end_frame();
    EXPECT_TRUE(active.objects().empty());
    EXPECT_EQ(destroyed, 2);
}

TEST(object_test, cascading_object_removals_at_frame_end)
{
    std::vector<int> calls;
    nori::gameplay::world active;
    const auto source{active.create_object<nori::gameplay::object>()};
    const auto target{active.create_object<variable_removal_object>(calls)};
    active.find_object(source)->add_component<removal_callback_component>([&]
    {
        EXPECT_TRUE(active.destroy_object(target));
    }, false);
    ASSERT_TRUE(active.destroy_object(source));
    active.fixed_update(fixed_step);
    active.fixed_update(fixed_step);
    EXPECT_NE(active.find_object(source), nullptr);
    EXPECT_NE(active.find_object(target), nullptr);
    EXPECT_EQ(calls, (std::vector<int>{7, 7}));
    active.end_frame();
    EXPECT_TRUE(active.objects().empty());
    EXPECT_EQ(calls, (std::vector<int>{7, 7, 30}));
}

class shutdown_creation_object final : public nori::core::rtti<shutdown_creation_object, nori::gameplay::object>
{
protected:
    void on_unregister() override
    {
        world()->create_object<nori::gameplay::object>();
    }
};

TEST(object_test, creation_during_world_shutdown)
{
    EXPECT_DEATH_IF_SUPPORTED(
        {
            nori::gameplay::world active;
            active.create_object<shutdown_creation_object>();
        },
        "cannot create an object while clearing a world"
    );
}
} // namespace
