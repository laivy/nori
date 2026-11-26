#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
TEST_F(lifecycle_test, initialization_failure)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-resource-initialization-failure"};
    const std::filesystem::path unresolvable{workdir.path() / std::string(40000, 'a')};
    EXPECT_FALSE(nori::resource::initialize(unresolvable));
    ASSERT_TRUE(nori::resource::initialize(workdir.path()));
    const nori::resource::handle document{nori::resource::create_root()};
    EXPECT_TRUE(children_of_or_fail(document).empty());
}

TEST_F(lifecycle_test, reinitialization)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-resource-reinitialization"};
    ASSERT_TRUE(nori::resource::initialize(workdir.path()));
    const nori::resource::handle old_handle{create_resource("old.nori")};
    const nori::resource::handle old_child{create_resource("old.nori::child")};
    ASSERT_TRUE(old_handle);
    ASSERT_TRUE(old_child);
    nori::resource::shutdown();

    ASSERT_TRUE(nori::resource::initialize(workdir.path()));
    const nori::resource::handle new_document{nori::resource::create_root()};
    EXPECT_TRUE(children_of_or_fail(new_document).empty());
}

TEST_F(lifecycle_test, scalar_round_trip)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-resource-round-trip"};
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "round-trip.nori"};
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const nori::resource::handle root{create_resource(root_path)};
        const nori::resource::handle value{create_resource(root_path + "::message")};
        const nori::resource::handle yes{create_resource(root_path + "::yes")};
        const nori::resource::handle no{create_resource(root_path + "::no")};
        nori::resource::set(value, std::string{"hello"});
        nori::resource::set(yes, true);
        nori::resource::set(no, false);
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const nori::resource::handle value{nori::resource::get(root_path + "::message")};
        ASSERT_TRUE(value);
        EXPECT_EQ(nori::resource::get<std::string>(value), "hello");
        EXPECT_TRUE(nori::resource::get<bool>(root_path + "::yes"));
        EXPECT_FALSE(nori::resource::get<bool>(root_path + "::no"));
        const auto missing{nori::resource::try_get(root_path + "::missing")};
        ASSERT_FALSE(missing);
        EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);
        ASSERT_TRUE(std::filesystem::remove(file));
        const auto cached{nori::resource::try_get(root_path + "::message")};
        ASSERT_TRUE(cached);
        EXPECT_EQ(*cached, value);
        nori::resource::shutdown();
    }
}

TEST_F(lifecycle_test, utf8_round_trip)
{
    const scoped_temp_dir workdir{
        std::filesystem::temp_directory_path() /
        nori::core::utf8_to_path("nori-한글-😀")
    };
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{
        directory / nori::core::utf8_to_path("자료-e\u0301.nori")
    };
    const std::string root_path{nori::core::path_to_utf8(file)};
    const std::string value_path{root_path + "::한글/메시지😀"};
    constexpr std::string_view expected{"안녕하세요 e\u0301 😀"};
    {
        ASSERT_TRUE(nori::resource::initialize());
        const nori::resource::handle root{create_resource(root_path)};
        const nori::resource::handle value{create_resource(value_path)};
        nori::resource::set(value, std::string{expected});
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const auto value{nori::resource::try_get<std::string>(value_path)};
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, expected);
        nori::resource::shutdown();
    }
}

TEST_F(lifecycle_test, file_boundary)
{
    const scoped_temp_dir workdir{
        std::filesystem::temp_directory_path() / "nori-file-boundary-round-trip"
    };
    const std::filesystem::path& base{workdir.path()};
    const std::filesystem::path directory{base / "directory.nori"};
    const std::filesystem::path file{directory / "test.nori"};
    const std::string root_path{nori::core::path_to_utf8(file)};
    const std::string value_path{root_path + "::hello.value/deep.value"};
    std::filesystem::create_directories(directory);
    {
        ASSERT_TRUE(nori::resource::initialize());
        const nori::resource::handle root{create_resource(root_path)};
        const nori::resource::handle value{create_resource(value_path)};
        nori::resource::set(value, std::string{"boundary"});
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const auto value{nori::resource::try_get<std::string>(value_path)};
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, "boundary");
        nori::resource::shutdown();
    }
}

TEST_F(lifecycle_test, int32_encoding)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-format-bytes"};
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "value.nori"};
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const auto root{create_resource(root_path)};
        const auto value{create_resource(root_path + "::value")};
        nori::resource::set(value, std::int32_t{0x12345678});
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }

    std::ifstream in{file, std::ios::binary};
    const std::vector<std::uint8_t> actual{
        std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}
    };
    const std::vector<std::uint8_t> expected{
        'n', 'o', 'r', 'i', 1, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 0, 0,
        0x38, 0, 0, 0, 0, 0, 0, 0,
        0x54, 0, 0, 0, 0, 0, 0, 0,
        7, 0, 0, 0, 0, 0, 0, 0,
        0x5B, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        2, 0, 0, 0,
        0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        5, 0, 'v', 'a', 'l', 'u', 'e'
    };
    EXPECT_EQ(actual, expected);
}

