#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

#include "codec_test_helpers.hpp"

namespace
{
class codec_test : public resource_test
{
protected:
    nori::resource::result<nori::resource::handle> reopen(const std::filesystem::path& file)
    {
        nori::resource::shutdown();
        if (!nori::resource::initialize(directory_))
        {
            ADD_FAILURE() << "resource initialization failed";
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        return nori::resource::try_get(nori::resource::path{file});
    }
};

TEST_F(codec_test, version_one_layout_matches_worked_example)
{
    const auto file{directory_ / "layout.nori"};
    const std::array properties{
        fixture_property{0, "hp", std::int32_t{100}},
        fixture_property{0, "name", std::string{"hero"}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    std::ostringstream expected;
    expected.write("nori", 4);
    write_scalar(expected, std::uint32_t{1});
    write_scalar(expected, std::uint32_t{2});
    write_scalar(expected, std::uint32_t{2});
    write_scalar(expected, std::uint64_t{56});
    write_scalar(expected, std::uint64_t{112});
    write_scalar(expected, std::uint64_t{10});
    write_scalar(expected, std::uint64_t{122});
    write_scalar(expected, std::uint64_t{4});
    write_scalar(expected, std::uint32_t{0});
    write_scalar(expected, std::uint32_t{0});
    write_scalar(expected, std::uint32_t{2});
    write_scalar(expected, std::int32_t{100});
    write_scalar(expected, std::uint32_t{0});
    write_scalar(expected, std::uint64_t{0});
    write_scalar(expected, std::uint32_t{1});
    write_scalar(expected, std::uint32_t{1});
    write_scalar(expected, std::uint32_t{5});
    write_scalar(expected, std::uint64_t{0});
    write_scalar(expected, std::uint64_t{4});
    write_scalar(expected, std::uint16_t{2});
    expected.write("hp", 2);
    write_scalar(expected, std::uint16_t{4});
    expected.write("namehero", 8);
    const auto actual{file_bytes(file)};
    EXPECT_EQ(std::string(actual.begin(), actual.end()), expected.str());
}

TEST_F(codec_test, version_one_topology)
{
    constexpr std::array records{
        fixture_record{"branch", 3, 10},
        fixture_record{"leaf", 1, 11},
        fixture_record{"nested", 3, 12},
        fixture_record{"inside", 3, 13},
        fixture_record{"other", 5, 14},
        fixture_record{"leaf", 5, 15},
    };
    const std::filesystem::path file{directory_ / "known.nori"};
    write_fixture_file(file, records);
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    const auto root{*input};
    const auto top_level{children_of_or_fail(root)};
    ASSERT_EQ(top_level.size(), 2);
    const auto branch_children{children_of_or_fail(top_level[0])};
    ASSERT_EQ(branch_children.size(), 2);
    const auto nested_children{children_of_or_fail(branch_children[1])};
    ASSERT_EQ(nested_children.size(), 1);
    const auto other_children{children_of_or_fail(top_level[1])};
    ASSERT_EQ(other_children.size(), 1);
    EXPECT_TRUE(children_of_or_fail(branch_children[0]).empty());
    EXPECT_TRUE(children_of_or_fail(nested_children[0]).empty());
    EXPECT_TRUE(children_of_or_fail(other_children[0]).empty());
    const std::array cursors{top_level[0], branch_children[0], branch_children[1], nested_children[0], top_level[1], other_children[0]};
    for (std::size_t index{0}; index < records.size(); ++index)
    {
        EXPECT_EQ(nori::resource::name_of(cursors[index]), records[index].name);
        EXPECT_EQ(nori::resource::type_of(cursors[index]), nori::resource::property_type::int32);
        const auto value{read_fixture_value(cursors[index])};
        ASSERT_TRUE(value);
        ASSERT_TRUE(std::holds_alternative<std::int32_t>(*value));
        EXPECT_EQ(std::get<std::int32_t>(*value), records[index].value);
    }
    const auto other{nori::resource::try_get(root, "other")};
    ASSERT_TRUE(other);
    EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(*other)), 14);
    const auto leaf{nori::resource::try_get(*other, "leaf")};
    ASSERT_TRUE(leaf);
    EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(*leaf)), 15);
    EXPECT_FALSE(nori::resource::try_get(root, "missing"));
}

