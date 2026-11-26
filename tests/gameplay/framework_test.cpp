#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/gameplay.hpp>

namespace
{
constexpr float fixed_step{1.0f / 60.0f};

bool approximately_equal(float a, float b)
{
    return std::abs(a - b) < 0.0001f;
}

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

TEST(gameplay_framework_test, components)
{
    nori::gameplay::object value;
    auto& root{value.root_component()};
    auto root_pose{nori::gameplay::transform::identity};
    root_pose.position = {4.0f, 0.0f, 0.0f};
    root_pose.scale = {2.0f, 2.0f, 2.0f};
    ASSERT_TRUE((&root.owner() == &value && !root.is_registered())) << "unattached component owner or registration incorrect";
    ASSERT_TRUE((root.set_relative_transform(root_pose))) << "valid root transform was rejected";

    auto hand_pose{nori::gameplay::transform::identity};
    hand_pose.position.x = 1.0f;
    hand_pose.rotation = nori::gameplay::axis_rotation({0.0f, 0.0f, 1.0f}, 0.4f);
    auto& hand{value.add_component<test_attachment>(hand_pose)};
    ASSERT_TRUE((hand.attach_to_component(root, nori::gameplay::keep_relative_transform))) << "could not attach hand to root";

    auto mesh_pose{nori::gameplay::transform::identity};
    mesh_pose.position.x = -3.0f;
    auto& body{value.add_component<test_mesh>(mesh_pose)};
    auto& weapon{value.add_component<test_mesh>()};
    ASSERT_TRUE((body.attach_to_component(root, nori::gameplay::keep_world_transform) && approximately_equal(body.world_transform().position.x, -3.0f))) << "keep-world attachment changed the mesh position";
    ASSERT_TRUE((weapon.attach_to_component(hand, nori::gameplay::snap_to_target_not_including_scale))) << "weapon attachment failed";
    ASSERT_TRUE((approximately_equal(weapon.world_transform().position.x, 6.0f) && approximately_equal(weapon.world_transform().scale.x, 1.0f) && approximately_equal(weapon.world_transform().rotation.z, hand.world_transform().rotation.z))) << "weapon did not snap with its world scale preserved";
    ASSERT_TRUE((!hand.attach_to_component(weapon, nori::gameplay::keep_relative_transform))) << "attachment cycle was accepted";
    const auto matches{value.find_components<test_mesh>()};
    ASSERT_TRUE((matches.size() == 2 &&
            ((matches[0] == &body && matches[1] == &weapon) || (matches[0] == &weapon && matches[1] == &body)) &&
            (value.find_component<test_mesh>() == &body || value.find_component<test_mesh>() == &weapon))) << "duplicate component lookup did not find both components";
    ASSERT_TRUE((value.find_components<nori::gameplay::scene_component>().size() == 4)) << "component lookup excluded derived types";
    ASSERT_TRUE((value.remove_component(hand) && weapon.parent() == &root && approximately_equal(weapon.world_transform().position.x, 6.0f))) << "removing a parent failed to preserve its child's transform";
    ASSERT_TRUE((weapon.detach_from_component(nori::gameplay::detachment_rule::keep_world) && approximately_equal(weapon.world_transform().position.x, 6.0f))) << "detachment changed the weapon position";
    ASSERT_TRUE((value.remove_component(body) && !value.remove_component(body) && value.find_component<test_mesh>() == &weapon && !value.remove_component(root))) << "component removal failed to isolate the requested instance";
}

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
            ASSERT_TRUE((&value.owner() == &owner() && !owner().contains_component(value))) << "component was destroyed or still active at notification";
            ASSERT_TRUE((!owner().remove_component(*this))) << "notification allowed recursive removal";
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

TEST(gameplay_framework_test, component_references)
{
    int destroyed{};
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
    ASSERT_TRUE((read_only.find_component<tracked_component>() == &first && read_only.find_component<test_mesh>() == &mesh && first_observer.target() == &first && second_observer.target() == &first && mesh_observer.target() == &mesh)) << "component addresses or references changed after insertion";
    ASSERT_TRUE((nori::core::cast<nori::gameplay::scene_component>(static_cast<const nori::gameplay::component*>(&first)) == nullptr &&
            nori::core::cast<test_mesh>(static_cast<const nori::gameplay::component*>(nullptr)) == nullptr &&
            !foreign.remove_component(first))) << "lookup or removal accepted an invalid component";
    ASSERT_TRUE((value.remove_component(first) && !value.remove_component(first) && destroyed == 0 && !first_observer.target() && !second_observer.target() && mesh_observer.target() == &mesh && first_observer.notifications() == 1 && second_observer.notifications() == 1 && mesh_observer.notifications() == 0)) << "detached removal failed to isolate or notify the references";
    value.finish_update();
    ASSERT_TRUE((destroyed == 1)) << "detached component was not destroyed at the boundary";
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
        ASSERT_TRUE((target_destroyed_ == 0 && owner().contains_component(target_) && !owner().remove_component(target_))) << "duplicate removal of a queued component was accepted during unregister";
    }

private:
    void queue_removals(float)
    {
        ASSERT_TRUE((owner().remove_component(*this) && owner().remove_component(target_) && owner().contains_component(*this) && owner().contains_component(target_))) << "removal requests changed the current snapshot";
    }

