#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/slot_map.hpp>

namespace
{
struct test_handle_tag;
using test_handle = nori::core::basic_handle<test_handle_tag>;
using test_slot_map = nori::core::slot_map<test_handle, std::uint32_t>;

struct handle_deleter
{
    std::vector<test_handle>* destroyed;

    void operator()(test_handle value) const
    {
        destroyed->push_back(value);
    }
};

struct potentially_throwing_move_deleter
{
    potentially_throwing_move_deleter() = default;

    potentially_throwing_move_deleter(potentially_throwing_move_deleter&&) noexcept(false)
    {
    }

    potentially_throwing_move_deleter& operator=(potentially_throwing_move_deleter&&) noexcept(false)
    {
        return *this;
    }

    void operator()(test_handle) const
    {
    }
};

static_assert(std::is_move_assignable_v<nori::core::unique_handle<test_handle, potentially_throwing_move_deleter>>);
static_assert(std::is_move_constructible_v<nori::core::unique_handle<test_handle, potentially_throwing_move_deleter>>);

TEST(slot_map_test, unique_handle_default_is_empty)
{
    nori::core::unique_handle<test_handle, potentially_throwing_move_deleter> owner;
    EXPECT_FALSE(owner);
    EXPECT_EQ(owner.get(), test_handle{});
}

TEST(slot_map_test, unique_handle_ownership)
{
    std::vector<test_handle> destroyed;
    const test_handle first{1, 1};
    const test_handle second{2, 1};
    const test_handle third{3, 1};
    {
        nori::core::unique_handle owner{first, handle_deleter{&destroyed}};
        EXPECT_EQ(owner.get(), first);
        auto moved{std::move(owner)};
        EXPECT_FALSE(owner);
        moved.reset(second);
        EXPECT_EQ(destroyed, (std::vector<test_handle>{first}));
        EXPECT_EQ(moved.release(), second);
        EXPECT_FALSE(moved);
        moved.reset(third);
    }
    EXPECT_EQ(destroyed, (std::vector<test_handle>{first, third}));
}

TEST(slot_map_test, unique_handle_move_assignment)
{
    std::vector<test_handle> first_destroyed;
    std::vector<test_handle> second_destroyed;
    const test_handle first{1, 1};
    const test_handle second{2, 1};
    {
        nori::core::unique_handle owner{first, handle_deleter{&first_destroyed}};
        nori::core::unique_handle other{second, handle_deleter{&second_destroyed}};
        owner = std::move(other);
        EXPECT_FALSE(other);
        EXPECT_EQ(first_destroyed, (std::vector<test_handle>{first}));
        EXPECT_EQ(owner.get(), second);
    }
    EXPECT_EQ(second_destroyed, (std::vector<test_handle>{second}));
}

TEST(slot_map_test, handle_identity)
{
    const test_handle empty{};
    const test_handle first{1, 2};
    const test_handle same{1, 2};
    const test_handle other_generation{1, 3};

    EXPECT_FALSE(empty);
    EXPECT_TRUE(first);
    EXPECT_EQ(first, same);
    EXPECT_NE(first, other_generation);

    const std::unordered_set<test_handle> handles{first, same, other_generation};
    EXPECT_EQ(handles.size(), 2);
}

class copy_only_value
{
public:
    explicit copy_only_value(int initial_value) :
        value{initial_value}
    {
    }

    copy_only_value(const copy_only_value&) = default;
    copy_only_value& operator=(const copy_only_value&) = delete;

    int value;
};

class non_movable_value
{
public:
    explicit non_movable_value(int initial_value) :
        value{initial_value}
    {
    }

    non_movable_value(const non_movable_value&) = delete;
    non_movable_value(non_movable_value&&) = delete;
    non_movable_value& operator=(const non_movable_value&) = delete;
    non_movable_value& operator=(non_movable_value&&) = delete;