TEST_F(codec_test, repeated_names_use_first_appearance_ids)
{
    const auto file{directory_ / "name-ids.nori"};
    const std::array properties{
        fixture_property{0, "z", nori::resource::empty_t{}},
        fixture_property{1, "hp", std::int32_t{100}},
        fixture_property{0, "a", nori::resource::empty_t{}},
        fixture_property{1, "hp", std::int32_t{80}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    std::ifstream bytes{file, std::ios::binary};
    bytes.seekg(12);
    EXPECT_EQ(read_value<std::uint32_t>(bytes), 3u);
    for (const auto [record, id] : std::array<std::pair<std::streamoff, std::uint32_t>, 4>{{{0, 0}, {1, 1}, {2, 2}, {3, 1}}})
    {
        bytes.seekg(56 + record * 28);
        EXPECT_EQ(read_value<std::uint32_t>(bytes), id);
    }
    const auto encoded{file_bytes(file)};
    const std::string names{encoded.begin() + 168, encoded.end()};
    EXPECT_EQ(names, std::string("\x01\0z\x02\0hp\x01\0"
                                 "a",
                         10));
    const auto input{reopen(file)};
    ASSERT_TRUE(input);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*input, "z/hp"), 100);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*input, "a/hp"), 80);
}

TEST_F(codec_test, name_length_limit_applies_only_when_saving_serialized_names)
{
    const auto file{directory_ / "long-name.nori"};
    const std::string name(65535, 'n');
    const std::string oversized(65536, 'n');
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(root, name, std::int32_t{42})};
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(root, file));
    const auto before{file_bytes(file)};
    ASSERT_TRUE(nori::resource::rename(child, oversized));
    const auto failed{nori::resource::save(root)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::invalid_format);
    EXPECT_EQ(nori::resource::name_of(child), oversized);
    EXPECT_EQ(file_bytes(file), before);
    ASSERT_TRUE(nori::resource::rename(child, name));
    const std::string payload(65536, 'v');
    nori::resource::set(child, payload);
    ASSERT_TRUE(nori::resource::save(root));
    const auto loaded{reopen(file)};
    ASSERT_TRUE(loaded);
    EXPECT_EQ(nori::resource::get<std::string>(*loaded, name), payload);
    for (const auto& entry : std::filesystem::directory_iterator{directory_})
    {
        EXPECT_NE(entry.path().extension(), ".tmp");
    }
}

TEST_F(codec_test, loading_trusts_file_identification_counts_and_pool_size)
{
    const auto file{directory_ / "trusted-header.nori"};
    for (const auto position : {0, 4, 8, 12, 48})
    {
        write_fixture_file(file, std::array{fixture_record{"value", 0, 42}});
        {
            std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
            bytes.seekp(position);
            write_scalar(bytes, UINT32_MAX);
        }
        const auto root{reopen(file)};
        ASSERT_TRUE(root);
        EXPECT_EQ(nori::resource::get<std::int32_t>(*root, "value"), 42);
    }
}

TEST_F(codec_test, incomplete_file_reads_report_io_failure)
{
    const auto file{directory_ / "short-read.nori"};
    for (const auto size : {0u, 4u, 12u, 55u, 56u, 84u})
    {
        write_fixture_file(file, std::array{fixture_record{"value", 0, 42}});
        std::filesystem::resize_file(file, size);
        const auto root{reopen(file)};
        ASSERT_FALSE(root);
        EXPECT_EQ(root.error(), nori::resource::error_code::io_failure);
    }
}

