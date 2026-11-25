#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
static_assert(!std::ranges::range<nori::resource::path>);

TEST(path_test, strings)
{
    const std::string owned_text{"owned.nori"};
    const nori::resource::path owned{owned_text};
    EXPECT_EQ(owned.string(), owned_text);
    EXPECT_EQ(owned.string_view(), owned_text);
    EXPECT_EQ(owned.file_path(), nori::core::utf8_to_path(owned_text));
    EXPECT_EQ(nori::resource::subpath_iterator{owned}, nori::resource::subpath_iterator{});
    EXPECT_TRUE(owned.has_file_path());

    std::string long_text(80, 'a');
    long_text.append(".nori");
    const nori::resource::path long_path{long_text};
    EXPECT_EQ(long_path.string(), long_text);
    EXPECT_EQ(long_path.string_view(), long_text);
    EXPECT_EQ(long_path.file_path(), nori::core::utf8_to_path(long_text));
}

TEST(path_test, lexical_path)
{
    const nori::resource::path target{"missing/../future.nori::value.nori.tmp"};
    EXPECT_TRUE(target.has_file_path());
    EXPECT_EQ(target.file_path().generic_string(), "missing/../future.nori");
    EXPECT_EQ(path_segments(target), std::vector<std::string>{"value.nori.tmp"});

    const std::string bad_file{"missing\0.nori", sizeof("missing\0.nori") - 1};
    const nori::resource::path invalid_file{bad_file};
    EXPECT_TRUE(invalid_file.has_file_path());
    EXPECT_FALSE(invalid_file.file_path().empty());
    EXPECT_EQ(nori::resource::subpath_iterator{invalid_file}, nori::resource::subpath_iterator{});

    const std::string bad_segment{"future.nori::value\0name", sizeof("future.nori::value\0name") - 1};
    const nori::resource::path invalid_segment{bad_segment};
    EXPECT_EQ(invalid_segment.string_view(), bad_segment);
    EXPECT_TRUE(invalid_segment.has_file_path());
    EXPECT_NE(nori::resource::subpath_iterator{invalid_segment}, nori::resource::subpath_iterator{});

    const nori::resource::path invalid_suffix{"future.nori::value.nori"};
    EXPECT_TRUE(invalid_suffix.has_file_path());
    EXPECT_EQ(path_segments(invalid_suffix), std::vector<std::string>{"value.nori"});
}

TEST(path_test, filesystem_syntax)
{
    const nori::resource::path single_colon{"folder:name.nori::value"};
    EXPECT_TRUE(single_colon.has_file_path());
    EXPECT_EQ(single_colon.file_path(), nori::core::utf8_to_path("folder:name.nori"));
    EXPECT_EQ(path_segments(single_colon), std::vector<std::string>{"value"});

    const std::string repeated_text{"folder//mixed\\path.nori::value"};
    const nori::resource::path repeated{repeated_text};
    EXPECT_TRUE(repeated.has_file_path());
    EXPECT_EQ(repeated.file_path(), nori::core::utf8_to_path("folder//mixed/path.nori"));
    EXPECT_EQ(path_segments(repeated), std::vector<std::string>{"value"});
}

TEST(path_test, has_subpath)
{
    EXPECT_TRUE(nori::resource::path{"a/b"}.has_subpath());
    EXPECT_TRUE(nori::resource::path{"file.nori::a/b"}.has_subpath());
    EXPECT_TRUE(nori::resource::path{"file.nori::value.nori"}.has_subpath());
    EXPECT_TRUE(nori::resource::path{"file.nori::/"}.has_subpath());
    EXPECT_FALSE(nori::resource::path{}.has_subpath());
    EXPECT_FALSE(nori::resource::path{"file.nori"}.has_subpath());
    EXPECT_FALSE(nori::resource::path{"file.nori::"}.has_subpath());
}

TEST(path_test, subpath)
{
    EXPECT_EQ(nori::resource::path{"file.nori::a/b"}.subpath(), "a/b");
    EXPECT_EQ(nori::resource::path{"a/b"}.subpath(), "a/b");
    EXPECT_TRUE(nori::resource::path{"file.nori"}.subpath().empty());
    EXPECT_TRUE(nori::resource::path{"file.nori::"}.subpath().empty());
    EXPECT_TRUE(nori::resource::path{}.subpath().empty());
    EXPECT_EQ(nori::resource::path{"file.nori::a/.././/b/"}.subpath(), "a/.././/b/");
    EXPECT_EQ(nori::resource::path{"file.nori::value.nori"}.subpath(), "value.nori");
    EXPECT_EQ(nori::resource::path{"file.nori::a::b"}.subpath(), "a::b");
}

TEST(path_test, segments)
{
    EXPECT_EQ(path_segments(nori::resource::path{"/folder//value/"}), (std::vector<std::string>{"", "folder", "", "value", ""}));
    EXPECT_EQ(path_segments(nori::resource::path{"tree.nori::/folder//value/"}), (std::vector<std::string>{"", "folder", "", "value", ""}));
    EXPECT_TRUE(path_segments(nori::resource::path{"tree.nori"}).empty());
    EXPECT_TRUE(path_segments(nori::resource::path{"tree.nori::"}).empty());

    const nori::resource::path raw_file{"folder\\mixed\\tree.nori::value"};
    EXPECT_EQ(raw_file.string(), "folder\\mixed\\tree.nori::value");
    EXPECT_EQ(raw_file.file_path().generic_string(), "folder/mixed/tree.nori");
    EXPECT_EQ(path_segments(raw_file), (std::vector<std::string>{"value"}));
}