    int value;
};

using copy_only_slot_map = nori::core::slot_map<test_handle, copy_only_value>;
using non_movable_slot_map = nori::core::slot_map<test_handle, non_movable_value>;
using move_only_slot_map = nori::core::slot_map<test_handle, std::unique_ptr<int>>;

TEST(slot_map_test, slot_reuse)
{
    test_slot_map values;

    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(values.contains({}));
    EXPECT_FALSE(values.contains(test_handle{999, 1}));
    const test_handle first{values.emplace(10)};
    const test_handle second{values.emplace(20)};

    EXPECT_FALSE(values.empty());
    EXPECT_EQ(values.size(), 2);
    EXPECT_TRUE(values.contains(first));
    EXPECT_TRUE(values.contains(second));
    EXPECT_EQ(values.at(first), 10);
    EXPECT_EQ(values.at(second), 20);

    values.erase(first);
    EXPECT_EQ(values.size(), 1);
    EXPECT_FALSE(values.contains(first));

    const test_handle replacement{values.emplace(30)};
    EXPECT_EQ(replacement.index, first.index);
    EXPECT_NE(replacement.generation, first.generation);
    EXPECT_FALSE(values.contains(first));
    EXPECT_TRUE(values.contains(replacement));
    EXPECT_EQ(values.at(replacement), 30);
}

TEST(slot_map_test, subscript_access)
{
    test_slot_map values;
    const test_handle first{values.emplace(10)};
    const test_handle second{values.emplace(20)};
    values[first] = 42;

    const test_slot_map& const_values{values};
    EXPECT_EQ(const_values[first], 42);
    EXPECT_EQ(const_values[second], 20);
    EXPECT_EQ(&values[first], &values.at(first));
    EXPECT_EQ(&const_values[first], &const_values.at(first));

    values.erase(first);
    const test_handle replacement{values.emplace(30)};
    values[replacement] = 50;
    EXPECT_EQ(const_values[replacement], 50);
    EXPECT_EQ(const_values[second], 20);
}

TEST(slot_map_test, clear_empty)
{
    test_slot_map values;
    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.begin(), values.end());

    const test_handle first{values.emplace(10)};
    values.erase(first);
    values.clear();
    values.clear();
    const test_handle replacement{values.emplace(20)};
    EXPECT_EQ(replacement.index, first.index);
    EXPECT_EQ(replacement.generation, first.generation + 1);
    EXPECT_EQ(values[replacement], 20);
}

TEST(slot_map_test, clear_preserves_slots_and_invalidates_handles)
{
    test_slot_map values;
    const std::array handles{values.emplace(10), values.emplace(20), values.emplace(30)};
    values.erase(handles[1]);
    values.clear();
    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.size(), 0);
    EXPECT_EQ(values.begin(), values.end());

    const std::array replacements{values.emplace(40), values.emplace(50), values.emplace(60)};
    std::unordered_set<std::uint32_t> reused_indices;
    for (const auto handle : replacements)
    {
        const auto it{std::ranges::find(handles, handle.index, &test_handle::index)};
        ASSERT_NE(it, handles.end());
        EXPECT_EQ(handle.generation, it->generation + 1);
        EXPECT_TRUE(values.contains(handle));
        reused_indices.insert(handle.index);
    }
    EXPECT_EQ(reused_indices.size(), handles.size());
    for (const auto handle : handles)
    {
        EXPECT_FALSE(values.contains(handle));
    }
}

TEST(slot_map_test, clear_destroys_values)
{
    nori::core::slot_map<test_handle, std::shared_ptr<int>> values;
    const test_handle first{values.emplace(std::make_shared<int>(10))};
    const test_handle removed{values.emplace(std::make_shared<int>(20))};
    const test_handle last{values.emplace(std::make_shared<int>(30))};
    const std::weak_ptr<int> first_value{values[first]};
    const std::weak_ptr<int> last_value{values[last]};
    values.erase(removed);
    values.clear();
    EXPECT_TRUE(first_value.expired());
    EXPECT_TRUE(last_value.expired());
    EXPECT_TRUE(values.empty());
}

