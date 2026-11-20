#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
TEST_F(value_test, binary_view_equality)
{
    const std::array bytes{std::byte{0x10}, std::byte{0x20}};
    const auto copy{bytes};
    const std::array different{std::byte{0x10}, std::byte{0x30}};
    const nori::resource::binary_view value{bytes};

    EXPECT_EQ(value, (nori::resource::binary_view{copy}));
    EXPECT_NE(value, nori::resource::binary_view{different});
    EXPECT_NE(value, nori::resource::binary_view{std::span{bytes}.first(1)});
    EXPECT_NE(value, nori::resource::binary_view{});
    EXPECT_EQ(nori::resource::binary_view{}, (nori::resource::binary_view{std::span{bytes}.first(0)}));
}

TEST_F(value_test, image_view_equality)
{
    const std::array bytes{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}};
    const auto copy{bytes};
    const std::array different{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x50}};
    const nori::resource::image_view value{
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = bytes
    };
    auto other{value};
    other.data = copy;
    EXPECT_EQ(value, other);

    other.data = different;
    EXPECT_NE(value, other);
    other.data = std::span{bytes}.first(3);
    EXPECT_NE(value, other);

    other = value;
    other.format = nori::resource::image_format::bc7_unorm;
    EXPECT_NE(value, other);
    other = value;
    other.width = 2;
    EXPECT_NE(value, other);
    other = value;
    other.height = 2;
    EXPECT_NE(value, other);
    other = value;
    other.mip_levels = 2;
    EXPECT_NE(value, other);

    other = value;
    other.data = {};
    EXPECT_NE(value, other);
    auto empty{value};
    empty.data = std::span{bytes}.first(0);
    EXPECT_EQ(empty, other);
}

TEST_F(value_test, query_contracts)
{
    const nori::resource::handle root{create_resource("values.nori")};
    const nori::resource::handle value{create_resource("values.nori::count")};
    ASSERT_TRUE(root);
    ASSERT_TRUE(value);

    nori::resource::set(value, std::int32_t{42});

    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);
    EXPECT_EQ(nori::resource::get<std::int32_t>("values.nori::count"), 42);
    EXPECT_EQ(nori::resource::get<std::int32_t>(root, "count"), 42);
    EXPECT_EQ(nori::resource::get("values.nori::count"), value);
    EXPECT_EQ(nori::resource::get(root, "count"), value);
    EXPECT_EQ(*nori::resource::try_get<std::int32_t>(value), 42);
    EXPECT_EQ(*nori::resource::try_get<std::int32_t>("values.nori::count"), 42);
    EXPECT_EQ(*nori::resource::try_get<std::int32_t>(root, "count"), 42);

    const auto missing{nori::resource::try_get<std::int32_t>("values.nori::missing")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);
    const auto relative_missing{nori::resource::try_get<std::int32_t>(root, "missing")};
    ASSERT_FALSE(relative_missing);
    EXPECT_EQ(relative_missing.error(), nori::resource::error_code::not_found);
    const auto type_mismatch{nori::resource::try_get<std::string>(value)};
    ASSERT_FALSE(type_mismatch);
    EXPECT_EQ(type_mismatch.error(), nori::resource::error_code::type_mismatch);
    EXPECT_FALSE(nori::resource::parent_of(root));
    EXPECT_TRUE(children_of_or_fail(value).empty());
}