TEST_F(codec_test, repeated_name_table_entries_preserve_ids)
{
    const auto file{directory_ / "repeated-names.nori"};
    write_fixture_file(file, std::array{
                                 fixture_record{"a", 1, 10},
                                 fixture_record{"c", 1, 11},
                                 fixture_record{"b", 3, 20},
                                 fixture_record{"d", 3, 21},
                             });
    {
        std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
        bytes.seekp(179);
        bytes.put('c');
        ASSERT_TRUE(bytes);
    }

    const auto input{reopen(file)};
    ASSERT_TRUE(input);
    const auto first{nori::resource::try_get(*input, "a/c")};
    const auto second{nori::resource::try_get(*input, "b/c")};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(nori::resource::name_of(*first), "c");
    EXPECT_EQ(nori::resource::name_of(*second), "c");
    const auto first_value{read_fixture_value(*first)};
    const auto second_value{read_fixture_value(*second)};
    ASSERT_TRUE(first_value);
    ASSERT_TRUE(second_value);
    EXPECT_EQ(std::get<std::int32_t>(*first_value), 11);
    EXPECT_EQ(std::get<std::int32_t>(*second_value), 21);
}

TEST_F(codec_test, saved_wire_format)
{
    constexpr std::array records{
        fixture_record{"branch", 3, 10},
        fixture_record{"nested", 2, 11},
        fixture_record{"inside", 2, 12},
        fixture_record{"sibling", 3, 13},
        fixture_record{"other", 4, 14},
    };
    constexpr std::array<std::uint32_t, 5> depths{0, 1, 2, 1, 0};
    const std::filesystem::path expected{directory_ / "expected.nori"};
    const std::filesystem::path actual{directory_ / "actual.nori"};
    write_fixture_file(expected, records);
    std::vector<fixture_property> encoded_properties;
    for (std::size_t index{0}; index < records.size(); ++index)
    {
        encoded_properties.push_back({depths[index], std::string{records[index].name}, records[index].value});
    }
    EXPECT_FALSE(std::filesystem::exists(actual));
    ASSERT_TRUE(save_fixture(actual, encoded_properties));
    EXPECT_EQ(file_bytes(actual), file_bytes(expected));
}

TEST_F(codec_test, scalars_are_inline_and_equal_pool_values_share_entries)
{
    const std::vector<fixture_value> values{
        nori::resource::empty_t{},
        true,
        std::int32_t{42},
        std::int64_t{42},
        42.0f,
        std::string{"same"},
        nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}},
        nori::resource::binary{{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}},
    };
    const auto file{directory_ / "dedup.nori"};
    std::vector<fixture_property> encoded_properties;
    for (std::size_t index{0}; index < values.size(); ++index)
    {
        encoded_properties.push_back({0, "first" + std::to_string(index), values[index]});
        encoded_properties.push_back({0, "second" + std::to_string(index), values[index]});
    }
    ASSERT_TRUE(save_fixture(file, encoded_properties));
    const auto bytes{file_bytes(file)};
    const auto offset = [&bytes](std::size_t record)
    {
        std::uint64_t result{0};
        for (std::size_t index{0}; index < 8; ++index)
        {
            result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[56 + record * 28 + 12 + index])) << (index * 8);
        }
        return result;
    };
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    for (std::uint32_t index{0}; index < values.size(); ++index)
    {
        EXPECT_EQ(static_cast<unsigned char>(bytes[56 + index * 2 * 28 + 8]), index);
        EXPECT_EQ(offset(index * 2), offset(index * 2 + 1));
        EXPECT_EQ(*read_fixture_value(children_of_or_fail(*input)[index * 2]), values[index]);
        EXPECT_EQ(*read_fixture_value(children_of_or_fail(*input)[index * 2 + 1]), values[index]);
    }
    EXPECT_EQ(offset(2), 1u);
    EXPECT_EQ(offset(4), 42u);
    EXPECT_NE(offset(12), offset(14));
}

TEST_F(codec_test, image_format_wire_values)
{
    using nori::resource::image_format;
    constexpr std::array formats{image_format::rgba8_unorm, image_format::bc5_unorm, image_format::bc7_unorm, image_format::bc7_unorm_srgb};
    const std::filesystem::path file{directory_ / "image.nori"};
    for (std::size_t index{0}; index < formats.size(); ++index)
    {
        const nori::resource::image image{formats[index], 4, 4, 1, std::vector<std::byte>(index == 0 ? 64 : 16)};
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "image", image});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
        const auto bytes{file_bytes(file)};
        ASSERT_GT(bytes.size(), 91u);
        EXPECT_EQ(static_cast<unsigned char>(bytes[91]), index);
        auto input{reopen(file)};
        ASSERT_TRUE(input);
        const auto value{read_fixture_value(children_of_or_fail(*input)[0])};
        ASSERT_TRUE(value);
        EXPECT_EQ(std::get<nori::resource::image>(*value), image);
    }
    for (const std::uint8_t format : std::array<std::uint8_t, 3>{4, 5, 255})
    {
        {
            std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
            bytes.seekp(91);
            bytes.put(static_cast<char>(format));
            ASSERT_TRUE(bytes);
        }
        auto input{reopen(file)};
        ASSERT_TRUE(input);
        EXPECT_TRUE(read_fixture_value(children_of_or_fail(*input)[0]));
    }
}