    nori::gameplay::component& target_;
    const int& target_destroyed_;
};

TEST(gameplay_framework_test, queued_component_removals)
{
    int destroyed{};
    nori::gameplay::world active;
    auto& value{*active.find_object(active.create_object<nori::gameplay::object>())};
    auto& target{value.add_component<tracked_component>(destroyed)};
    auto& observer{value.add_component<removal_observer>(target)};
    value.add_component<duplicate_removal_probe>(target, destroyed);
    active.fixed_update(fixed_step);
    ASSERT_TRUE((!value.find_component<duplicate_removal_probe>() && !value.find_component<tracked_component>() && !observer.target() && observer.notifications() == 1 && destroyed == 1)) << "queued component removal or notification was missed or repeated";
}

TEST(gameplay_framework_test, object_lifecycle)
{
    int destroyed{};
    int components_destroyed{};
    nori::gameplay::world active;
    nori::gameplay::world foreign;
    const auto first{active.create_object<tracked_object>(destroyed)};
    auto* value{active.find_object(first)};
    ASSERT_TRUE((value && !foreign.find_object(first))) << "registry accepted a foreign handle";
    auto& component{value->add_component<tracked_component>(components_destroyed)};
    ASSERT_TRUE((&component.owner() == value && component.is_registered())) << "component owner or registration was not assigned";
    for (int i{}; i < 128; ++i)
    {
        active.create_object<nori::gameplay::object>();
    }
    ASSERT_TRUE((active.find_object(first) == value)) << "object address changed after insertion";
    ASSERT_TRUE((value->remove_component(component) && !component.is_registered() && !value->find_component<tracked_component>() && components_destroyed == 0)) << "component removal was not deferred";
    active.flush_pending_changes();
    ASSERT_TRUE((components_destroyed == 1)) << "component was not destroyed at the boundary";
    value->add_component<tracked_component>(components_destroyed);
    ASSERT_TRUE((active.destroy_object(first) && !active.find_object(first) && !active.destroy_object(first) && destroyed == 0)) << "pending object was visible or destroyed early";
    for (const auto handle : active.objects())
    {
        ASSERT_TRUE((handle != first)) << "pending object remained in iteration";
    }
    active.flush_pending_changes();
    ASSERT_TRUE((destroyed == 1 && components_destroyed == 2)) << "object or owned component leaked";
    const auto next{active.create_object<tracked_object>(destroyed)};
    ASSERT_TRUE((next.slot == first.slot && next.generation != first.generation && !active.find_object(first))) << "slot reuse accepted a stale handle";
    ASSERT_TRUE((!active.destroy_object(first) && active.find_object(next))) << "stale handle removed a live object";
}

