#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
TEST_F(path_resolution_test, node_name_affects_unbound_and_bound_paths)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(root, "child", std::int32_t{42})};
    ASSERT_TRUE(child);
    EXPECT_EQ(nori::resource::path_of(child).string(), "child");
    ASSERT_TRUE(nori::resource::rename(child, "renamed"));
    EXPECT_EQ(nori::resource::path_of(child).string(), "renamed");
    ASSERT_TRUE(nori::resource::save_as(root, "root.nori"));
    EXPECT_EQ(nori::resource::path_of(child), bound_resource_path(directory_ / "root.nori", "renamed"));
    ASSERT_TRUE(nori::resource::rename(child, "another"));
    const auto expected{bound_resource_path(directory_ / "root.nori", "another")};
    EXPECT_EQ(nori::resource::path_of(child), expected);
    EXPECT_EQ(nori::resource::get("root.nori::another"), child);
}

TEST_F(path_resolution_test, top_level_parent_boundary)
{
    const auto root{nori::resource::create_root()};
    const auto parent{nori::resource::create(root, "parent", nori::resource::empty)};
    const auto child{nori::resource::create(parent, "child", std::int32_t{42})};
    EXPECT_EQ(nori::resource::try_get(child, ".."), parent);
    EXPECT_EQ(nori::resource::try_get(child, "../.."), root);
    EXPECT_EQ(nori::resource::try_get(child, "../../..").error(), nori::resource::error_code::invalid_path);
    EXPECT_EQ(nori::resource::try_get(root, "..").error(), nori::resource::error_code::invalid_path);
}

TEST_F(path_resolution_test, sequential_navigation)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(root, "child", nori::resource::empty)};
    const nori::resource::handle sibling{nori::resource::create(root, "sibling", nori::resource::empty)};
    ASSERT_TRUE(child);
    ASSERT_TRUE(sibling);

    EXPECT_EQ(nori::resource::try_get(root, "./child/.."), root);
    EXPECT_EQ(nori::resource::try_get(root, "child/../sibling"), sibling);
    for (const auto target : {"missing/..", "missing/../child", "missing/../../..", "child/missing/.."})
    {
        const auto missing{nori::resource::try_get(root, target)};
        ASSERT_FALSE(missing) << target;
        EXPECT_EQ(missing.error(), nori::resource::error_code::not_found) << target;
    }

    EXPECT_EQ(nori::resource::try_get(child, "."), child);
    const auto detached_missing{nori::resource::try_get(child, "missing/../..")};
    ASSERT_FALSE(detached_missing);
    EXPECT_EQ(detached_missing.error(), nori::resource::error_code::not_found);
}

TEST_F(path_resolution_test, lazy_navigation_returns_to_parent_before_resolving_sibling)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(root, "branch", nori::resource::empty)};
    ASSERT_TRUE(nori::resource::create(branch, "left", nori::resource::empty));
    ASSERT_TRUE(nori::resource::create(branch, "right", std::int32_t{42}));
    ASSERT_TRUE(nori::resource::save_as(root, "navigation.nori"));
    nori::resource::remove(root);

    EXPECT_EQ(nori::resource::try_get<std::int32_t>("navigation.nori::branch/left/../right"), 42);
}

TEST_F(path_resolution_test, aliases)
{
    std::filesystem::create_directories(directory_ / "alias");
    const std::string absolute_path{nori::core::path_to_utf8(directory_ / "shared.nori")};

    const nori::resource::handle root{create_resource(absolute_path)};
    const nori::resource::handle value{create_resource("alias/../shared.nori::value")};
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(directory_ / "shared.nori"));
    EXPECT_EQ(nori::resource::parent_of(value), root);
    EXPECT_EQ(nori::resource::try_get("shared.nori"), root);

    nori::resource::set(value, std::int32_t{42});
    const auto saved{nori::resource::save(root)};
    ASSERT_TRUE(saved) << (saved ? "" : nori::resource::to_string(saved.error()));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto absolute_value{nori::resource::try_get(absolute_path + "::value")};
    const auto relative_value{nori::resource::try_get("shared.nori::value")};
    const auto alias_value{nori::resource::try_get("alias/../shared.nori::value")};
    ASSERT_TRUE(absolute_value) << (absolute_value ? "" : nori::resource::to_string(absolute_value.error()));
    ASSERT_TRUE(relative_value);
    ASSERT_TRUE(alias_value);
    EXPECT_EQ(*absolute_value, *relative_value);
    EXPECT_EQ(*absolute_value, *alias_value);
    EXPECT_EQ(nori::resource::path_of(nori::resource::parent_of(*absolute_value)), bound_resource_path(directory_ / "shared.nori"));
    EXPECT_EQ(nori::resource::get<std::int32_t>(*absolute_value), 42);
}

TEST_F(path_resolution_test, bound_round_trip)
{
    const nori::resource::handle root{create_resource("nested/../shared.nori")};
    const nori::resource::handle value{create_resource("shared.nori::folder/value")};

    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(directory_ / "shared.nori"));
    EXPECT_EQ(nori::resource::path_of(value), bound_resource_path(directory_ / "shared.nori", "folder/value"));
    EXPECT_EQ(nori::resource::get(nori::resource::path_of(root)), root);
    EXPECT_EQ(nori::resource::get(nori::resource::path_of(value)), value);

    const scoped_temp_dir outside_directory{directory_.parent_path() / "nori-resource-outside"};
    const std::filesystem::path outside{outside_directory.path() / "external.nori"};
    const nori::resource::handle external{create_resource(nori::core::path_to_utf8(outside))};
    const nori::resource::handle external_value{
        create_resource(nori::core::path_to_utf8(outside) + "::value")
    };
    EXPECT_EQ(nori::resource::path_of(external), bound_resource_path(outside));
    EXPECT_EQ(nori::resource::path_of(external_value), bound_resource_path(outside, "value"));
    EXPECT_EQ(nori::resource::get(nori::resource::path_of(external_value)), external_value);
}