TEST_F(codec_test, sibling_name_scopes)
{
    const std::filesystem::path file{directory_ / "scopes.nori"};
    std::vector<fixture_property> encoded_properties;
    encoded_properties.push_back({0, "first", nori::resource::empty_t{}});
    encoded_properties.push_back({1, "leaf", std::int32_t{1}});
    encoded_properties.push_back({0, "second", nori::resource::empty_t{}});
    encoded_properties.push_back({1, "leaf", std::int32_t{2}});
    ASSERT_TRUE(save_fixture(file, encoded_properties));
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    const auto root{*input};
    const auto children{children_of_or_fail(root)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(children[1]), "second");
    const auto first{nori::resource::try_get(root, "first")};
    const auto second{nori::resource::try_get(root, "second")};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_EQ(children_of_or_fail(*first).size(), 1);
    ASSERT_EQ(children_of_or_fail(*second).size(), 1);
    EXPECT_EQ(nori::resource::name_of(children_of_or_fail(*first)[0]), "leaf");
    EXPECT_EQ(nori::resource::name_of(children_of_or_fail(*second)[0]), "leaf");
    const auto first_leaf{nori::resource::try_get(*first, "leaf")};
    const auto second_leaf{nori::resource::try_get(*second, "leaf")};
    ASSERT_TRUE(first_leaf);
    ASSERT_TRUE(second_leaf);
    EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(*first_leaf)), 1);
    EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(*second_leaf)), 2);
    EXPECT_FALSE(nori::resource::try_get(root, "leaf"));
    EXPECT_FALSE(nori::resource::try_get(*first, "second"));
    EXPECT_FALSE(nori::resource::try_get(*first, "missing"));
    EXPECT_FALSE(nori::resource::try_get(*first_leaf, "leaf"));
}

TEST_F(codec_test, name_lookup_uses_name_order_without_reordering_records)
{
    constexpr std::array records{
        fixture_record{"zeta", 4, 0},
        fixture_record{"prefix_z", 1, 1},
        fixture_record{"Alpha", 2, 2},
        fixture_record{"prefix", 3, 3},
        fixture_record{"alpha", 4, 4},
        fixture_record{"alpha", 6, 5},
        fixture_record{"zeta", 6, 6},
    };
    const auto file{directory_ / "name-order.nori"};
    write_fixture_file(file, records);
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    const auto root{*input};
    const auto top_level{children_of_or_fail(root)};
    ASSERT_EQ(top_level.size(), 2);
    const auto first_children{children_of_or_fail(top_level[0])};
    ASSERT_EQ(first_children.size(), 4);
    const auto second_children{children_of_or_fail(top_level[1])};
    ASSERT_EQ(second_children.size(), 1);
    const std::array parents{root, top_level[0], top_level[0], top_level[0], top_level[0], root, top_level[1]};
    const std::array cursors{top_level[0], first_children[0], first_children[1], first_children[2], first_children[3], top_level[1], second_children[0]};
    for (std::size_t index{0}; index < records.size(); ++index)
    {
        const auto found{nori::resource::try_get(parents[index], records[index].name)};
        ASSERT_TRUE(found);
        EXPECT_EQ(nori::resource::name_of(*found), records[index].name);
        EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(*found)), records[index].value);
        EXPECT_EQ(nori::resource::name_of(cursors[index]), records[index].name);
    }
    EXPECT_FALSE(nori::resource::try_get(top_level[0], "ALPHA"));
    EXPECT_FALSE(nori::resource::try_get(top_level[0], "prefi"));
    EXPECT_FALSE(nori::resource::try_get(top_level[0], "prefix_zz"));
    EXPECT_FALSE(nori::resource::try_get(root, "prefix"));
}