TEST_F(value_test, type_mapping)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle empty_value{nori::resource::create(root, "empty", nori::resource::empty)};
    ASSERT_TRUE(empty_value);
    EXPECT_EQ(nori::resource::type_of(empty_value), nori::resource::property_type::empty);

    const nori::resource::handle boolean_value{nori::resource::create(root, "boolean", true)};
    ASSERT_TRUE(boolean_value);
    EXPECT_EQ(nori::resource::type_of(boolean_value), nori::resource::property_type::boolean);

    const nori::resource::handle int32_value{nori::resource::create(root, "int32", std::int32_t{1})};
    ASSERT_TRUE(int32_value);
    EXPECT_EQ(nori::resource::type_of(int32_value), nori::resource::property_type::int32);

    const nori::resource::handle int64_value{nori::resource::create(root, "int64", std::int64_t{2})};
    ASSERT_TRUE(int64_value);
    EXPECT_EQ(nori::resource::type_of(int64_value), nori::resource::property_type::int64);

    const nori::resource::handle float_value{nori::resource::create(root, "float", 0.5f)};
    ASSERT_TRUE(float_value);
    EXPECT_EQ(nori::resource::type_of(float_value), nori::resource::property_type::float32);

    const nori::resource::handle string_value{nori::resource::create(root, "string", "text")};
    ASSERT_TRUE(string_value);
    EXPECT_EQ(nori::resource::type_of(string_value), nori::resource::property_type::string);
    EXPECT_EQ(nori::resource::get<std::string>(string_value), "text");

    const nori::resource::handle image_value{nori::resource::create(root, "image", nori::resource::image{.width = 1, .height = 1, .mip_levels = 1, .data = {std::byte{0x01}}})};
    ASSERT_TRUE(image_value);
    EXPECT_EQ(nori::resource::type_of(image_value), nori::resource::property_type::image);

    const nori::resource::handle binary_value{nori::resource::create(root, "binary", nori::resource::binary{.data = {std::byte{0x02}}})};
    ASSERT_TRUE(binary_value);
    EXPECT_EQ(nori::resource::type_of(binary_value), nori::resource::property_type::binary);
}

TEST_F(value_test, set)
{
    const nori::resource::handle value{create_resource("values.nori::value")};
    nori::resource::set(value, std::int32_t{42});
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);

    nori::resource::set(value, std::int64_t{43});
    EXPECT_EQ(nori::resource::type_of(value), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::get<std::int64_t>(value), 43);
}

TEST_F(value_test, empty_value)
{
    const nori::resource::handle value{create_resource("tree.nori::value")};
    const nori::resource::handle child{create_resource("tree.nori::value/child")};
    nori::resource::set(value, std::int32_t{42});
    nori::resource::set(child, std::string{"preserved"});

    nori::resource::set(value, nori::resource::empty);

    EXPECT_EQ(nori::resource::type_of(value), nori::resource::property_type::empty);
    ASSERT_EQ(children_of_or_fail(value).size(), 1u);
    EXPECT_EQ(children_of_or_fail(value).front(), child);
    EXPECT_EQ(nori::resource::parent_of(child), value);
    EXPECT_EQ(nori::resource::get<std::string>(child), "preserved");

    const auto saved{
        nori::resource::save_as(nori::resource::get("tree.nori"), directory_ / "tree.nori")
    };
    ASSERT_TRUE(saved);
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto reloaded_value{nori::resource::try_get("tree.nori::value")};
    const auto reloaded_child{nori::resource::try_get("tree.nori::value/child")};
    ASSERT_TRUE(reloaded_value);
    ASSERT_TRUE(reloaded_child);
    EXPECT_EQ(nori::resource::type_of(*reloaded_value), nori::resource::property_type::empty);
    EXPECT_EQ(nori::resource::parent_of(*reloaded_child), *reloaded_value);
    EXPECT_EQ(nori::resource::get<std::string>(*reloaded_child), "preserved");
}

TEST_F(value_test, invalid_image_save_preserves_file_and_runtime_value)
{
    const auto root{create_resource("arbitrary-image.nori")};
    const auto value{create_resource("arbitrary-image.nori::texture")};
    ASSERT_TRUE(nori::resource::save(root));
    std::ifstream input{directory_ / "arbitrary-image.nori", std::ios::binary};
    const std::vector<char> before{std::istreambuf_iterator<char>{input}, {}};
    input.close();
    nori::resource::set(value, nori::resource::image{.format = nori::resource::image_format::bc5_unorm, .width = 8, .height = 8, .mip_levels = 4, .data = std::vector<std::byte>(111)});
    const auto saved{nori::resource::save(root)};
    ASSERT_FALSE(saved);
    EXPECT_EQ(saved.error(), nori::resource::error_code::invalid_format);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(value).data.size(), 111u);
    std::ifstream unchanged{directory_ / "arbitrary-image.nori", std::ios::binary};
    EXPECT_EQ((std::vector<char>{std::istreambuf_iterator<char>{unchanged}, {}}), before);
}