struct update_probe
{
    nori::gameplay::update_scheduler& scheduler;
    std::vector<int>& calls;
    nori::gameplay::update_handle target{};
    nori::gameplay::update_handle created{};
    nori::gameplay::update_handle self{};
    nori::gameplay::update_handle disabled{};
    nori::gameplay::update_handle enabled{};
    nori::gameplay::update_handle dependent{};
    nori::gameplay::update_handle prerequisite{};
    float last_delta_seconds{};

    void first(float delta_seconds)
    {
        last_delta_seconds = delta_seconds;
        calls.push_back(1);
    }

    void second(float)
    {
        calls.push_back(2);
    }

    void third(float)
    {
        calls.push_back(3);
    }

    void mutate(float)
    {
        calls.push_back(4);
        ASSERT_TRUE((scheduler.unregister_update(target) && scheduler.is_update_enabled(target))) << "removal changed committed state";
        ASSERT_TRUE((scheduler.set_update_enabled(disabled, false) && scheduler.is_update_enabled(disabled))) << "disable changed committed state";
        ASSERT_TRUE((scheduler.set_update_enabled(enabled, true) && !scheduler.is_update_enabled(enabled))) << "enable changed committed state";
        ASSERT_TRUE((scheduler.add_prerequisite(dependent, prerequisite))) << "deferred prerequisite rejected";
        ASSERT_TRUE((!scheduler.add_prerequisite(prerequisite, dependent))) << "pending cycle accepted";
        created = scheduler.register_update(*this, nori::gameplay::update_group::post_fixed_update, &update_probe::third);
        ASSERT_TRUE((!scheduler.is_update_enabled(created))) << "new registration visible before boundary";
        scheduler.unregister_update(self);
    }
};

TEST(gameplay_framework_test, update_scheduler)
{
    nori::gameplay::update_scheduler scheduler;
    nori::gameplay::update_scheduler foreign;
    std::vector<int> calls;
    update_probe probe{scheduler, calls};
    const auto second{scheduler.register_update(probe, nori::gameplay::update_group::pre_physics, &update_probe::second)};
    const auto first{scheduler.register_update(probe, nori::gameplay::update_group::pre_physics, &update_probe::first)};
    const auto third{scheduler.register_update(probe, nori::gameplay::update_group::post_physics, &update_probe::third)};
    const auto other{foreign.register_update(probe, nori::gameplay::update_group::pre_physics, &update_probe::first)};
    ASSERT_TRUE((scheduler.add_prerequisite(second, first))) << "valid update prerequisite rejected";
    ASSERT_TRUE((!scheduler.add_prerequisite(first, second) && !scheduler.add_prerequisite(first, first) && !scheduler.add_prerequisite(first, third) && !scheduler.add_prerequisite(first, other))) << "invalid update prerequisite accepted";
    ASSERT_TRUE((!scheduler.add_prerequisite(second, first))) << "duplicate prerequisite accepted";
    const auto variable{scheduler.register_update(probe, nori::gameplay::update_group::update, &update_probe::first)};
    const auto late{scheduler.register_update(probe, nori::gameplay::update_group::late_update, &update_probe::third)};
    ASSERT_TRUE((!scheduler.add_prerequisite(first, variable) && !scheduler.add_prerequisite(late, first))) << "cross-category dependency accepted";
    scheduler.execute(nori::gameplay::update_category::variable, 0.02f);
    ASSERT_TRUE((calls == std::vector<int>{1, 3} && approximately_equal(probe.last_delta_seconds, 0.02f))) << "variable category dispatch failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1, 2, 3} && approximately_equal(probe.last_delta_seconds, fixed_step))) << "update ordering or delta seconds incorrect";
    calls.clear();
    ASSERT_TRUE((scheduler.set_update_enabled(first, false) && !scheduler.is_update_enabled(first))) << "disable failed";
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{2, 3})) << "disabled prerequisite incorrectly blocked its dependent";
    ASSERT_TRUE((scheduler.set_update_enabled(first, true) && scheduler.remove_prerequisite(second, first))) << "update edit failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{2, 1, 3})) << "registration ordering was not deterministic";

    calls.clear();
    probe.target = third;
    probe.disabled = second;
    probe.enabled = scheduler.register_update(probe, nori::gameplay::update_group::post_physics, &update_probe::second);
    scheduler.set_update_enabled(probe.enabled, false);
    probe.dependent = second;
    probe.prerequisite = first;
    probe.self = scheduler.register_update(probe, nori::gameplay::update_group::input, &update_probe::mutate);
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{4, 2, 1, 3})) << "mutation changed current snapshot";
    ASSERT_TRUE((!scheduler.unregister_update(third) && !scheduler.set_update_enabled(other, true))) << "stale or foreign handle accepted";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1, 2, 3})) << "deferred edits were not applied at next cycle";
    ASSERT_TRUE((scheduler.set_update_enabled(second, true))) << "re-enable failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1, 2, 2, 3})) << "deferred dependency was not applied";
    ASSERT_TRUE((probe.created && probe.created != third)) << "removed update handle was reused";
}

