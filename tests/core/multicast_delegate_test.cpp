#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/multicast_delegate.hpp>

#include "delegate_test_helpers.hpp"

namespace
{
static_assert(!std::is_copy_constructible_v<nori::core::multicast_delegate<void()>>);
static_assert(!std::is_copy_assignable_v<nori::core::multicast_delegate<void()>>);

TEST(multicast_delegate_test, move_construction)
{
    nori::core::multicast_delegate<void()> source;
    int calls{0};
    const auto handle{source.add([&, value = std::make_unique<int>(2)]()
    {
        calls += *value;
    })};
    nori::core::multicast_delegate<void()> destination{std::move(source)};
    EXPECT_FALSE(source.is_bound());
    destination.broadcast();
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(destination.remove(handle));
    std::ignore = source.add([&]()
    {
        ++calls;
    });
    source.broadcast();
    EXPECT_EQ(calls, 3);
}

TEST(multicast_delegate_test, move_assignment_reentry)
{
    nori::core::multicast_delegate<void()> source;
    int calls{0};
    const auto handle{source.add([&]()
    {
        ++calls;
    })};
    nori::core::multicast_delegate<void()> destination;
    std::ignore = destination.add([owned = std::make_unique<destructor_action>([&]()
    {
        EXPECT_FALSE(source.is_bound());
        destination.broadcast();
        std::ignore = destination.add([&]()
        {
            calls += 10;
        });
    })]()
    {
    });
    destination = std::move(source);
    EXPECT_EQ(calls, 1);
    auto& alias{destination};
    destination = std::move(alias);
    destination.broadcast();
    EXPECT_EQ(calls, 12);
    EXPECT_TRUE(destination.remove(handle));
    std::ignore = source.add([&]()
    {
        calls += 100;
    });
    source.broadcast();
    EXPECT_EQ(calls, 112);
}

TEST(multicast_delegate_test, slot_reuse)
{
    nori::core::multicast_delegate<void()> event;
    EXPECT_FALSE(event.is_bound());
    event.broadcast();
    int calls{0};
    const auto first{event.add([&]()
    {
        ++calls;
    })};
    EXPECT_TRUE(event.is_bound());
    event.broadcast();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(event.remove(first));
    EXPECT_FALSE(event.is_bound());
    const auto replacement{event.add([&]()
    {
        ++calls;
    })};
    EXPECT_EQ(first.index, replacement.index);
    EXPECT_NE(first.generation, replacement.generation);
    event.broadcast();
    EXPECT_EQ(calls, 2);
}

TEST(multicast_delegate_test, removal)
{
    nori::core::multicast_delegate<void()> event;
    int first_calls{0};
    int second_calls{0};
    int third_calls{0};
    const auto first{event.add([&]()
    {
        ++first_calls;
    })};
    const auto second{event.add([&]()
    {
        ++second_calls;
    })};
    const auto third{event.add([&]()
    {
        ++third_calls;
    })};
    EXPECT_TRUE(event.remove(second));
    event.broadcast();
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0);
    EXPECT_EQ(third_calls, 1);
    EXPECT_TRUE(event.remove(first));
    event.broadcast();
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0);
    EXPECT_EQ(third_calls, 2);
    EXPECT_TRUE(event.remove(third));
    EXPECT_FALSE(event.is_bound());
    event.broadcast();
    EXPECT_EQ(third_calls, 2);
}

TEST(multicast_delegate_test, member_binding)
{
    callback_target target;
    nori::core::multicast_delegate<void(int)> event;
    std::ignore = event.add(double_value);
    std::ignore = event.add(&target, &callback_target::increment);
    event.broadcast(3);
    EXPECT_EQ(target.value(), 3);

    const callback_target& constant{target};
    nori::core::multicast_delegate<void()> read;
    std::ignore = read.add(&constant, &callback_target::value);
    read.broadcast();
}

TEST(multicast_delegate_test, move_only)
{
    nori::core::multicast_delegate<void()> event;
    int observed{0};
    std::ignore = event.add([&, value = std::make_unique<int>(0)]() mutable
    {
        observed = ++*value;
    });
    event.broadcast();
    EXPECT_EQ(observed, 1);
    event.broadcast();
    EXPECT_EQ(observed, 2);
}

TEST(multicast_delegate_test, value_copies)
{
    nori::core::multicast_delegate<void(std::string)> event;
    std::vector<std::string> observed;
    std::ignore = event.add([&](std::string value)
    {
        observed.push_back(std::move(value));
    });
    std::ignore = event.add([&](std::string&& value)
    {
        observed.push_back(std::move(value));
    });
    event.broadcast("same value");
    ASSERT_EQ(observed.size(), 2);
    EXPECT_EQ(observed[0], "same value");
    EXPECT_EQ(observed[1], "same value");
}

TEST(multicast_delegate_test, references)
{
    nori::core::multicast_delegate<void(const std::unique_ptr<int>&, int&)> event;
    int calls{0};
    const auto value{std::make_unique<int>(7)};
    int result{0};
    for (int i{0}; i < 2; ++i)
    {
        std::ignore = event.add([&](const std::unique_ptr<int>& argument, int& output)
        {
            EXPECT_EQ(&argument, &value);
            output += *argument;
            ++calls;
        });
    }
    event.broadcast(value, result);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(result, 14);
}