TEST_F(value_test, binary_views)
{
    const std::array bytes{
        std::byte{0x44}, std::byte{0x58}, std::byte{0x49}, std::byte{0x4C}, std::byte{0x00}, std::byte{0xFF}, std::byte{0x80}, std::byte{0x01}
    };
    const nori::resource::handle value{create_resource("binary.nori::shader")};
    nori::resource::set(value, nori::resource::binary{.data = {bytes.begin(), bytes.end()}});

    const nori::resource::binary_view first{nori::resource::get<nori::resource::binary_view>(value)};
    const nori::resource::binary_view second{nori::resource::get<nori::resource::binary_view>(value)};
    EXPECT_EQ(first.data.data(), second.data.data());
    EXPECT_TRUE(std::ranges::equal(first.data, bytes));
    EXPECT_EQ(nori::resource::type_of(value), nori::resource::property_type::binary);
    EXPECT_FALSE(nori::resource::try_get<nori::resource::image_view>(value));

    const nori::resource::handle empty{create_resource("binary.nori::empty")};
    nori::resource::set(empty, nori::resource::binary{});
    EXPECT_TRUE(nori::resource::get<nori::resource::binary_view>(empty).data.empty());
}

TEST_F(value_test, owning_copies)
{
    const std::array pixels{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
        std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80}
    };
    const std::array bytes{
        std::byte{0x44}, std::byte{0x58}, std::byte{0x49}, std::byte{0x4C},
        std::byte{0x00}, std::byte{0xFF}, std::byte{0x80}, std::byte{0x01}
    };
    const nori::resource::image expected_image{
        .width = 2,
        .height = 1,
        .mip_levels = 1,
        .data = {pixels.begin(), pixels.end()}
    };
    const nori::resource::binary expected_binary{.data = {bytes.begin(), bytes.end()}};
    const nori::resource::handle root{create_resource("owning-values.nori")};
    const nori::resource::handle image{create_resource("owning-values.nori::image")};
    const nori::resource::handle binary{create_resource("owning-values.nori::binary")};
    nori::resource::set(image, expected_image);
    nori::resource::set(binary, expected_binary);

    const nori::resource::image handle_image{nori::resource::get<nori::resource::image>(image)};
    const auto path_image{nori::resource::try_get<nori::resource::image>("owning-values.nori::image")};
    const nori::resource::image parent_image{
        nori::resource::get<nori::resource::image>(root, "image")
    };
    const auto handle_binary{nori::resource::try_get<nori::resource::binary>(binary)};
    const nori::resource::binary path_binary{
        nori::resource::get<nori::resource::binary>("owning-values.nori::binary")
    };
    const auto parent_binary{
        nori::resource::try_get<nori::resource::binary>(root, "binary")
    };

    ASSERT_TRUE(path_image);
    ASSERT_TRUE(handle_binary);
    ASSERT_TRUE(parent_binary);
    EXPECT_EQ(handle_image, expected_image);
    EXPECT_EQ(*path_image, expected_image);
    EXPECT_EQ(parent_image, expected_image);
    EXPECT_EQ(*handle_binary, expected_binary);
    EXPECT_EQ(path_binary, expected_binary);
    EXPECT_EQ(*parent_binary, expected_binary);

    const auto wrong_binary{nori::resource::try_get<nori::resource::binary>(image)};
    const auto wrong_image{nori::resource::try_get<nori::resource::image>(binary)};
    ASSERT_FALSE(wrong_binary);
    ASSERT_FALSE(wrong_image);
    EXPECT_EQ(wrong_binary.error(), nori::resource::error_code::type_mismatch);
    EXPECT_EQ(wrong_image.error(), nori::resource::error_code::type_mismatch);

    const nori::resource::image replacement{
        .format = nori::resource::image_format::bc5_unorm,
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = std::vector<std::byte>(16, std::byte{0xAA})
    };
    nori::resource::set(image, replacement);
    EXPECT_EQ(handle_image, expected_image);
    EXPECT_EQ(*path_image, expected_image);
    EXPECT_EQ(parent_image, expected_image);

    ASSERT_TRUE(nori::resource::save(root));
    nori::resource::remove(binary);
    EXPECT_EQ(*handle_binary, expected_binary);
    EXPECT_EQ(path_binary, expected_binary);
    EXPECT_EQ(*parent_binary, expected_binary);

    nori::resource::remove(image);
    EXPECT_EQ(handle_image, expected_image);
    EXPECT_EQ(*path_image, expected_image);
    EXPECT_EQ(parent_image, expected_image);
}
} // namespace