struct owner_prefix
{
    virtual ~owner_prefix() = default;
};

struct owner_base
{
    virtual ~owner_base() = default;
};

struct owner_probe final : owner_prefix, owner_base
{
    std::vector<int>& calls;

    explicit owner_probe(std::vector<int>& value) :
        calls{value}
    {
    }

    void run(float)
    {
        calls.push_back(1);
    }
};

TEST(gameplay_framework_test, update_ownership)
{
    nori::gameplay::update_scheduler scheduler;
    std::vector<int> calls;
    owner_probe first{calls};
    owner_probe second{calls};
    auto& base{static_cast<owner_base&>(first)};
    auto& other_base{static_cast<owner_base&>(second)};
    const auto a{scheduler.register_update(base, first, nori::gameplay::update_group::pre_physics, &owner_probe::run)};
    const auto b{scheduler.register_update(base, first, nori::gameplay::update_group::post_physics, &owner_probe::run)};
    const auto c{scheduler.register_update(other_base, second, nori::gameplay::update_group::post_physics, &owner_probe::run)};
    ASSERT_TRUE((scheduler.owns_update(base, a) && scheduler.owns_update(base, b) && !scheduler.owns_update(base, c))) << "explicit callback owner did not match its base reference";
    ASSERT_TRUE((scheduler.add_prerequisite(c, a))) << "owner prerequisite rejected";
    ASSERT_TRUE((scheduler.unregister_updates(base) == 2 && !scheduler.owns_update(base, a) && !scheduler.is_update_enabled(b) && scheduler.is_update_enabled(c))) << "owner removal affected wrong callbacks";
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1})) << "owner removal left a dangling dependency or callback";

    struct removal_probe
    {
        nori::gameplay::update_scheduler& scheduler;
        owner_probe& target;
        nori::gameplay::update_handle existing;
        nori::gameplay::update_handle created{};
        nori::gameplay::update_handle own{};

        void run(float)
        {
            created = scheduler.register_update(target, nori::gameplay::update_group::update, &owner_probe::run);
            ASSERT_TRUE((scheduler.owns_update(target, created) && !scheduler.is_update_enabled(created))) << "pending registration ownership or committed state incorrect";
            ASSERT_TRUE((scheduler.unregister_updates(target) == 2 && scheduler.is_update_enabled(existing))) << "owner removal modified the current snapshot or missed a pending callback";
            ASSERT_TRUE((scheduler.unregister_update(own))) << "could not remove mutation callback";
        }
    };

    const auto existing{scheduler.register_update(first, nori::gameplay::update_group::pre_physics, &owner_probe::run)};
    removal_probe removal{scheduler, first, existing};
    removal.own = scheduler.register_update(removal, nori::gameplay::update_group::input, &removal_probe::run);
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1, 1} && !scheduler.owns_update(first, existing) && !scheduler.owns_update(first, removal.created))) << "owner removal did not commit at category boundary";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::variable, 0.02f);
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{1})) << "owner removal leaked callbacks or removed another owner";
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
        register_update(nori::gameplay::update_group::pre_physics, &lifecycle_update_component::before);
        register_update(nori::gameplay::update_group::post_physics, &lifecycle_update_component::after);
    }

    void on_unregister() override
    {
        calls_.push_back(20);
    }