TEST_F(lifecycle_test, float32_zero)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-float32-zero-format-bytes"};
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "value.nori"};
    const std::string root_path{nori::core::path_to_utf8(file)};
    {
        ASSERT_TRUE(nori::resource::initialize());
        const auto root{create_resource(root_path)};
        const auto value{create_resource(root_path + "::value")};
        nori::resource::set(value, -0.0f);
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }

    std::ifstream in{file, std::ios::binary};
    const std::vector<std::uint8_t> actual{
        std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}
    };
    const std::vector<std::uint8_t> expected{
        'n', 'o', 'r', 'i', 1, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 0, 0,
        0x38, 0, 0, 0, 0, 0, 0, 0,
        0x54, 0, 0, 0, 0, 0, 0, 0,
        7, 0, 0, 0, 0, 0, 0, 0,
        0x5B, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        4, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        5, 0, 'v', 'a', 'l', 'u', 'e'
    };
    EXPECT_EQ(actual, expected);

    ASSERT_TRUE(nori::resource::initialize());
    const auto value{nori::resource::try_get<float>(root_path + "::value")};
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 0.0f);
    EXPECT_FALSE(std::signbit(*value));
    nori::resource::shutdown();
}

TEST_F(lifecycle_test, image_round_trip)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-image-round-trip"};
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "image.nori"};
    const std::array pixels{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}, std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80}
    };
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const nori::resource::handle root{create_resource(root_path)};
        const nori::resource::handle value{create_resource(root_path + "::pixels")};
        const nori::resource::image asset{
            .width = 2,
            .height = 1,
            .mip_levels = 1,
            .data = {pixels.begin(), pixels.end()}
        };
        nori::resource::set(value, asset);

        const nori::resource::image_view first{nori::resource::get<nori::resource::image_view>(value)};
        const nori::resource::image_view second{nori::resource::get<nori::resource::image_view>(value)};
        EXPECT_EQ(first.data.data(), second.data.data());
        EXPECT_TRUE(std::ranges::equal(first.data, pixels));
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const nori::resource::handle value{
            nori::resource::get(nori::core::path_to_utf8(file) + "::pixels")
        };
        ASSERT_TRUE(value);
        const nori::resource::image_view image{nori::resource::get<nori::resource::image_view>(value)};
        EXPECT_EQ(image.format, nori::resource::image_format::rgba8_unorm);
        EXPECT_EQ(image.width, 2);
        EXPECT_EQ(image.height, 1);
        EXPECT_EQ(image.mip_levels, 1);
        EXPECT_TRUE(std::ranges::equal(image.data, pixels));
        nori::resource::shutdown();
    }
}

TEST_F(lifecycle_test, compressed_image_round_trip)
{
    const scoped_temp_dir workdir{
        std::filesystem::temp_directory_path() / "nori-bc-image-round-trip"
    };
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "image.nori"};
    std::vector<std::byte> blocks(112, std::byte{0x5A});
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const auto root{create_resource(root_path)};
        const auto value{create_resource(root_path + "::texture")};
        nori::resource::set(value, nori::resource::image{.format = nori::resource::image_format::bc7_unorm_srgb, .width = 8, .height = 8, .mip_levels = 4, .data = blocks});
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const auto image{nori::resource::get<nori::resource::image_view>(
            nori::core::path_to_utf8(file) + "::texture"
        )};
        EXPECT_EQ(image.format, nori::resource::image_format::bc7_unorm_srgb);
        EXPECT_EQ(image.width, 8u);
        EXPECT_EQ(image.height, 8u);
        EXPECT_EQ(image.mip_levels, 4u);
        EXPECT_TRUE(std::ranges::equal(image.data, blocks));
        nori::resource::shutdown();
    }
}

TEST_F(lifecycle_test, value_lifetime)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-resource-value-lifetime"};
    ASSERT_TRUE(nori::resource::initialize(workdir.path()));
    const nori::resource::image expected_image{
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}}
    };
    const nori::resource::binary expected_binary{
        .data = {std::byte{0x05}, std::byte{0x06}, std::byte{0x07}}
    };
    const nori::resource::handle image{create_resource("owning-shutdown.nori::image")};
    const nori::resource::handle binary{create_resource("owning-shutdown.nori::binary")};
    nori::resource::set(image, expected_image);
    nori::resource::set(binary, expected_binary);

    const nori::resource::image image_copy{nori::resource::get<nori::resource::image>(image)};
    const nori::resource::binary binary_copy{nori::resource::get<nori::resource::binary>(binary)};
    nori::resource::shutdown();

    EXPECT_EQ(image_copy, expected_image);
    EXPECT_EQ(binary_copy, expected_binary);
}

TEST_F(lifecycle_test, binary_round_trip)
{
    const scoped_temp_dir workdir{std::filesystem::temp_directory_path() / "nori-binary-round-trip"};
    const std::filesystem::path& directory{workdir.path()};
    const std::filesystem::path file{directory / "shaders.nori"};
    const std::array bytes{
        std::byte{0x44}, std::byte{0x58}, std::byte{0x49}, std::byte{0x4C}, std::byte{0x00}, std::byte{0x7F}, std::byte{0x80}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x10}, std::byte{0x20}
    };
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const nori::resource::handle root{create_resource(root_path)};
        const nori::resource::handle shader{create_resource(root_path + "::vertex")};
        const nori::resource::handle empty{create_resource(root_path + "::empty")};
        nori::resource::set(shader, nori::resource::binary{.data = {bytes.begin(), bytes.end()}});
        nori::resource::set(empty, nori::resource::binary{});
        ASSERT_TRUE(nori::resource::save_as(root, file));
        nori::resource::shutdown();
    }
    {
        ASSERT_TRUE(nori::resource::initialize());
        const std::string root_path{nori::core::path_to_utf8(file)};
        const nori::resource::binary_view shader{
            nori::resource::get<nori::resource::binary_view>(root_path + "::vertex")
        };
        EXPECT_TRUE(std::ranges::equal(shader.data, bytes));
        EXPECT_TRUE(nori::resource::get<nori::resource::binary_view>(root_path + "::empty").data.empty());
        nori::resource::shutdown();
    }
}
} // namespace