TEST(multicast_delegate_test, self_removal)
{
    nori::core::multicast_delegate<void()> event;
    nori::core::multicast_delegate<void()>::handle listener{};
    bool runs{false};
    int calls{0};
    listener = event.add([&, owned = std::make_unique<destructor_action>([&]()
    {
        runs = true;
    })]()
    {
        ++calls;
        EXPECT_TRUE(event.remove(listener));
        EXPECT_FALSE(event.is_bound());
        event.broadcast();
        EXPECT_FALSE(runs);
        EXPECT_NE(owned, nullptr);
    });
    event.broadcast();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(runs);
    EXPECT_FALSE(event.is_bound());
}

TEST(multicast_delegate_test, nested_removal)
{
    nori::core::multicast_delegate<void()> event;
    nori::core::multicast_delegate<void()>::handle removed{};
    bool reenters{false};
    int removed_calls{0};
    int removed_calls_at_removal{0};
    int remaining_calls{0};
    std::ignore = event.add([&]()
    {
        if (!reenters)
        {
            EXPECT_TRUE(event.remove(removed));
            removed_calls_at_removal = removed_calls;
            reenters = true;
            event.broadcast();
        }
    });
    removed = event.add([&]()
    {
        ++removed_calls;
    });
    std::ignore = event.add([&]()
    {
        ++remaining_calls;
    });
    event.broadcast();
    EXPECT_EQ(removed_calls, removed_calls_at_removal);
    EXPECT_EQ(remaining_calls, 2);
    event.broadcast();
    EXPECT_EQ(removed_calls, removed_calls_at_removal);
    EXPECT_EQ(remaining_calls, 3);
}

TEST(multicast_delegate_test, deferred_addition)
{
    nori::core::multicast_delegate<void()> event;
    bool adds_listeners{false};
    int added_calls{0};
    int original_calls{0};
    std::ignore = event.add([&, value = std::make_unique<int>(0)]() mutable
    {
        ++original_calls;
        ++*value;
        if (!adds_listeners)
        {
            adds_listeners = true;
            for (int i{0}; i < 4096; ++i)
            {
                std::ignore = event.add([&]()
                {
                    ++added_calls;
                });
            }
            event.broadcast();
            EXPECT_EQ(added_calls, 0);
            EXPECT_EQ(*value, 2);
        }
    });
    event.broadcast();
    EXPECT_EQ(original_calls, 2);
    EXPECT_EQ(added_calls, 0);
    event.broadcast();
    EXPECT_EQ(original_calls, 3);
    EXPECT_EQ(added_calls, 4096);
}

TEST(multicast_delegate_test, pending_removal)
{
    nori::core::multicast_delegate<void()> event;
    int added_calls{0};
    std::ignore = event.add([&]()
    {
        const auto listener{event.add([&]()
        {
            ++added_calls;
        })};
        EXPECT_TRUE(event.remove(listener));
    });
    event.broadcast();
    event.broadcast();
    EXPECT_EQ(added_calls, 0);
}

TEST(multicast_delegate_test, independent_instances)
{
    nori::core::multicast_delegate<void()> first;
    nori::core::multicast_delegate<void()> second;
    int calls{0};
    std::ignore = first.add([&]()
    {
        std::ignore = second.add([&]()
        {
            ++calls;
        });
        second.broadcast();
    });
    first.broadcast();
    EXPECT_EQ(calls, 1);
}

TEST(multicast_delegate_test, removal_reentry)
{
    nori::core::multicast_delegate<void()> event;
    nori::core::multicast_delegate<void()>::handle replacement{};
    int calls{0};
    const auto listener{event.add([owned = std::make_unique<destructor_action>([&]()
    {
        replacement = event.add([&]()
        {
            ++calls;
        });
    })]()
    {
    })};
    EXPECT_TRUE(event.remove(listener));
    EXPECT_EQ(replacement.index, listener.index);
    EXPECT_NE(replacement.generation, listener.generation);
    event.broadcast();
    EXPECT_EQ(calls, 1);
}

TEST(multicast_delegate_test, deferred_reentry)
{
    nori::core::multicast_delegate<void()> event;
    nori::core::multicast_delegate<void()>::handle self{};
    nori::core::multicast_delegate<void()>::handle other{};
    int removed_calls{0};
    int added_calls{0};
    self = event.add([&, owned = std::make_unique<destructor_action>([&]()
    {
        EXPECT_TRUE(event.remove(other));
        std::ignore = event.add([&]()
        {
            ++added_calls;
        });
        event.broadcast();
    })]()
    {
        EXPECT_TRUE(event.remove(self));
    });
    other = event.add([&]()
    {
        ++removed_calls;
    });
    event.broadcast();
    EXPECT_EQ(removed_calls, 1);
    EXPECT_EQ(added_calls, 1);
    EXPECT_TRUE(event.is_bound());
    event.broadcast();
    EXPECT_EQ(added_calls, 2);
}
} // namespace