private:
    void before(float)
    {
        calls_.push_back(1);
        if (remove_self_)
        {
            ASSERT_TRUE((owner().remove_component(*this) && owner().find_component<lifecycle_update_component>() == this && is_registered())) << "component disappeared inside snapshot";
        }
    }

    void after(float)
    {
        calls_.push_back(2);
    }

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
    void on_register() override
    {
        register_update(nori::gameplay::update_group::input, &lifecycle_update_object::before);
        register_update(nori::gameplay::update_group::post_fixed_update, &lifecycle_update_object::after);
    }

private:
    void before(float)
    {
        calls_.push_back(3);
    }

    void after(float)
    {
        calls_.push_back(4);
    }

    std::vector<int>& calls_;
};

TEST(gameplay_framework_test, update_lifecycle)
{
    std::vector<int> calls;
    nori::gameplay::world active;
    const auto handle{active.create_object<lifecycle_update_object>(calls)};
    auto* object{active.find_object(handle)};
    ASSERT_TRUE((calls == std::vector<int>{10})) << "initial component not registered";
    calls.clear();
    active.fixed_update(fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{3, 1, 2, 4, 20} && !object->find_component<lifecycle_update_component>())) << "component removal did not wait until cycle boundary";
    auto& added{object->add_component<lifecycle_update_component>(calls, false)};
    ASSERT_TRUE((&added.owner() == object && calls.back() == 10)) << "dynamic component registration failed";
    calls.clear();
    active.fixed_update(fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{3, 1, 2, 4})) << "component multi-group update incorrect";
    calls.clear();
    ASSERT_TRUE((active.destroy_object(handle) && calls == std::vector<int>{20})) << "object removal did not unregister components";
    calls.clear();
    active.fixed_update(fixed_step);
    ASSERT_TRUE((calls.empty())) << "removed object's update executed";
}

class variable_removal_object final : public nori::core::rtti<variable_removal_object, nori::gameplay::object>
{
public:
    explicit variable_removal_object(std::vector<int>& calls) :
        calls_{calls}
    {
    }

protected:
    void on_register() override
    {
        register_update(nori::gameplay::update_group::update, &variable_removal_object::remove_self);
        register_update(nori::gameplay::update_group::late_update, &variable_removal_object::late);
        register_update(nori::gameplay::update_group::pre_physics, &variable_removal_object::fixed);
    }

    void on_unregister() override
    {
        calls_.push_back(30);
    }

private:
    void remove_self(float delta_seconds)
    {
        ASSERT_TRUE((approximately_equal(delta_seconds, 0.005f))) << "variable delta seconds incorrect";
        calls_.push_back(5);
        ASSERT_TRUE((world().destroy_object(handle()) && world().find_object(handle()) == this)) << "object removal was not deferred";
        ASSERT_TRUE((!world().destroy_object(handle()))) << "duplicate object destruction accepted";
    }

    void late(float)
    {
        ASSERT_TRUE((world().find_object(handle()) == this)) << "object detached during snapshot";
        calls_.push_back(6);
    }

    void fixed(float delta_seconds)
    {
        ASSERT_TRUE((approximately_equal(delta_seconds, fixed_step))) << "fixed delta seconds incorrect";
        calls_.push_back(7);
    }

    std::vector<int>& calls_;
};

TEST(gameplay_framework_test, variable_update_lifecycle)
{
    std::vector<int> calls;
    nori::gameplay::game_instance game;
    auto& active{game.create_world()};
    const auto handle{active.create_object<variable_removal_object>(calls)};
    game.fixed_update(fixed_step);
    ASSERT_TRUE((calls == std::vector<int>{7})) << "fixed update invoked variable callbacks";
    calls.clear();
    game.update(0.005f);
    ASSERT_TRUE((calls == std::vector<int>{5, 6, 30} && !active.find_object(handle))) << "variable object removal violated the category boundary";
    calls.clear();
    game.fixed_update(fixed_step);
    game.update(0.005f);
    ASSERT_TRUE((calls.empty())) << "removed object's callback executed in another category";
}

