#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include <nori/core/delegate.hpp>

#include "delegate_test_helpers.hpp"

namespace
{
static_assert(!std::is_copy_constructible_v<nori::core::delegate<void()>>);
static_assert(!std::is_copy_assignable_v<nori::core::delegate<void()>>);

TEST(delegate_test, move_construction)
{
    nori::core::delegate<int()> source;
    source.bind([value = std::make_unique<int>(7)]()
    {
        return *value;
    });
    nori::core::delegate<int()> destination{std::move(source)};
    EXPECT_FALSE(source.is_bound());
    EXPECT_EQ(destination.execute(), 7);
    source.bind([]()
    {
        return 3;
    });
    EXPECT_EQ(source.execute(), 3);
}

TEST(delegate_test, move_assignment_reentry)
{
    nori::core::delegate<int()> source;
    source.bind([]()
    {
        return 7;
    });
    nori::core::delegate<int()> destination;
    destination.bind([owned = std::make_unique<destructor_action>([&]()
    {
        EXPECT_FALSE(source.is_bound());
        EXPECT_EQ(destination.execute(), 7);
        destination.bind([]()
        {
            return 9;
        });
    })]()
    {
        return 1;
    });
    destination = std::move(source);
    EXPECT_EQ(destination.execute(), 9);
    source.bind([]()
    {
        return 3;
    });
    EXPECT_EQ(source.execute(), 3);
    auto& alias{destination};
    destination = std::move(alias);
    EXPECT_EQ(destination.execute(), 9);
}

TEST(delegate_test, binding)
{
    nori::core::delegate<int(int)> event;
    EXPECT_FALSE(event.is_bound());
    event.bind(double_value);
    EXPECT_TRUE(event.is_bound());
    EXPECT_EQ(event.execute(3), 6);
    event.bind([](int value)
    {
        return value + 1;
    });
    EXPECT_EQ(event.execute(3), 4);
    event.unbind();
    EXPECT_FALSE(event.is_bound());
}

TEST(delegate_test, member_binding)
{
    callback_target target;
    nori::core::delegate<int(int)> increment;
    increment.bind(&target, &callback_target::increment);
    EXPECT_EQ(increment.execute(3), 3);
    EXPECT_EQ(increment.execute(2), 5);

    const callback_target& constant{target};
    nori::core::delegate<int()> read;
    read.bind(&constant, &callback_target::value);
    EXPECT_EQ(read.execute(), 5);
}

TEST(delegate_test, move_only)
{
    nori::core::delegate<std::unique_ptr<int>(std::unique_ptr<int>)> event;
    event.bind([offset = std::make_unique<int>(2)](std::unique_ptr<int> value) mutable
    {
        *value += (*offset)++;
        return value;
    });
    EXPECT_EQ(*event.execute(std::make_unique<int>(10)), 12);
    EXPECT_EQ(*event.execute(std::make_unique<int>(10)), 13);
}

TEST(delegate_test, references)
{
    nori::core::delegate<int&(int&)> event;
    event.bind([](int& value) -> int&
    {
        return ++value;
    });
    int value{2};
    EXPECT_EQ(&event.execute(value), &value);
    EXPECT_EQ(value, 3);
}

TEST(delegate_test, self_unbind)
{
    nori::core::delegate<void()> event;
    bool runs{false};
    event.bind([&, owned = std::make_unique<destructor_action>([&]()
    {
        runs = true;
    })]()
    {
        event.unbind();
        EXPECT_FALSE(event.is_bound());
        EXPECT_FALSE(runs);
        EXPECT_NE(owned, nullptr);
    });
    event.execute();
    EXPECT_TRUE(runs);
    EXPECT_FALSE(event.is_bound());
}

TEST(delegate_test, nested_rebind)
{
    nori::core::delegate<int()> event;
    bool runs{false};
    event.bind([&, owned = std::make_unique<destructor_action>([&]()
    {
        runs = true;
    })]()
    {
        event.bind([]()
        {
            return 7;
        });
        const int nested{event.execute()};
        EXPECT_FALSE(runs);
        EXPECT_NE(owned, nullptr);
        return nested + 1;
    });
    EXPECT_EQ(event.execute(), 8);
    EXPECT_TRUE(runs);
    EXPECT_EQ(event.execute(), 7);
}

TEST(delegate_test, replace_reentry)
{
    nori::core::delegate<int()> event;
    event.bind([owned = std::make_unique<destructor_action>([&]()
    {
        event.bind([]()
        {
            return 9;
        });
    })]()
    {
        return 1;
    });
    event.bind([]()
    {
        return 2;
    });
    EXPECT_EQ(event.execute(), 9);
}

TEST(delegate_test, unbind_reentry)
{
    nori::core::delegate<int()> event;
    event.bind([owned = std::make_unique<destructor_action>([&]()
    {
        EXPECT_FALSE(event.is_bound());
        event.bind([]()
        {
            return 4;
        });
    })]()
    {
        return 1;
    });
    event.unbind();
    EXPECT_TRUE(event.is_bound());
    EXPECT_EQ(event.execute(), 4);
}
} // namespace
