#include <concepts>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace
{
constexpr float fixed_step{1.0f / 60.0f};

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

class removal_observer final : public nori::core::rtti<removal_observer, nori::gameplay::component>
{
public:
    removal_observer(nori::gameplay::object& owner, nori::gameplay::component& target) :
        rtti{owner},
        target_{&target}
    {
    }

    [[nodiscard]] const nori::gameplay::component* target() const
    {
        return target_;
    }

    [[nodiscard]] int notifications() const
    {
        return notifications_;
    }

protected:
    void on_component_removed(const nori::gameplay::component& value) noexcept override
    {
        if (target_ == &value)
        {
            ASSERT_EQ((&value.owner()), (&owner())) << "component was destroyed or still active at notification";
            ASSERT_FALSE((owner().contains_component(value))) << "component was destroyed or still active at notification";
            ASSERT_FALSE((owner().remove_component(*this))) << "notification allowed recursive removal";
            target_ = nullptr;
            ++notifications_;
        }
    }

private:
    const nori::gameplay::component* target_;
    int notifications_{};
};

class unregistered_component final : public nori::gameplay::component
{
public:
    using component::component;
};

template <typename T>
concept component_query = requires(nori::gameplay::object& value) {
    value.template find_component<T>();
    value.template find_components<T>();
    value.template add_component<T>();
};

static_assert(component_query<test_mesh>);
static_assert(!component_query<unregistered_component>);
static_assert(std::same_as<decltype(std::declval<nori::gameplay::object&>().find_component<test_mesh>()), test_mesh*>);
static_assert(std::same_as<decltype(std::declval<const nori::gameplay::object&>().find_component<test_mesh>()), const test_mesh*>);
static_assert(std::same_as<decltype(std::declval<const nori::gameplay::object&>().find_components<test_mesh>()), std::vector<const test_mesh*>>);

TEST(component_test, component_references)
{
    int destroyed{};
    {
        nori::gameplay::object value;
        nori::gameplay::object foreign;
        auto& first{value.add_component<tracked_component>(destroyed)};
        auto& first_observer{value.add_component<removal_observer>(first)};
        auto& second_observer{value.add_component<removal_observer>(first)};
        auto& mesh{value.add_component<test_mesh>()};
        auto& mesh_observer{value.add_component<removal_observer>(mesh)};
        for (int i{}; i < 128; ++i)
        {
            value.add_component<test_attachment>();
        }
        const auto& read_only{value};
        ASSERT_EQ((read_only.find_component<tracked_component>()), (&first)) << "component addresses or references changed after insertion";
        ASSERT_EQ((read_only.find_component<test_mesh>()), (&mesh)) << "component addresses or references changed after insertion";
        ASSERT_EQ((first_observer.target()), (&first)) << "component addresses or references changed after insertion";
        ASSERT_EQ((second_observer.target()), (&first)) << "component addresses or references changed after insertion";
        ASSERT_EQ((mesh_observer.target()), (&mesh)) << "component addresses or references changed after insertion";
        ASSERT_EQ((nori::core::cast<nori::gameplay::scene_component>(static_cast<const nori::gameplay::component*>(&first))), (nullptr)) << "lookup or removal accepted an invalid component";
        ASSERT_EQ((nori::core::cast<test_mesh>(static_cast<const nori::gameplay::component*>(nullptr))), (nullptr)) << "lookup or removal accepted an invalid component";
        ASSERT_FALSE((foreign.remove_component(first))) << "lookup or removal accepted an invalid component";
        ASSERT_TRUE((value.remove_component(first))) << "detached removal failed to isolate or notify the references";
        ASSERT_FALSE((value.remove_component(first))) << "detached removal failed to isolate or notify the references";
        ASSERT_EQ((destroyed), (0)) << "detached removal failed to isolate or notify the references";
        ASSERT_FALSE((first_observer.target())) << "detached removal failed to isolate or notify the references";
        ASSERT_FALSE((second_observer.target())) << "detached removal failed to isolate or notify the references";
        ASSERT_EQ((mesh_observer.target()), (&mesh)) << "detached removal failed to isolate or notify the references";
        ASSERT_EQ((first_observer.notifications()), (1)) << "detached removal failed to isolate or notify the references";
        ASSERT_EQ((second_observer.notifications()), (1)) << "detached removal failed to isolate or notify the references";
        ASSERT_EQ((mesh_observer.notifications()), (0)) << "detached removal failed to isolate or notify the references";
    }
    ASSERT_EQ((destroyed), (1)) << "detached component was not destroyed at the boundary";
}

class duplicate_removal_probe final : public nori::core::rtti<duplicate_removal_probe, nori::gameplay::component>
{
public:
    duplicate_removal_probe(nori::gameplay::object& owner, nori::gameplay::component& target, const int& target_destroyed) :
        rtti{owner},
        target_{target},
        target_destroyed_{target_destroyed}
    {
    }

protected:
    void on_register() override
    {
        register_update(nori::gameplay::update_group::pre_physics, &duplicate_removal_probe::queue_removals);
    }

    void on_unregister() override
    {
        ASSERT_EQ((target_destroyed_), (0)) << "duplicate removal of a queued component was accepted during unregister";
        ASSERT_TRUE((owner().contains_component(target_))) << "duplicate removal of a queued component was accepted during unregister";
        ASSERT_FALSE((owner().remove_component(target_))) << "duplicate removal of a queued component was accepted during unregister";
    }

private:
    void queue_removals(float)
    {
        ASSERT_TRUE((owner().remove_component(*this))) << "removal requests changed the current snapshot";
        ASSERT_TRUE((owner().remove_component(target_))) << "removal requests changed the current snapshot";
        ASSERT_TRUE((owner().contains_component(*this))) << "removal requests changed the current snapshot";
        ASSERT_TRUE((owner().contains_component(target_))) << "removal requests changed the current snapshot";
    }

    nori::gameplay::component& target_;
    const int& target_destroyed_;
};

TEST(component_test, queued_component_removals)
{
    int destroyed{};
    nori::gameplay::world active;
    auto& value{*active.find_object(active.create_object<nori::gameplay::object>())};
    auto& target{value.add_component<tracked_component>(destroyed)};
    auto& observer{value.add_component<removal_observer>(target)};
    value.add_component<duplicate_removal_probe>(target, destroyed);
    active.fixed_update(fixed_step);
    ASSERT_FALSE((value.find_component<duplicate_removal_probe>())) << "queued component removal or notification was missed or repeated";
    ASSERT_FALSE((value.find_component<tracked_component>())) << "queued component removal or notification was missed or repeated";
    ASSERT_FALSE((observer.target())) << "queued component removal or notification was missed or repeated";
    ASSERT_EQ((observer.notifications()), (1)) << "queued component removal or notification was missed or repeated";
    ASSERT_EQ((destroyed), (1)) << "queued component removal or notification was missed or repeated";
}
} // namespace