class test_physics_object final : public nori::core::rtti<test_physics_object, nori::gameplay::object>
{
public:
    test_physics_object(nori::gameplay::transform pose, nori::gameplay::physics_body_config config) :
        rtti{pose}
    {
        add_component<nori::gameplay::physics_body_component>(std::move(config));
    }
};

TEST(gameplay_framework_test, physics_body_lifecycle)
{
    nori::gameplay::world active;
    auto pose{nori::gameplay::transform::identity};
    pose.position.z = 5.0f;
    const nori::gameplay::physics_body_config config{.type = nori::physics2d::body_type::kinematic_body, .velocity = {{6.0f, 0.0f}, 3.0f}};
    const auto object_handle{active.create_object<test_physics_object>(pose, config)};
    auto& object{*active.find_object(object_handle)};
    auto& body{*object.find_component<nori::gameplay::physics_body_component>()};
    const auto original{body.body_handle()};
    ASSERT_TRUE((active.physics_scene().contains(original))) << "body was not created during world registration";
    active.fixed_update(fixed_step);
    const auto moved{object.root_component().world_transform()};
    ASSERT_TRUE((approximately_equal(moved.position.x, 0.1f) && moved.position.z == 5.0f && approximately_equal(moved.rotation.z, std::sin(0.025f)))) << "kinematic pose was not reflected in gameplay";
    ASSERT_TRUE((body.teleport({{2.0f, 1.0f}, 0.0f}, true))) << "could not queue velocity-preserving teleport";
    active.fixed_update(fixed_step);
    ASSERT_TRUE((approximately_equal(object.root_component().world_transform().position.x, 2.1f))) << "velocity-preserving teleport stopped the kinematic body";

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
            ASSERT_TRUE((object_.remove_component(body_) && active_.physics_scene().contains(handle))) << "body was removed inside update";
        }

    private:
        nori::gameplay::world& active_;
        ::nori::gameplay::object& object_;
        nori::gameplay::physics_body_component& body_;
    } probe{active, object, body};

    active.scheduler().register_update(probe, nori::gameplay::update_group::input, &removal_probe::remove);
    active.fixed_update(fixed_step);
    ASSERT_TRUE((!active.physics_scene().contains(original))) << "body survived component removal";
}

TEST(gameplay_framework_test, static_body_sync)
{
    nori::gameplay::world active;
    auto initial_pose{nori::gameplay::transform::identity};
    initial_pose.position = {2.0f, 1.0f, 3.0f};
    const auto handle{active.create_object<test_physics_object>(initial_pose, nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::static_body, .colliders = {{{0.5f, 0.5f}, {}, {}}}})};
    auto& object{*active.find_object(handle)};
    const auto body{object.find_component<nori::gameplay::physics_body_component>()};
    auto next_pose{initial_pose};
    next_pose.position.x = 4.0f;
    next_pose.rotation = nori::gameplay::axis_rotation({0.0f, 0.0f, 1.0f}, 0.5f);
    ASSERT_TRUE((object.root_component().set_relative_transform(next_pose))) << "static root transform was rejected";
    active.fixed_update(fixed_step);
    const auto current{active.physics_scene().get_transform(body->body_handle())};
    ASSERT_TRUE((current && current->position.x == 4.0f && std::abs(current->angle_radians - 0.5f) < 0.001f)) << "static root transform was not applied to physics";
    ASSERT_TRUE((body->teleport({{6.0f, 1.0f}, 0.0f}))) << "could not queue static teleport";
    active.fixed_update(fixed_step);
    active.fixed_update(fixed_step);
    ASSERT_TRUE((object.root_component().relative_transform().position.x == 6.0f &&
            active.physics_scene().get_transform(body->body_handle())->position.x == 6.0f)) << "static teleport was reverted by the root transform";
}