TEST_F(codec_test, opening_metadata_does_not_validate_sibling_name_uniqueness)
{
    const std::array<std::array<fixture_record, 5>, 4> cases{{
        {{{"same", 0, 0}, {"same", 1, 1}, {"other", 2, 2}, {"last", 3, 3}, {"leaf", 4, 4}}},
        {{{"parent", 4, 0}, {"same", 1, 1}, {"same", 2, 2}, {"other", 3, 3}, {"leaf", 4, 4}}},
        {{{"same", 1, 0}, {"nested", 1, 1}, {"other", 2, 2}, {"same", 4, 3}, {"leaf", 4, 4}}},
        {{{"parent", 4, 0}, {"same", 2, 1}, {"nested", 2, 2}, {"other", 3, 3}, {"same", 4, 4}}},
    }};
    const auto file{directory_ / "duplicate-siblings.nori"};
    for (const auto& records : cases)
    {
        write_fixture_file(file, records);
        const auto input{reopen(file)};
        ASSERT_TRUE(input);
    }
}

TEST_F(codec_test, empty_document)
{
    const std::filesystem::path file{directory_ / "empty.nori"};
    std::vector<fixture_property> encoded_properties;
    ASSERT_TRUE(save_fixture(file, encoded_properties));
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    EXPECT_TRUE(children_of_or_fail(*input).empty());
    EXPECT_FALSE(nori::resource::try_get(*input, "missing"));
    EXPECT_EQ(std::filesystem::file_size(file), 56);
}

TEST_F(codec_test, scalar_padding_is_ignored)
{
    constexpr std::array records{fixture_record{"first", 0, 42}, fixture_record{"broken", 1, 99}};
    const std::filesystem::path file{directory_ / "lazy.nori"};
    write_fixture_file(file, records, true);
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    const auto first{read_fixture_value(children_of_or_fail(*input)[0])};
    ASSERT_TRUE(first);
    EXPECT_EQ(std::get<std::int32_t>(*first), 42);
    EXPECT_EQ(std::get<std::int32_t>(*read_fixture_value(children_of_or_fail(*input)[1])), 99);
    EXPECT_TRUE(read_fixture_value(children_of_or_fail(*input)[0]));
}

