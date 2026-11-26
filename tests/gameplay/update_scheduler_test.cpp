#include <vector>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/gameplay/update/update_scheduler.hpp>

namespace
{
constexpr float fixed_step{1.0f / 60.0f};

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
        ASSERT_TRUE((scheduler.unregister_update(target))) << "removal changed committed state";
        ASSERT_TRUE((scheduler.is_update_enabled(target))) << "removal changed committed state";
        ASSERT_TRUE((scheduler.set_update_enabled(disabled, false))) << "disable changed committed state";
        ASSERT_TRUE((scheduler.is_update_enabled(disabled))) << "disable changed committed state";
        ASSERT_TRUE((scheduler.set_update_enabled(enabled, true))) << "enable changed committed state";
        ASSERT_FALSE((scheduler.is_update_enabled(enabled))) << "enable changed committed state";
        ASSERT_TRUE((scheduler.add_prerequisite(dependent, prerequisite))) << "deferred prerequisite rejected";
        ASSERT_FALSE((scheduler.add_prerequisite(prerequisite, dependent))) << "pending cycle accepted";
        created = scheduler.register_update(*this, nori::gameplay::update_group::post_fixed_update, &update_probe::third);
        ASSERT_FALSE((scheduler.is_update_enabled(created))) << "new registration visible before boundary";
        scheduler.unregister_update(self);
    }
};

TEST(update_scheduler_test, update_scheduler)
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
    ASSERT_FALSE((scheduler.add_prerequisite(first, second))) << "invalid update prerequisite accepted";
    ASSERT_FALSE((scheduler.add_prerequisite(first, first))) << "invalid update prerequisite accepted";
    ASSERT_FALSE((scheduler.add_prerequisite(first, third))) << "invalid update prerequisite accepted";
    ASSERT_FALSE((scheduler.add_prerequisite(first, other))) << "invalid update prerequisite accepted";
    ASSERT_FALSE((scheduler.add_prerequisite(second, first))) << "duplicate prerequisite accepted";
    const auto variable{scheduler.register_update(probe, nori::gameplay::update_group::update, &update_probe::first)};
    const auto late{scheduler.register_update(probe, nori::gameplay::update_group::late_update, &update_probe::third)};
    ASSERT_FALSE((scheduler.add_prerequisite(first, variable))) << "cross-category dependency accepted";
    ASSERT_FALSE((scheduler.add_prerequisite(late, first))) << "cross-category dependency accepted";
    scheduler.execute(nori::gameplay::update_category::variable, 0.02f);
    ASSERT_EQ((calls), (std::vector<int>{1, 3})) << "variable category dispatch failed";
    EXPECT_NEAR((probe.last_delta_seconds), (0.02f), 0.0001f) << "variable category dispatch failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1, 2, 3})) << "update ordering or delta seconds incorrect";
    EXPECT_NEAR((probe.last_delta_seconds), (fixed_step), 0.0001f) << "update ordering or delta seconds incorrect";
    calls.clear();
    ASSERT_TRUE((scheduler.set_update_enabled(first, false))) << "disable failed";
    ASSERT_FALSE((scheduler.is_update_enabled(first))) << "disable failed";
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{2, 3})) << "disabled prerequisite incorrectly blocked its dependent";
    ASSERT_TRUE((scheduler.set_update_enabled(first, true))) << "update edit failed";
    ASSERT_TRUE((scheduler.remove_prerequisite(second, first))) << "update edit failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{2, 1, 3})) << "registration ordering was not deterministic";

    calls.clear();
    probe.target = third;
    probe.disabled = second;
    probe.enabled = scheduler.register_update(probe, nori::gameplay::update_group::post_physics, &update_probe::second);
    scheduler.set_update_enabled(probe.enabled, false);
    probe.dependent = second;
    probe.prerequisite = first;
    probe.self = scheduler.register_update(probe, nori::gameplay::update_group::input, &update_probe::mutate);
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{4, 2, 1, 3})) << "mutation changed current snapshot";
    ASSERT_FALSE((scheduler.unregister_update(third))) << "stale or foreign handle accepted";
    ASSERT_FALSE((scheduler.set_update_enabled(other, true))) << "stale or foreign handle accepted";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1, 2, 3})) << "deferred edits were not applied at next cycle";
    ASSERT_TRUE((scheduler.set_update_enabled(second, true))) << "re-enable failed";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1, 2, 2, 3})) << "deferred dependency was not applied";
    ASSERT_TRUE((probe.created)) << "removed update handle was reused";
    ASSERT_NE((probe.created), (third)) << "removed update handle was reused";
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

TEST(update_scheduler_test, update_ownership)
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
    ASSERT_TRUE((scheduler.owns_update(base, a))) << "explicit callback owner did not match its base reference";
    ASSERT_TRUE((scheduler.owns_update(base, b))) << "explicit callback owner did not match its base reference";
    ASSERT_FALSE((scheduler.owns_update(base, c))) << "explicit callback owner did not match its base reference";
    ASSERT_TRUE((scheduler.add_prerequisite(c, a))) << "owner prerequisite rejected";
    ASSERT_EQ((scheduler.unregister_updates(base)), (2)) << "owner removal affected wrong callbacks";
    ASSERT_FALSE((scheduler.owns_update(base, a))) << "owner removal affected wrong callbacks";
    ASSERT_FALSE((scheduler.is_update_enabled(b))) << "owner removal affected wrong callbacks";
    ASSERT_TRUE((scheduler.is_update_enabled(c))) << "owner removal affected wrong callbacks";
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1})) << "owner removal left a dangling dependency or callback";

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
            ASSERT_TRUE((scheduler.owns_update(target, created))) << "pending registration ownership or committed state incorrect";
            ASSERT_FALSE((scheduler.is_update_enabled(created))) << "pending registration ownership or committed state incorrect";
            ASSERT_EQ((scheduler.unregister_updates(target)), (2)) << "owner removal modified the current snapshot or missed a pending callback";
            ASSERT_TRUE((scheduler.is_update_enabled(existing))) << "owner removal modified the current snapshot or missed a pending callback";
            ASSERT_TRUE((scheduler.unregister_update(own))) << "could not remove mutation callback";
        }
    };

    const auto existing{scheduler.register_update(first, nori::gameplay::update_group::pre_physics, &owner_probe::run)};
    removal_probe removal{scheduler, first, existing};
    removal.own = scheduler.register_update(removal, nori::gameplay::update_group::input, &removal_probe::run);
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1, 1})) << "owner removal did not commit at category boundary";
    ASSERT_FALSE((scheduler.owns_update(first, existing))) << "owner removal did not commit at category boundary";
    ASSERT_FALSE((scheduler.owns_update(first, removal.created))) << "owner removal did not commit at category boundary";
    calls.clear();
    scheduler.execute(nori::gameplay::update_category::variable, 0.02f);
    scheduler.execute(nori::gameplay::update_category::fixed, fixed_step);
    ASSERT_EQ((calls), (std::vector<int>{1})) << "owner removal leaked callbacks or removed another owner";
}
} // namespace
