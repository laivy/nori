#include <concepts>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>

namespace
{
using nori::core::cast;
using nori::core::rtti;
using nori::core::rtti_type;
using nori::core::type_info;

class test_component : public rtti<test_component>
{
public:
    explicit test_component(int value) noexcept :
        value_{value}
    {
    }

    [[nodiscard]] int value() const noexcept
    {
        return value_;
    }

private:
    int value_;
};

class test_scene : public rtti<test_scene, test_component>
{
public:
    using rtti<test_scene, test_component>::rtti;
};

class test_mesh final : public rtti<test_mesh, test_scene>
{
public:
    using rtti<test_mesh, test_scene>::rtti;
};

class test_body final : public rtti<test_body, test_component>
{
public:
    using rtti<test_body, test_component>::rtti;
};

class test_object : public rtti<test_object>
{
};

class test_other_object : public rtti<test_other_object>
{
};

class test_unregistered : public test_scene
{
public:
    using test_scene::test_scene;
};

class test_protected : public rtti<test_protected>
{
protected:
    explicit test_protected(int) noexcept
    {
    }
};

class test_protected_child : public rtti<test_protected_child, test_protected>
{
public:
    explicit test_protected_child(int value) noexcept :
        rtti{value}
    {
    }
};

template <typename T, typename U>
concept can_cast = requires(U* value)
{
    cast<T>(value);
};

static_assert(rtti_type<test_component>);
static_assert(rtti_type<test_scene>);
static_assert(rtti_type<test_mesh>);
static_assert(!rtti_type<test_unregistered>);
static_assert(!rtti_type<int>);
static_assert(!rtti_type<rtti<test_component>>);
static_assert(!std::is_copy_constructible_v<type_info>);
static_assert(!std::is_move_constructible_v<type_info>);
static_assert(!std::is_copy_assignable_v<type_info>);
static_assert(!std::is_move_assignable_v<type_info>);
static_assert(std::has_virtual_destructor_v<test_component>);
static_assert(std::is_nothrow_constructible_v<test_mesh, int>);
static_assert(!std::is_convertible_v<int, test_mesh>);
static_assert(!std::is_constructible_v<rtti<test_protected_child, test_protected>, int>);
static_assert(std::is_nothrow_constructible_v<test_protected_child, int>);
static_assert(can_cast<test_mesh, test_component>);
static_assert(can_cast<test_component, test_mesh>);
static_assert(!can_cast<test_object, test_component>);
static_assert(!can_cast<test_body, test_scene>);
static_assert(!can_cast<test_unregistered, test_component>);
static_assert(can_cast<test_scene, test_unregistered>);
static_assert(!can_cast<test_mesh, volatile test_component>);
static_assert(std::same_as<decltype(cast<test_mesh>(static_cast<const test_component*>(nullptr))), const test_mesh*>);
static_assert(std::same_as<decltype(cast<test_mesh>(static_cast<test_component*>(nullptr))), test_mesh*>);

TEST(rtti_test, type_identity)
{
    test_mesh mesh{42};
    const test_component& value{mesh};

    EXPECT_EQ(value.type(), test_mesh::static_type());
    EXPECT_NE(value.type(), test_scene::static_type());
    EXPECT_NE(test_body::static_type(), test_scene::static_type());
    EXPECT_NE(test_object::static_type(), test_other_object::static_type());
    EXPECT_EQ(test_component::static_type().parent(), nullptr);
    EXPECT_EQ(test_mesh::static_type().parent(), &test_scene::static_type());
    EXPECT_EQ(test_scene::static_type().parent(), &test_component::static_type());
}

TEST(rtti_test, ancestry)
{
    const test_mesh mesh{42};
    const test_component& value{mesh};

    EXPECT_TRUE(value.is_a<test_component>());
    EXPECT_TRUE(value.is_a<test_scene>());
    EXPECT_TRUE(value.is_a<test_mesh>());
    EXPECT_FALSE(value.is_a<test_body>());
    EXPECT_FALSE(value.is_a<test_object>());
    EXPECT_TRUE(value.type().is_a(test_component::static_type()));
    EXPECT_FALSE(test_component::static_type().is_a(test_mesh::static_type()));
}

TEST(rtti_test, pointer_cast)
{
    test_mesh mesh{42};
    test_component* value{&mesh};
    const test_component* const_value{value};

    EXPECT_EQ(cast<test_mesh>(value), &mesh);
    EXPECT_EQ(cast<test_scene>(value), static_cast<test_scene*>(&mesh));
    EXPECT_EQ(cast<test_component>(&mesh), value);
    EXPECT_EQ(cast<test_component>(value), value);
    EXPECT_EQ(cast<test_body>(value), nullptr);
    EXPECT_EQ(cast<test_mesh>(const_value), &mesh);
    EXPECT_EQ(cast<test_component>(static_cast<const test_mesh*>(&mesh)), const_value);
    EXPECT_EQ(cast<test_mesh>(static_cast<test_component*>(nullptr)), nullptr);
    EXPECT_EQ(cast<test_mesh>(static_cast<const test_component*>(nullptr)), nullptr);
    EXPECT_EQ(cast<test_component>(static_cast<test_mesh*>(nullptr)), nullptr);
    EXPECT_EQ(cast<test_mesh>(nullptr), nullptr);
}

TEST(rtti_test, unregistered_derived_type)
{
    test_unregistered instance{42};
    test_component* value{&instance};

    EXPECT_EQ(value->type(), test_scene::static_type());
    EXPECT_EQ(cast<test_scene>(&instance), static_cast<test_scene*>(&instance));
    EXPECT_EQ(cast<test_scene>(value), static_cast<test_scene*>(&instance));
    EXPECT_EQ(cast<test_mesh>(value), nullptr);
}

TEST(rtti_test, inherited_constructor)
{
    const test_mesh mesh{42};
    const test_mesh copy{mesh};
    const test_protected_child child{1};
    const std::unique_ptr<test_component> owner{std::make_unique<test_mesh>(7)};

    EXPECT_EQ(copy.value(), 42);
    EXPECT_EQ(child.type(), test_protected_child::static_type());
    EXPECT_EQ(owner->value(), 7);
    EXPECT_TRUE(owner->is_a<test_mesh>());
}
}