TEST_F(codec_test, inline_scalar_golden_bytes_and_float_normalization)
{
    struct scalar_case
    {
        fixture_value value;
        std::uint8_t tag;
        std::array<unsigned char, 16> bytes;
    };

    const std::array cases{
        scalar_case{nori::resource::empty_t{}, 0, {}},
        scalar_case{false, 1, {}},
        scalar_case{true, 1, {1}},
        scalar_case{std::int32_t{-1}, 2, {255, 255, 255, 255}},
        scalar_case{std::numeric_limits<std::int32_t>::min(), 2, {0, 0, 0, 128}},
        scalar_case{std::int64_t{-1}, 3, {255, 255, 255, 255, 255, 255, 255, 255}},
        scalar_case{std::numeric_limits<std::int64_t>::min(), 3, {0, 0, 0, 0, 0, 0, 0, 128}},
        scalar_case{1.0f, 4, {0, 0, 128, 63}},
    };
    const auto file{directory_ / "scalars.nori"};
    std::vector<fixture_property> encoded_properties;
    for (std::size_t index{0}; index < cases.size(); ++index)
    {
        encoded_properties.push_back({0, std::to_string(index), cases[index].value});
    }
    ASSERT_TRUE(save_fixture(file, encoded_properties));
    const auto bytes{file_bytes(file)};
    ASSERT_EQ(bytes.size(), 56u + 28u * cases.size() + 3u * cases.size());
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    for (std::uint32_t index{0}; index < cases.size(); ++index)
    {
        EXPECT_EQ(static_cast<unsigned char>(bytes[56 + index * 28 + 8]), cases[index].tag);
        for (std::size_t byte{0}; byte < 16; ++byte)
        {
            EXPECT_EQ(static_cast<unsigned char>(bytes[56 + index * 28 + 12 + byte]), cases[index].bytes[byte]);
        }
        ASSERT_TRUE(read_fixture_value(children_of_or_fail(*input)[index]));
        EXPECT_EQ(*read_fixture_value(children_of_or_fail(*input)[index]), cases[index].value);
    }
    for (const auto [bits, expected] : std::array<std::pair<std::uint32_t, std::uint32_t>, 11>{{
             {0x00000000, 0x00000000},
             {0x80000000, 0x00000000},
             {0x7FC00000, 0x7FC00000},
             {0x7F800001, 0x7FC00000},
             {0xFFC12345, 0x7FC00000},
             {0xFF800001, 0x7FC00000},
             {0x7F800000, 0x7F800000},
             {0xFF800000, 0xFF800000},
             {0x00000001, 0x00000001},
             {0x80000001, 0x80000001},
             {0x3F800000, 0x3F800000},
         }})
    {
        const auto float_file{directory_ / "float.nori"};
        std::vector<fixture_property> float_properties;
        float_properties.push_back({0, "f", std::bit_cast<float>(bits)});
        ASSERT_TRUE(save_fixture(float_file, float_properties));
        std::ostringstream golden;
        write_scalar(golden, expected);
        const auto encoded{file_bytes(float_file)};
        EXPECT_EQ(std::string(encoded.begin() + 68, encoded.begin() + 72), golden.str());
        EXPECT_TRUE(std::all_of(encoded.begin() + 72, encoded.begin() + 84, [](char byte)
        {
            return byte == 0;
        }));
        auto loaded{reopen(float_file)};
        ASSERT_TRUE(loaded);
        const auto value{read_fixture_value(children_of_or_fail(*loaded)[0])};
        ASSERT_TRUE(value);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(std::get<float>(*value)), expected);
    }
}

TEST_F(codec_test, scalar_payloads_are_decoded_without_canonical_validation)
{
    struct invalid_case
    {
        std::uint8_t tag;
        std::size_t offset;
        std::uint32_t bits;
    };

    const auto file{directory_ / "invalid-scalar.nori"};
    const std::array cases{
        invalid_case{0, 0, 1},
        invalid_case{1, 0, 2},
        invalid_case{1, 1, 1},
        invalid_case{2, 4, 1},
        invalid_case{3, 8, 1},
        invalid_case{4, 4, 1},
        invalid_case{4, 0, 0x80000000},
        invalid_case{4, 0, 0x7F800001},
        invalid_case{4, 0, 0xFFC00000},
    };
    for (const auto& invalid : cases)
    {
        write_fixture_file(file, std::array{fixture_record{"value", 0, 0}});
        {
            std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
            bytes.seekp(64);
            write_scalar(bytes, invalid.tag);
            bytes.seekp(static_cast<std::streamoff>(68 + invalid.offset));
            write_scalar(bytes, invalid.bits);
        }
        auto input{reopen(file)};
        ASSERT_TRUE(input);
        EXPECT_EQ(nori::resource::type_of(children_of_or_fail(*input)[0]), nori::resource::property_type{invalid.tag});
        const auto value{read_fixture_value(children_of_or_fail(*input)[0])};
        ASSERT_TRUE(value);
        if (invalid.tag == 4 && invalid.offset == 0)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(std::get<float>(*value)), invalid.bits);
        }
        if (invalid.tag == 1)
        {
            EXPECT_EQ(std::get<bool>(*value), invalid.offset == 0);
        }
    }
}