TEST(gameplay_framework_test, gameplay_trigger_events)
{
    nori::gameplay::world active;
    active.physics_scene().set_gravity({});
    auto settings{nori::physics2d::default_box_collider_settings};
    settings.is_sensor = true;
    const auto sensor{active.create_object<test_physics_object>(nori::gameplay::transform::identity,
        nori::gameplay::physics_body_config{nori::physics2d::body_type::static_body, {}, false, {}, {{{0.5f, 0.5f}, {}, settings}, {{0.25f, 0.25f}, {}, settings}}})};
    const auto visitor{active.create_object<test_physics_object>(nori::gameplay::transform::identity,
        nori::gameplay::physics_body_config{.type = nori::physics2d::body_type::dynamic_body, .colliders = {{{0.25f, 0.25f}, {}, {}}}})};
    const nori::physics2d::box_collider_config raw_shape[]{{{0.5f, 0.5f}, {}, settings}};
    const auto raw_sensor{active.physics_scene().create_body({nori::physics2d::body_type::static_body, {}, {}, false, {}}, raw_shape)};
    ASSERT_TRUE((active.physics_scene().contains(raw_sensor))) << "could not create a physics-only sensor";

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
        ASSERT_TRUE((event.sensor == sensor && event.visitor == visitor && event.visitor_collider == 0)) << "trigger was not mapped to its Gameplay owners";
        events.push_back(event);
        if (event.phase == nori::gameplay::trigger_phase::begin)
        {
            if (events.size() == 1)
            {
                ASSERT_TRUE((active.destroy_object(event.visitor))) << "event callback could not remove the visitor";
                static_cast<void>(active.subscribe_trigger_changed([&](const nori::gameplay::trigger_changed& later)
                {
                    late_events.push_back(later.sensor_collider);
                }));
            }
            ASSERT_TRUE((active.find_object(event.visitor))) << "event callback did not defer object removal";
        }
    }));
    active.fixed_update(fixed_step);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].phase, nori::gameplay::trigger_phase::begin);
    EXPECT_EQ(events[1].phase, nori::gameplay::trigger_phase::begin);
    EXPECT_NE(events[0].sensor_collider, events[1].sensor_collider);
    EXPECT_EQ(late_events, (std::vector{events[1].sensor_collider}));
    EXPECT_FALSE(active.find_object(visitor));
    EXPECT_EQ(once_count, 1u);
    EXPECT_FALSE(active.unsubscribe(once));
    active.fixed_update(0.0f);
    ASSERT_TRUE((events.size() == 2)) << "invalid fixed step replayed the previous physics events";
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

TEST(gameplay_framework_test, trigger_subscription_handles)
{
    nori::gameplay::world first;
    nori::gameplay::world second;
    const auto original{first.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&) {})};
    const auto foreign{second.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&) {})};
    EXPECT_FALSE(first.unsubscribe(foreign));
    EXPECT_TRUE(first.unsubscribe(original));
    EXPECT_FALSE(first.unsubscribe(original));
    const auto replacement{first.subscribe_trigger_changed([](const nori::gameplay::trigger_changed&) {})};
    EXPECT_FALSE(first.unsubscribe(original));
    EXPECT_TRUE(first.unsubscribe(replacement));
    EXPECT_TRUE(second.unsubscribe(foreign));
}


TEST(gameplay_framework_test, world_initializer)
{
    struct initializer
    {
        initializer(nori::gameplay::world& active, float x)
        {
            auto pose{nori::gameplay::transform::identity};
            pose.position.x = x;
            active.create_object<nori::gameplay::object>(pose);
        }
    };

    nori::gameplay::game_instance game;
    auto& active{game.create_world<initializer>(3.0f)};
    ASSERT_EQ(active.objects().size(), 1u);
    EXPECT_FLOAT_EQ(active.find_object(active.objects().front())->root_component().world_transform().position.x, 3.0f);
    game.clear_world();
    EXPECT_EQ(game.active_world(), nullptr);
}
} // namespace