TEST(slot_map_test, stable_addresses)
{
    test_slot_map values;
    const test_handle first{values.emplace(10)};
    const test_handle second{values.emplace(20)};
    const auto original_address{reinterpret_cast<std::uintptr_t>(&values.at(first))};
    std::uint32_t& original_value{values.at(first)};

    for (std::uint32_t i{0}; i < 4096; ++i)
    {
        values.emplace(i);
    }
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(&values.at(first)), original_address);
    original_value = 42;
    EXPECT_EQ(values.at(first), 42);

    values.erase(second);
    const test_handle replacement{values.emplace(30)};
    EXPECT_EQ(replacement.index, second.index);
    EXPECT_NE(replacement.generation, second.generation);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&values.at(first)), original_address);
    EXPECT_EQ(original_value, 42);
    EXPECT_EQ(values.at(replacement), 30);
}

TEST(slot_map_test, iteration)
{
    test_slot_map values;
    const test_handle first{values.emplace(10)};
    const test_handle second{values.emplace(20)};
    const test_handle third{values.emplace(30)};
    const test_handle fourth{values.emplace(40)};
    const test_handle fifth{values.emplace(50)};
    values.erase(first);
    values.erase(third);
    values.erase(fifth);

    std::vector<std::pair<test_handle, std::uint32_t>> entries;
    for (auto& [h, value] : values)
    {
        ++value;
        entries.emplace_back(h, value);
    }
    const std::vector<std::pair<test_handle, std::uint32_t>> expected{{second, 21}, {fourth, 41}};
    EXPECT_EQ(entries, expected);

    entries.clear();
    const test_slot_map& const_values{values};
    for (const auto& [h, value] : const_values)
    {
        entries.emplace_back(h, value);
    }
    EXPECT_EQ(entries, expected);
    values.erase(second);
    values.erase(fourth);
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.begin(), values.end());
    EXPECT_EQ(const_values.begin(), const_values.end());
}

TEST(slot_map_test, copy_preserves_slots_and_independence)
{
    for (const bool uses_assignment : {false, true})
    {
        test_slot_map source;
        const test_handle stale{source.emplace(10)};
        source.erase(stale);
        const test_handle current{source.emplace(20)};
        const test_handle first_free{source.emplace(30)};
        const test_handle second_free{source.emplace(40)};
        const test_handle last{source.emplace(50)};
        source.erase(first_free);
        source.erase(second_free);

        test_slot_map copy{source};
        if (uses_assignment)
        {
            copy.emplace(99);
            copy = source;
        }
        EXPECT_EQ(copy.size(), source.size());
        EXPECT_FALSE(copy.contains(stale));
        EXPECT_FALSE(copy.contains(first_free));
        EXPECT_FALSE(copy.contains(second_free));
        ASSERT_TRUE(copy.contains(current));
        ASSERT_TRUE(copy.contains(last));
        EXPECT_EQ(copy.at(current), 20);
        EXPECT_EQ(copy.at(last), 50);
        EXPECT_NE(&copy.at(current), &source.at(current));
        copy.at(current) = 60;
        copy.erase(last);
        EXPECT_EQ(source.at(current), 20);
        EXPECT_TRUE(source.contains(last));

        const test_handle copy_last{copy.emplace(70)};
        EXPECT_EQ(copy_last.index, last.index);
        EXPECT_EQ(copy_last.generation, last.generation + 1);
        EXPECT_FALSE(copy.contains(last));
        const test_handle copy_second{copy.emplace(80)};
        const test_handle copy_first{copy.emplace(90)};
        EXPECT_EQ(copy_second, source.emplace(80));
        EXPECT_EQ(copy_first, source.emplace(90));
        EXPECT_EQ(copy_second.index, second_free.index);
        EXPECT_EQ(copy_first.index, first_free.index);
        EXPECT_EQ(copy_second.generation, second_free.generation + 1);
        EXPECT_EQ(copy_first.generation, first_free.generation + 1);
    }
}