TEST_F(codec_test, raw_pool_bytes_and_deduplication_categories)
{
    const auto file{directory_ / "pool.nori"};
    const std::string text{"\xFF\0x", 3};
    const nori::resource::image image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)};
    const nori::resource::binary image_bytes{{
        std::byte{0},
        std::byte{1},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{1},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{1},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{0},
    }};
    std::vector<fixture_property> encoded_properties;
    encoded_properties.push_back({0, "same", std::string{"same"}});
    encoded_properties.push_back({0, "b", nori::resource::binary{{std::byte{'s'}, std::byte{'a'}, std::byte{'m'}, std::byte{'e'}}}});
    encoded_properties.push_back({0, "i", image});
    encoded_properties.push_back({0, "r", image_bytes});
    encoded_properties.push_back({0, "s", text});
    encoded_properties.push_back({0, "e", std::string{}});
    encoded_properties.push_back({0, "z", nori::resource::binary{}});
    ASSERT_TRUE(save_fixture(file, encoded_properties));
    std::ifstream bytes{file, std::ios::binary};
    const auto field = [&bytes](std::streamoff position)
    {
        bytes.seekg(position);
        std::uint64_t bits{0};
        for (std::size_t index{0}; index < 8; ++index)
        {
            bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes.get())) << (index * 8);
        }
        return bits;
    };
    EXPECT_EQ(field(24), 252u);
    EXPECT_EQ(field(40), 276u);
    EXPECT_EQ(field(68), 0u);
    EXPECT_EQ(field(96), 4u);
    EXPECT_NE(field(124), field(152));
    EXPECT_EQ(field(132), 17u);
    EXPECT_EQ(field(188), 3u);
    const auto encoded{file_bytes(file)};
    const std::string pool{encoded.begin() + 276, encoded.end()};
    const std::string expected{std::string{"samesame"} + std::string(reinterpret_cast<const char*>(image_bytes.data.data()), 17) + std::string(reinterpret_cast<const char*>(image_bytes.data.data()), 17) + text};
    EXPECT_EQ(pool, expected);
    auto input{reopen(file)};
    ASSERT_TRUE(input);
    EXPECT_EQ(std::get<std::string>(*read_fixture_value(children_of_or_fail(*input)[4])), text);
    EXPECT_TRUE(std::get<std::string>(*read_fixture_value(children_of_or_fail(*input)[5])).empty());
    EXPECT_TRUE(std::get<nori::resource::binary>(*read_fixture_value(children_of_or_fail(*input)[6])).data.empty());
}

TEST_F(codec_test, image_mip_lengths_and_malformed_metadata)
{
    struct image_case
    {
        nori::resource::image_format format;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t levels;
        std::size_t bytes;
    };

    using nori::resource::image_format;
    const auto file{directory_ / "mips.nori"};
    for (const auto& valid : std::array{
             image_case{image_format::rgba8_unorm, 1, 1, 1, 4},
             image_case{image_format::rgba8_unorm, 2, 2, 2, 20},
             image_case{image_format::rgba8_unorm, 4, 4, 3, 84},
             image_case{image_format::rgba8_unorm, 5, 7, 3, 168},
             image_case{image_format::rgba8_unorm, 1, 8, 4, 60},
             image_case{image_format::bc5_unorm, 1, 1, 1, 16},
             image_case{image_format::bc5_unorm, 2, 2, 2, 32},
             image_case{image_format::bc7_unorm, 4, 4, 3, 48},
             image_case{image_format::bc7_unorm, 5, 7, 3, 96},
             image_case{image_format::bc7_unorm_srgb, 8, 8, 4, 112},
         })
    {
        const nori::resource::image image{valid.format, valid.width, valid.height, valid.levels, std::vector<std::byte>(valid.bytes)};
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "i", image});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
        EXPECT_EQ(std::filesystem::file_size(file), 56u + 28u + 3u + 13u + valid.bytes);
        auto input{reopen(file)};
        ASSERT_TRUE(input);
        const auto read{read_fixture_value(children_of_or_fail(*input)[0])};
        ASSERT_TRUE(read);
        EXPECT_EQ(std::get<nori::resource::image>(*read), image);
    }
    for (const auto [position, value] : std::array<std::pair<std::streamoff, std::uint32_t>, 6>{{
             {88, 0},
             {92, 0},
             {96, 0},
             {96, 33},
             {88, UINT32_MAX},
             {92, UINT32_MAX},
         }})
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "i", nori::resource::image{image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
        {
            std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
            bytes.seekp(position);
            write_scalar(bytes, value);
        }
        auto input{reopen(file)};
        ASSERT_TRUE(input);
        EXPECT_TRUE(read_fixture_value(children_of_or_fail(*input)[0]));
    }
}