TEST_F(path_resolution_test, parsing)
{
    const nori::resource::path literal{"old.nori"};
    EXPECT_TRUE(literal.has_file_path());
    EXPECT_EQ(literal.string(), "old.nori");
    EXPECT_EQ(literal.file_path(), nori::core::utf8_to_path("old.nori"));
    EXPECT_EQ(nori::resource::subpath_iterator{literal}, nori::resource::subpath_iterator{});

    const nori::resource::path child{"old.nori::child/value"};
    EXPECT_TRUE(child.has_file_path());
    EXPECT_EQ(child.string(), "old.nori::child/value");
    EXPECT_EQ(child.file_path(), nori::core::utf8_to_path("old.nori"));
    const std::vector<std::string> expected_child_segments{"child", "value"};
    EXPECT_EQ(path_segments(child), expected_child_segments);

    const nori::resource::path relative{"child/value"};
    EXPECT_FALSE(relative.has_file_path());
    EXPECT_TRUE(relative.file_path().empty());
    const std::vector<std::string> expected_relative_segments{"child", "value"};
    EXPECT_EQ(path_segments(relative), expected_relative_segments);

    const nori::resource::handle root{create_resource("test.nori")};
    const nori::resource::handle nested{create_resource("test.nori::hello/world")};
    std::filesystem::create_directories(directory_ / "dir.nori");
    const nori::resource::handle dotted{
        create_resource("dir.nori/test.nori::hello.value")
    };

    EXPECT_EQ(nori::resource::get("test.nori"), root);
    EXPECT_EQ(nori::resource::get("test.nori::hello/world"), nested);
    EXPECT_EQ(nori::resource::get("dir.nori/test.nori::hello.value"), dotted);
    EXPECT_EQ(nori::resource::name_of(dotted), "hello.value");
    EXPECT_EQ(nori::resource::path_of(nori::resource::parent_of(dotted)), bound_resource_path(directory_ / "dir.nori/test.nori"));
}

TEST_F(path_resolution_test, lexical_composition)
{
    const nori::resource::path root{"test.nori"};
    const nori::resource::path child{root / "folder"};
    const nori::resource::path nested{child / "value/item"};
    const nori::resource::path relative{nori::resource::path{"folder"} / "value/item"};

    EXPECT_TRUE(child.has_file_path());
    EXPECT_EQ(child.string(), "test.nori::folder");
    EXPECT_EQ(nested.string(), "test.nori::folder/value/item");
    EXPECT_FALSE(relative.has_file_path());
    EXPECT_EQ(relative.string(), "folder/value/item");

    EXPECT_EQ((root / nori::resource::path{"other.nori"}).string(), "other.nori");
    EXPECT_EQ((nori::resource::path{} / "value").string(), "value");
}

TEST_F(path_resolution_test, missing_paths)
{
    const nori::resource::path target{"missing/future.nori::folder/value"};
    EXPECT_TRUE(target.has_file_path());
    EXPECT_EQ(target.file_path(), nori::core::utf8_to_path("missing/future.nori"));
    const std::vector<std::string> expected_target_segments{"folder", "value"};
    EXPECT_EQ(path_segments(target), expected_target_segments);
    EXPECT_FALSE(std::filesystem::exists(directory_ / "missing/future.nori"));

    const std::filesystem::path filesystem_path{directory_ / "future.nori"};
    const nori::resource::path converted{filesystem_path};
    EXPECT_TRUE(converted.has_file_path());
    EXPECT_EQ(converted.file_path(), filesystem_path);
    EXPECT_EQ(nori::resource::subpath_iterator{converted}, nori::resource::subpath_iterator{});
    EXPECT_EQ(converted.string(), nori::core::path_to_utf8(filesystem_path));
    EXPECT_FALSE(std::filesystem::exists(filesystem_path));
}

TEST_F(path_resolution_test, implicit_paths)
{
    const std::string root_text{"implicit.nori"};
    const std::string_view child_text{"implicit.nori::parent/child"};
    const nori::resource::handle root{create_resource(root_text)};
    const nori::resource::handle child{create_resource(child_text)};

    ASSERT_TRUE(root);
    ASSERT_TRUE(child);
    EXPECT_EQ(nori::resource::get(nori::resource::path{root_text}), root);
    EXPECT_EQ(nori::resource::get(child_text), child);

    const nori::resource::path relative{"parent/child"};
    EXPECT_EQ(nori::resource::try_get(root, relative), child);
    EXPECT_EQ(nori::resource::try_get(root, "parent/child"), child);
    EXPECT_EQ(nori::resource::try_get(root, nori::resource::path{}), root);
    EXPECT_EQ(nori::resource::try_get(root, "."), root);
    EXPECT_EQ(nori::resource::try_get("implicit.nori::"), root);
}
} // namespace