TEST(slot_map_test, copy_empty)
{
    const test_handle initial{1, 1};
    test_slot_map source;
    test_slot_map constructed{source};
    EXPECT_TRUE(constructed.empty());
    EXPECT_EQ(constructed.begin(), constructed.end());
    EXPECT_EQ(constructed.emplace(10), initial);

    test_slot_map assigned;
    assigned.emplace(20);
    assigned = source;
    EXPECT_EQ(assigned.size(), 0);
    EXPECT_EQ(assigned.begin(), assigned.end());
    EXPECT_EQ(assigned.emplace(30), initial);
    EXPECT_TRUE(source.empty());

    const test_handle removed{source.emplace(40)};
    source.erase(removed);
    test_slot_map copied_slots{source};
    EXPECT_TRUE(copied_slots.empty());
    EXPECT_EQ(copied_slots.begin(), copied_slots.end());
    EXPECT_FALSE(copied_slots.contains(removed));
    EXPECT_EQ(copied_slots.emplace(50), source.emplace(60));
}

TEST(slot_map_test, move_construction)
{
    const test_handle initial{1, 1};
    test_slot_map source;
    const test_handle first{source.emplace(10)};
    const test_handle removed{source.emplace(20)};
    source.erase(removed);
    const auto address{&source.at(first)};

    test_slot_map moved{std::move(source)};
    EXPECT_EQ(moved.size(), 1);
    EXPECT_EQ(&moved.at(first), address);
    EXPECT_FALSE(moved.contains(removed));
    const test_handle reused{moved.emplace(30)};
    EXPECT_EQ(reused.index, removed.index);
    EXPECT_EQ(reused.generation, removed.generation + 1);

    EXPECT_EQ(source.size(), 0);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.begin(), source.end());
    EXPECT_FALSE(source.contains(first));
    const test_handle fresh{source.emplace(40)};
    EXPECT_EQ(fresh, initial);
    EXPECT_EQ(source.begin()->first, fresh);
    EXPECT_EQ(std::ranges::distance(source), 1);
    source.erase(fresh);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(moved.at(first), 10);
}

TEST(slot_map_test, move_assignment)
{
    const test_handle initial{1, 1};
    test_slot_map source;
    const test_handle first{source.emplace(10)};
    const test_handle removed{source.emplace(20)};
    source.erase(removed);
    const auto address{&source.at(first)};

    test_slot_map moved;
    moved.emplace(99);
    moved.emplace(100);
    moved = std::move(source);
    EXPECT_EQ(moved.size(), 1);
    EXPECT_EQ(&moved.at(first), address);
    EXPECT_EQ(moved.at(first), 10);
    const test_handle reused{moved.emplace(30)};
    EXPECT_EQ(reused.index, removed.index);
    EXPECT_EQ(reused.generation, removed.generation + 1);

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.size(), 0);
    EXPECT_EQ(source.begin(), source.end());
    EXPECT_FALSE(source.contains(first));
    const test_handle fresh{source.emplace(40)};
    EXPECT_EQ(fresh, initial);
    EXPECT_TRUE(source.contains(fresh));
    EXPECT_EQ(std::ranges::distance(source), 1);
    EXPECT_EQ(moved.at(first), 10);
}

TEST(slot_map_test, move_empty)
{
    const test_handle initial{1, 1};
    test_slot_map source;
    test_slot_map moved{std::move(source)};
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(moved.begin(), moved.end());
    EXPECT_EQ(source.begin(), source.end());
    EXPECT_EQ(source.emplace(10), initial);

    test_slot_map assigned;
    assigned.emplace(20);
    assigned = std::move(moved);
    EXPECT_TRUE(assigned.empty());
    EXPECT_EQ(assigned.begin(), assigned.end());
    EXPECT_EQ(moved.emplace(30), initial);
    EXPECT_EQ(assigned.emplace(40), initial);
}