TEST_F(codec_test, incomplete_image_header_and_pixels_report_io_failure)
{
    const auto file{directory_ / "short-image.nori"};
    const std::array properties{
        fixture_property{0, "icon", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}},
    };
    for (const auto removed : {5u, 1u})
    {
        ASSERT_TRUE(save_fixture(file, properties));
        std::filesystem::resize_file(file, std::filesystem::file_size(file) - removed);
        const auto root{reopen(file)};
        ASSERT_TRUE(root);
        const auto res{nori::resource::try_get<nori::resource::image_view>(*root, "icon")};
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), nori::resource::error_code::io_failure);
    }
}

TEST_F(codec_test, loading_trusts_property_names_and_reserved_bytes)
{
    const auto file{directory_ / "trusted-names.nori"};
    for (const std::string_view name : std::array<std::string_view, 7>{"", ".", "..", "a/b", "a::b", "a.nori", std::string_view{"a\0b", 3}})
    {
        write_fixture_file(file, std::array{fixture_record{name, 0, 42}});
        {
            std::fstream bytes{file, std::ios::binary | std::ios::in | std::ios::out};
            bytes.seekp(65);
            bytes.put(static_cast<char>(255));
            bytes.put(static_cast<char>(255));
            bytes.put(static_cast<char>(255));
        }
        const auto root{reopen(file)};
        ASSERT_TRUE(root);
        const auto children{children_of_or_fail(*root)};
        ASSERT_EQ(children.size(), 1);
        EXPECT_EQ(nori::resource::name_of(children[0]), name);
        EXPECT_EQ(nori::resource::get<std::int32_t>(children[0]), 42);
    }
}

TEST_F(codec_test, invalid_name_writes_preserve_destination)
{
    const auto file{directory_ / "name-validation.nori"};
    write_fixture_file(file, std::array{fixture_record{"value", 0, 42}});
    const auto before{file_bytes(file)};
    for (const auto name : std::array{
             std::string_view{},
             std::string_view{"."},
             std::string_view{".."},
             std::string_view{"bad/name"},
             std::string_view{"bad::name"},
             std::string_view{"bad\0name", 8},
             std::string_view{"bad.nori"},
         })
    {
        const std::array properties{
            fixture_property{0, "valid", std::int32_t{1}},
            fixture_property{0, std::string{name}, std::int32_t{2}},
        };
        const auto failed{save_fixture(file, properties)};
        ASSERT_FALSE(failed);
        EXPECT_EQ(failed.error(), nori::resource::error_code::invalid_format);
        EXPECT_EQ(file_bytes(file), before);
    }
    for (const auto& entry : std::filesystem::directory_iterator{directory_})
    {
        EXPECT_NE(entry.path().extension(), ".tmp");
    }
}

TEST_F(codec_test, invalid_image_writes_preserve_destination)
{
    using nori::resource::image_format;
    const auto file{directory_ / "image-validation.nori"};
    write_fixture_file(file, std::array{fixture_record{"value", 0, 42}});
    const auto before{file_bytes(file)};
    for (const auto& invalid : std::array{
             nori::resource::image{image_format::rgba8_unorm, 0, 1, 1, {}},
             nori::resource::image{image_format::rgba8_unorm, 1, 0, 1, {}},
             nori::resource::image{image_format::rgba8_unorm, 1, 1, 0, {}},
             nori::resource::image{image_format::rgba8_unorm, 1, 1, 2, std::vector<std::byte>(8)},
             nori::resource::image{image_format::rgba8_unorm, UINT32_MAX, UINT32_MAX, 32, {}},
             nori::resource::image{image_format::bc7_unorm, 8, 8, 4, std::vector<std::byte>(111)},
             nori::resource::image{image_format::bc7_unorm, 8, 8, 4, std::vector<std::byte>(113)},
             nori::resource::image{image_format{255}, 1, 1, 1, std::vector<std::byte>(4)},
         })
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "image", invalid});
        const auto failed{save_fixture(file, encoded_properties)};
        ASSERT_FALSE(failed);
        EXPECT_EQ(failed.error(), nori::resource::error_code::invalid_format);
        EXPECT_EQ(file_bytes(file), before);
    }
}
} // namespace