TEST(path_test, lexical_operations)
{
    EXPECT_EQ((nori::resource::path{"folder/"} / "/value").string(), "folder//value");
    EXPECT_EQ((nori::resource::path{"tree.nori::"} / "value").string(), "tree.nori::value");
    EXPECT_EQ((nori::resource::path{"tree.nori"} / "value").string(), "tree.nori::value");
    EXPECT_EQ((nori::resource::path{"folder"} / nori::resource::path{"other.nori::value"}).string(), "other.nori::value");
    EXPECT_EQ((nori::resource::path{} / nori::resource::path{"value"}).string(), "value");
    EXPECT_EQ((nori::resource::path{"value"} / nori::resource::path{}).string(), "value");

    EXPECT_NE(nori::resource::path{"tree.nori"}, nori::resource::path{"tree.nori::"});
    std::unordered_set<nori::resource::path> values;
    values.emplace("tree.nori");
    values.emplace("tree.nori::");
    EXPECT_EQ(values.size(), 2);
}

TEST(path_test, copy_move)
{
    const nori::resource::path source{"folder:name//path.nori::value/child"};
    const auto expected_file{source.file_path()};
    const auto expected_segments{path_segments(source)};

    const nori::resource::path copied{source};
    EXPECT_EQ(copied.string(), source.string());
    EXPECT_EQ(copied.has_file_path(), source.has_file_path());
    EXPECT_EQ(copied.file_path(), expected_file);
    EXPECT_EQ(path_segments(copied), expected_segments);

    nori::resource::path copy_assigned{};
    copy_assigned = source;
    EXPECT_EQ(copy_assigned.string(), source.string());
    EXPECT_EQ(copy_assigned.has_file_path(), source.has_file_path());
    EXPECT_EQ(copy_assigned.file_path(), expected_file);
    EXPECT_EQ(path_segments(copy_assigned), expected_segments);

    nori::resource::path moved{std::move(copied)};
    EXPECT_EQ(moved.string(), source.string());
    EXPECT_EQ(moved.has_file_path(), source.has_file_path());
    EXPECT_EQ(moved.file_path(), expected_file);
    EXPECT_EQ(path_segments(moved), expected_segments);

    nori::resource::path move_assigned{};
    move_assigned = std::move(copy_assigned);
    EXPECT_EQ(move_assigned.string(), source.string());
    EXPECT_EQ(move_assigned.has_file_path(), source.has_file_path());
    EXPECT_EQ(move_assigned.file_path(), expected_file);
    EXPECT_EQ(path_segments(move_assigned), expected_segments);
}

static_assert(std::input_iterator<nori::resource::subpath_iterator>);
static_assert(!std::forward_iterator<nori::resource::subpath_iterator>);
static_assert(std::ranges::input_range<nori::resource::subpath_iterator>);

TEST(path_test, owns_path)
{
    nori::resource::subpath_iterator it;
    {
        nori::resource::path source{"tree.nori::parent/child"};
        it = nori::resource::subpath_iterator{source};
        source = "other.nori::replacement";
        EXPECT_EQ(*it, "parent");
    }
    EXPECT_EQ(*it, "parent");
    ++it;
    EXPECT_EQ(*it, "child");
    ++it;
    EXPECT_EQ(it, nori::resource::subpath_iterator{});
}

TEST(path_test, copies_have_independent_positions)
{
    nori::resource::subpath_iterator it{nori::resource::path{"parent/child"}};
    const auto copy{it};
    EXPECT_EQ(it, copy);
    ++it;
    EXPECT_NE(it, copy);
    EXPECT_EQ(*it, "child");
    EXPECT_EQ(*copy, "parent");
    const std::string_view retained{*copy};
    ++it;
    EXPECT_NE(copy, nori::resource::subpath_iterator{});
    it = nori::resource::subpath_iterator{nori::resource::path{"replacement"}};
    EXPECT_EQ(*copy, "parent");
    EXPECT_EQ(retained, "parent");
}

TEST(path_test, postfix_returns_previous_iterator)
{
    nori::resource::subpath_iterator it{nori::resource::path{"parent/child"}};
    const auto copy{it};
    const auto parent{it++};
    EXPECT_EQ(*parent, "parent");
    EXPECT_EQ(*it, "child");
    EXPECT_EQ(*copy, "parent");
    const auto child{it++};
    EXPECT_EQ(*child, "child");
    EXPECT_EQ(*parent, "parent");
    EXPECT_EQ(it, nori::resource::subpath_iterator{});
    EXPECT_NE(copy, nori::resource::subpath_iterator{});
}

TEST(path_test, lexical_range)
{
    const std::vector<std::string> expected{"a", "..", ".", "", "b", ""};
    std::vector<std::string> segments;
    for (auto segment : nori::resource::subpath_iterator{nori::resource::path{"tree.nori::a/.././/b/"}})
    {
        segments.emplace_back(segment);
    }
    EXPECT_EQ(segments, expected);

    nori::resource::subpath_iterator finished{nori::resource::path{"value"}};
    ++finished;
    EXPECT_EQ(finished, nori::resource::subpath_iterator{});
    EXPECT_EQ(finished, nori::resource::subpath_iterator{nori::resource::path{"tree.nori"}});
    EXPECT_EQ(finished, nori::resource::subpath_iterator{nori::resource::path{"tree.nori::"}});
    EXPECT_EQ(finished, nori::resource::subpath_iterator{nori::resource::path{}});
}
} // namespace