TEST(slot_map_test, self_assignment)
{
    test_slot_map values;
    const test_handle first{values.emplace(10)};
    const test_handle removed{values.emplace(20)};
    values.erase(removed);
    const auto address{&values.at(first)};
    test_slot_map& alias{values};

    EXPECT_EQ(&(values = alias), &values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(&values.at(first), address);
    EXPECT_EQ(&(values = std::move(alias)), &values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(&values.at(first), address);
    const test_handle reused{values.emplace(30)};
    EXPECT_EQ(reused.index, removed.index);
    EXPECT_EQ(reused.generation, removed.generation + 1);
}

TEST(slot_map_test, copy_without_value_assignment)
{
    copy_only_slot_map source;
    const test_handle first{source.emplace(10)};
    copy_only_slot_map constructed{source};
    copy_only_slot_map assigned;
    assigned.emplace(99);
    assigned = source;
    EXPECT_EQ(constructed.at(first).value, 10);
    EXPECT_EQ(assigned.at(first).value, 10);
    assigned.at(first).value = 20;
    EXPECT_EQ(source.at(first).value, 10);
    EXPECT_EQ(constructed.at(first).value, 10);
}

TEST(slot_map_test, move_only_values)
{
    move_only_slot_map source;
    const test_handle first{source.emplace(std::make_unique<int>(10))};
    const auto address{&source.at(first)};
    const auto pointee{source.at(first).get()};
    move_only_slot_map moved{std::move(source)};
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(&moved.at(first), address);
    EXPECT_EQ(moved.at(first).get(), pointee);

    move_only_slot_map assigned;
    assigned.emplace(std::make_unique<int>(99));
    assigned = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(&assigned.at(first), address);
    EXPECT_EQ(*assigned.at(first), 10);
}

TEST(slot_map_test, non_movable_values)
{
    non_movable_slot_map source;
    const test_handle first{source.emplace(10)};
    const auto address{&source.at(first)};
    for (int i{0}; i < 128; ++i)
    {
        source.emplace(i);
    }
    EXPECT_EQ(&source.at(first), address);
    non_movable_slot_map moved{std::move(source)};
    EXPECT_EQ(&moved.at(first), address);
    EXPECT_TRUE(source.empty());
    EXPECT_TRUE(source.contains(source.emplace(20)));

    non_movable_slot_map assigned;
    assigned.emplace(99);
    assigned = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(&assigned.at(first), address);
    assigned.erase(first);
    const test_handle reused{assigned.emplace(30)};
    EXPECT_EQ(reused.index, first.index);
    EXPECT_EQ(reused.generation, first.generation + 1);
    EXPECT_EQ(assigned.at(reused).value, 30);
    assigned.clear();
    EXPECT_TRUE(assigned.empty());
    EXPECT_FALSE(assigned.contains(reused));
    const test_handle after_clear{assigned.emplace(40)};
    EXPECT_EQ(assigned[after_clear].value, 40);
}

TEST(slot_map_test, iterator_conversion)
{
    test_slot_map values;
    const test_handle removed{values.emplace(10)};
    const test_handle first{values.emplace(20)};
    values.emplace(30);
    values.erase(removed);
    auto it{values.begin()};
    test_slot_map::const_iterator converted{it};
    EXPECT_EQ(converted->first, first);
    EXPECT_EQ(&*converted, &*it);
    EXPECT_EQ(it, converted);
    EXPECT_EQ(converted, it);
    EXPECT_EQ(std::ranges::distance(it, values.cend()), 2);
    it->second = 40;
    EXPECT_EQ(converted->second, 40);
    ++it;
    EXPECT_NE(it, converted);
    EXPECT_NE(converted, it);
    ++converted;
    EXPECT_EQ(it, converted);
    ++it;
    ++converted;
    EXPECT_EQ(it, values.cend());
    EXPECT_EQ(converted, values.end());

    const test_slot_map::iterator singular;
    const test_slot_map::const_iterator const_singular{singular};
    EXPECT_EQ(singular, const_singular);
}
} // namespace
