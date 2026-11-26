#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

#include "codec_test_helpers.hpp"

namespace
{
TEST_F(loading_test, position_completes_only_parent_child_list)
{
    const auto file{directory_ / "position.nori"};
    const std::vector<fixture_property> properties{
        {0, "branch", nori::resource::empty_t{}},
        {1, "first", std::string{"first value"}},
        {1, "last", std::string{"last value"}},
        {2, "child", std::string{"child value"}},
    };
    ASSERT_TRUE(save_fixture(file, properties));

    const auto last{nori::resource::get("position.nori::branch/last")};
    ASSERT_EQ(nori::resource::position_of(last), 1);
    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(nori::resource::try_get<std::string>(last).error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::position_of(last), 1);
}

TEST_F(loading_test, metadata_and_value_loading_remain_independent)
{
    const auto file{directory_ / "metadata.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", nori::resource::empty_t{}});
        encoded_properties.push_back({1, "first", std::int32_t{42}});
        encoded_properties.push_back({1, "last", std::string{"payload"}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }

    const auto last{nori::resource::get("metadata.nori::branch/last")};
    const auto branch{nori::resource::parent_of(last)};
    const auto children{children_of_or_fail(branch)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(children.back(), last);
    EXPECT_EQ(nori::resource::name_of(children.front()), "first");
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::empty);
    EXPECT_EQ(nori::resource::type_of(last), nori::resource::property_type::string);

    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(nori::resource::get<std::int32_t>(children.front()), 42);
    EXPECT_EQ(nori::resource::try_get<std::string>(branch).error(), nori::resource::error_code::type_mismatch);
    EXPECT_EQ(nori::resource::try_get<std::string>(last).error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::type_of(last), nori::resource::property_type::string);
    EXPECT_EQ(children_of_or_fail(branch), children);

    ASSERT_TRUE(nori::resource::unload(branch));
    EXPECT_EQ(nori::resource::get("metadata.nori::branch"), branch);
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::empty);
    const auto reloaded{children_of_or_fail(branch)};
    ASSERT_EQ(reloaded.size(), 2);
    EXPECT_NE(reloaded.back(), last);
    EXPECT_EQ(nori::resource::name_of(reloaded.back()), "last");
    EXPECT_EQ(nori::resource::get<std::int32_t>(reloaded.front()), 42);
}

TEST_F(loading_test, loading_a_value_keeps_its_child_list_incomplete)
{
    const auto file{directory_ / "value-before-children.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", std::string{"branch payload"}});
        encoded_properties.push_back({1, "first", std::string{"unread payload"}});
        encoded_properties.push_back({1, "last", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4, std::byte{7})}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto last{nori::resource::get("value-before-children.nori::branch/last")};
    const auto branch{nori::resource::parent_of(last)};
    const auto before{nori::resource::get<nori::resource::image_view>(last)};
    EXPECT_EQ(nori::resource::get<std::string>(branch), "branch payload");
    const auto children{children_of_or_fail(branch)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children.front()), "first");
    EXPECT_EQ(children.back(), last);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(last).data.data(), before.data.data());
    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(nori::resource::get<std::string>(branch), "branch payload");
    EXPECT_EQ(children_of_or_fail(branch), children);
    EXPECT_EQ(nori::resource::try_get<std::string>(children.front()).error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(last).data.data(), before.data.data());
}

TEST_F(loading_test, repeated_subtree_unload_preserves_the_target_and_clears_descendants)
{
    const auto file{directory_ / "repeated-unload.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", std::string{"branch payload"}});
        encoded_properties.push_back({1, "child", std::int32_t{42}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto child{nori::resource::get("repeated-unload.nori::branch/child")};
    const auto branch{nori::resource::parent_of(child)};
    const auto root{nori::resource::root_of(branch)};
    ASSERT_TRUE(nori::resource::unload(branch));
    ASSERT_TRUE(nori::resource::unload(branch));
    EXPECT_EQ(nori::resource::get(root, "branch"), branch);
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::string);
    const auto children{children_of_or_fail(branch)};
    ASSERT_EQ(children.size(), 1);
    EXPECT_NE(children.front(), child);
    EXPECT_EQ(nori::resource::get<std::int32_t>(children.front()), 42);
    EXPECT_EQ(nori::resource::get<std::string>(branch), "branch payload");
    ASSERT_TRUE(nori::resource::unload(root));
    ASSERT_TRUE(std::filesystem::remove(file));

    const nori::resource::handle memory_root{nori::resource::create_root()};
    const nori::resource::handle memory{nori::resource::create(memory_root, "memory", std::int32_t{7})};
    EXPECT_TRUE(children_of_or_fail(memory).empty());
    EXPECT_EQ(nori::resource::get<std::int32_t>(memory), 7);
}

TEST_F(loading_test, subtree_load_reads_pool_values_and_leaves_siblings_unloaded)
{
    const auto file{directory_ / "subtree-pool.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "menu", std::string{"menu payload"}});
        encoded_properties.push_back({1, "title", std::string{"title payload"}});
        encoded_properties.push_back({1, "binary", nori::resource::binary{{std::byte{42}}}});
        encoded_properties.push_back({1, "icon", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4, std::byte{7})}});
        encoded_properties.push_back({0, "other", std::string{"other payload"}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto menu{nori::resource::get("subtree-pool.nori::menu")};
    const auto binary{nori::resource::get(menu, "binary")};
    const auto before{nori::resource::get<nori::resource::binary_view>(binary)};
    ASSERT_TRUE(nori::resource::load(menu));
    ASSERT_TRUE(std::filesystem::remove(file));

    EXPECT_EQ(nori::resource::get<std::string>(menu), "menu payload");
    EXPECT_EQ(nori::resource::get<std::string>(menu, "title"), "title payload");
    EXPECT_EQ(nori::resource::get(menu, "binary"), binary);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(binary).data.data(), before.data.data());
    const auto icon{nori::resource::get<nori::resource::image_view>(menu, "icon")};
    EXPECT_EQ(icon.width, 1u);
    EXPECT_EQ(icon.data.size(), 4u);
    EXPECT_EQ(icon.data.front(), std::byte{7});
    const auto other{nori::resource::get(nori::resource::root_of(menu), "other")};
    EXPECT_EQ(nori::resource::type_of(other), nori::resource::property_type::string);
    EXPECT_EQ(nori::resource::try_get<std::string>(other).error(), nori::resource::error_code::io_failure);
}

TEST_F(loading_test, copying_a_partially_loaded_subtree_creates_a_complete_independent_copy)
{
    const auto file{directory_ / "partial-copy.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", std::string{"branch payload"}});
        encoded_properties.push_back({1, "first", std::string{"first payload"}});
        encoded_properties.push_back({1, "nested", nori::resource::empty_t{}});
        encoded_properties.push_back({2, "leaf", std::string{"leaf payload"}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }

    const auto first{nori::resource::get("partial-copy.nori::branch/first")};
    const auto branch{nori::resource::parent_of(first)};
    EXPECT_EQ(nori::resource::get<std::string>(first), "first payload");
    const auto destination{nori::resource::create_root()};
    const auto copy{nori::resource::copy(branch, destination)};
    ASSERT_TRUE(copy);
    EXPECT_EQ(nori::resource::parent_of(*copy), destination);
    EXPECT_EQ(nori::resource::root_of(*copy), destination);
    EXPECT_EQ(nori::resource::path_of(*copy), nori::resource::path{"branch"});

    ASSERT_TRUE(nori::resource::load(*copy));
    EXPECT_EQ(nori::resource::get<std::string>(*copy), "branch payload");
    const auto children{children_of_or_fail(*copy)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children.front()), "first");
    EXPECT_EQ(nori::resource::name_of(children.back()), "nested");
    EXPECT_EQ(nori::resource::get<std::string>(children.front()), "first payload");
    EXPECT_EQ(nori::resource::get<std::string>(children.back(), "leaf"), "leaf payload");
    EXPECT_TRUE(std::filesystem::exists(file));
    EXPECT_TRUE(children_of_or_fail(children.front()).empty());
    EXPECT_TRUE(children_of_or_fail(nori::resource::get(children.back(), "leaf")).empty());
}

TEST_F(loading_test, deep_full_load_preserves_cached_values_and_handles)
{
    constexpr std::uint32_t depth{4096};
    const auto file{directory_ / "deep.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "cached", nori::resource::binary{.data = std::vector<std::byte>(1024 * 1024, std::byte{42})}});
        for (std::uint32_t index{0}; index < depth; ++index)
        {
            encoded_properties.push_back({index, "node", static_cast<std::int32_t>(index)});
        }
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto document{nori::resource::get("deep.nori")};
    const auto cached{nori::resource::get(document, "cached")};
    const auto before{nori::resource::get<nori::resource::binary_view>(cached)};
    const auto branch{nori::resource::get(document, "node/node")};
    ASSERT_TRUE(nori::resource::load(document));
    EXPECT_EQ(nori::resource::get(document, "node/node"), branch);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(cached).data.data(), before.data.data());
    ASSERT_TRUE(std::filesystem::remove(file));
    auto current{nori::resource::get(document, "node")};
    for (std::uint32_t index{0}; index < depth; ++index)
    {
        EXPECT_EQ(nori::resource::get<std::int32_t>(current), static_cast<std::int32_t>(index));
        if (index + 1 < depth)
        {
            current = nori::resource::get(current, "node");
        }
    }
    EXPECT_TRUE(children_of_or_fail(current).empty());
}

TEST_F(loading_test, full_load_matches_values_to_nodes_across_partially_loaded_branches)
{
    const auto file{directory_ / "mixed.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "left", std::int32_t{11}});
        encoded_properties.push_back({1, "cached", nori::resource::binary{.data = {std::byte{1}, std::byte{2}}}});
        encoded_properties.push_back({1, "pending", std::string{"left payload"}});
        encoded_properties.push_back({2, "empty", nori::resource::empty_t{}});
        encoded_properties.push_back({0, "middle", std::int64_t{22}});
        encoded_properties.push_back({1, "first", false});
        encoded_properties.push_back({1, "second", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4, std::byte{3})}});
        encoded_properties.push_back({0, "right", std::string{"right payload"}});
        encoded_properties.push_back({1, "leaf", 3.5f});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }

    const auto leaf{nori::resource::get("mixed.nori::right/leaf")};
    EXPECT_EQ(nori::resource::get<float>(leaf), 3.5f);
    const auto right{nori::resource::parent_of(leaf)};
    EXPECT_EQ(children_of_or_fail(right), std::vector{leaf});
    const auto pending{nori::resource::get("mixed.nori::left/pending")};
    const auto left{nori::resource::parent_of(pending)};
    const auto cached{nori::resource::get(left, "cached")};
    const auto before{nori::resource::get<nori::resource::binary_view>(cached)};
    const auto middle{nori::resource::get("mixed.nori::middle")};
    EXPECT_EQ(nori::resource::get<std::int64_t>(middle), 22);
    const auto root{nori::resource::root_of(left)};

    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(children_of_or_fail(root), (std::vector{left, middle, right}));
    EXPECT_EQ(children_of_or_fail(left), (std::vector{cached, pending}));
    EXPECT_EQ(nori::resource::get<std::int32_t>(left), 11);
    EXPECT_EQ(nori::resource::get<std::string>(pending), "left payload");
    EXPECT_EQ(nori::resource::type_of(nori::resource::get(pending, "empty")), nori::resource::property_type::empty);
    EXPECT_EQ(nori::resource::get<std::int64_t>(middle), 22);
    EXPECT_FALSE(nori::resource::get<bool>(middle, "first"));
    const auto image{nori::resource::get<nori::resource::image_view>(middle, "second")};
    EXPECT_EQ(image.width, 1u);
    EXPECT_EQ(image.data.size(), 4u);
    EXPECT_EQ(image.data.front(), std::byte{3});
    EXPECT_EQ(nori::resource::get<std::string>(right), "right payload");
    EXPECT_EQ(nori::resource::get(right, "leaf"), leaf);
    EXPECT_EQ(nori::resource::get<float>(leaf), 3.5f);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(cached).data.data(), before.data.data());
    EXPECT_TRUE(nori::resource::load(root));
}

TEST_F(loading_test, document_unload_allows_reopen)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle first{nori::resource::create(source, "first", "saved")};
    const nori::resource::handle second{nori::resource::create(source, "second", nori::resource::empty)};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(nori::resource::save_as(source, "tree.nori"));
    nori::resource::remove(source);

    const auto loaded{nori::resource::get("tree.nori")};
    const auto children{children_of_or_fail(loaded)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children.front()), "first");
    EXPECT_EQ(nori::resource::name_of(children.back()), "second");
    ASSERT_TRUE(nori::resource::unload(loaded));
    const auto reloaded_root{nori::resource::get("tree.nori")};
    ASSERT_NE(reloaded_root, loaded);
    ASSERT_TRUE(nori::resource::load(reloaded_root));
    const auto reloaded{children_of_or_fail(reloaded_root)};
    ASSERT_EQ(reloaded.size(), 2);
    EXPECT_NE(reloaded.front(), children.front());
    EXPECT_EQ(nori::resource::get<std::string>(reloaded.front()), "saved");
}

TEST_F(loading_test, ancestor_reuse)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(source, "branch", nori::resource::empty)};
    const nori::resource::handle value{nori::resource::create(branch, "value", std::int32_t{42})};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(source, "tree.nori"));
    nori::resource::remove(source);

    const auto loaded_value{nori::resource::get("tree.nori::branch/value")};
    const auto loaded_branch{nori::resource::parent_of(loaded_value)};
    const auto root{nori::resource::parent_of(loaded_branch)};
    EXPECT_EQ(nori::resource::get(root, "branch"), loaded_branch);
    EXPECT_EQ(nori::resource::get(loaded_branch, "value"), loaded_value);
    ASSERT_TRUE(nori::resource::unload(loaded_value));
    EXPECT_EQ(nori::resource::get(loaded_value, nori::resource::path{}), loaded_value);
    EXPECT_EQ(nori::resource::get<std::int32_t>(loaded_value), 42);
    EXPECT_FALSE(nori::resource::try_get(root, "branch/branch"));
}

TEST_F(loading_test, explicit_load_preserves_changes_and_loads_all_values)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle first{nori::resource::create(document, "first", std::string_view{"original"})};
    const nori::resource::handle second{nori::resource::create(document, "second", std::string_view{"payload"})};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(nori::resource::save_as(document, "full-load.nori"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded{nori::resource::get("full-load.nori")};
    const auto edited{nori::resource::get(loaded, "first")};
    ASSERT_TRUE(nori::resource::load(loaded));
    nori::resource::set(edited, std::string_view{"edited"});
    ASSERT_TRUE(nori::resource::load(loaded));
    ASSERT_TRUE(nori::resource::load(loaded));
    ASSERT_TRUE(std::filesystem::remove(directory_ / "full-load.nori"));
    EXPECT_EQ(nori::resource::get<std::string>(edited), "edited");
    EXPECT_EQ(nori::resource::get<std::string>(loaded, "second"), "payload");
}

TEST_F(loading_test, copied_subtree_survives_source_document_unload)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(source, "parent", std::int32_t{7})};
    const nori::resource::handle child{nori::resource::create(parent, "child", "preserved")};
    ASSERT_TRUE(parent);
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(source, "source.nori"));
    ASSERT_TRUE(nori::resource::save_as(destination, "destination.nori"));
    ASSERT_TRUE(nori::resource::unload(source));
    ASSERT_TRUE(nori::resource::unload(destination));

    const auto loaded_source{nori::resource::get("source.nori")};
    const auto loaded_destination{nori::resource::get("destination.nori")};
    ASSERT_TRUE(nori::resource::load(loaded_source));
    ASSERT_TRUE(nori::resource::load(loaded_destination));
    const auto loaded_parent{nori::resource::get(loaded_source, "parent")};
    const auto copied_parent{nori::resource::copy(loaded_parent, loaded_destination)};
    ASSERT_TRUE(copied_parent);
    const nori::resource::handle copied_child{nori::resource::get(*copied_parent, "child")};
    nori::resource::set(copied_child, "edited");
    ASSERT_TRUE(nori::resource::unload(loaded_source));
    const auto reloaded_parent{nori::resource::get("source.nori::parent")};
    EXPECT_NE(reloaded_parent, loaded_parent);
    EXPECT_EQ(nori::resource::get<std::string>(reloaded_parent, "child"), "preserved");
    ASSERT_TRUE(nori::resource::save(loaded_destination));
    ASSERT_TRUE(nori::resource::unload(loaded_destination));
    EXPECT_EQ(nori::resource::get<std::int32_t>("destination.nori::parent"), 7);
    EXPECT_EQ(nori::resource::get<std::string>("destination.nori::parent/child"), "edited");
}

TEST_F(loading_test, subtree_unload_reloads_file_contents)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(document, "parent", std::int32_t{7})};
    const nori::resource::handle child{nori::resource::create(parent, "child", "preserved")};
    ASSERT_TRUE(parent);
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(document, "subtree.nori"));
    ASSERT_TRUE(nori::resource::unload(document));

    const auto loaded_parent{nori::resource::get("subtree.nori::parent")};
    const auto loaded_child{nori::resource::get(loaded_parent, "child")};
    EXPECT_EQ(nori::resource::get<std::int32_t>(loaded_parent), 7);
    EXPECT_EQ(nori::resource::get<std::string>(loaded_child), "preserved");
    ASSERT_TRUE(nori::resource::unload(loaded_parent));
    EXPECT_EQ(nori::resource::get("subtree.nori::parent"), loaded_parent);
    EXPECT_EQ(nori::resource::type_of(loaded_parent), nori::resource::property_type::int32);
    EXPECT_EQ(nori::resource::get<std::int32_t>(loaded_parent), 7);
    const auto reloaded_child{nori::resource::get(loaded_parent, "child")};
    EXPECT_NE(reloaded_child, loaded_child);
    EXPECT_EQ(nori::resource::get<std::string>(reloaded_child), "preserved");
    EXPECT_EQ(children_of_or_fail(loaded_parent), (std::vector{reloaded_child}));
    ASSERT_TRUE(nori::resource::unload(loaded_parent));
    EXPECT_EQ(nori::resource::get<std::string>(loaded_parent, "child"), "preserved");
}

TEST_F(loading_test, subtree_unload_preserves_other_subtrees)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle first{nori::resource::create(document, "first", std::int32_t{7})};
    const nori::resource::handle second{nori::resource::create(document, "second", std::int32_t{8})};
    const nori::resource::handle child{nori::resource::create(second, "child", "other subtree")};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(document, "siblings.nori"));
    ASSERT_TRUE(nori::resource::unload(document));

    const auto loaded{nori::resource::get("siblings.nori")};
    const auto loaded_first{nori::resource::get(loaded, "first")};
    const auto loaded_second{nori::resource::get(loaded, "second")};
    const auto loaded_child{nori::resource::get(loaded_second, "child")};
    EXPECT_EQ(nori::resource::get<std::string>(loaded_child), "other subtree");
    ASSERT_TRUE(nori::resource::unload(loaded_first));
    EXPECT_EQ(children_of_or_fail(loaded), (std::vector{loaded_first, loaded_second}));
    EXPECT_EQ(nori::resource::get(loaded_second, "child"), loaded_child);
    ASSERT_TRUE(std::filesystem::remove(directory_ / "siblings.nori"));
    EXPECT_EQ(nori::resource::get<std::string>(loaded_child), "other subtree");
}

TEST_F(loading_test, full_load_preserves_subtree_unload)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(document, "parent", "parent value")};
    const nori::resource::handle child{nori::resource::create(parent, "child", "child value")};
    ASSERT_TRUE(parent);
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(document, "full-read.nori"));
    ASSERT_TRUE(nori::resource::unload(document));

    const auto loaded{nori::resource::get("full-read.nori")};
    ASSERT_TRUE(nori::resource::load(loaded));
    const auto loaded_parent{nori::resource::get(loaded, "parent")};
    const auto loaded_child{nori::resource::get(loaded_parent, "child")};
    ASSERT_TRUE(nori::resource::unload(loaded_parent));
    const auto reloaded_child{nori::resource::get(loaded_parent, "child")};
    EXPECT_NE(reloaded_child, loaded_child);
    EXPECT_EQ(nori::resource::get<std::string>(loaded_parent), "parent value");
    EXPECT_EQ(nori::resource::get<std::string>(reloaded_child), "child value");
}

TEST_F(loading_test, queries_do_not_decode_descendants)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(document, "parent", std::int32_t{7})};
    const nori::resource::handle child{nori::resource::create(parent, "broken", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)})};
    ASSERT_TRUE(parent);
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(document, "lazy-value.nori"));
    nori::resource::shutdown();
    const auto file{directory_ / "lazy-value.nori"};
    std::filesystem::resize_file(file, std::filesystem::file_size(file) - 1);
    ASSERT_TRUE(nori::resource::initialize(directory_));
    const auto loaded{nori::resource::try_get("lazy-value.nori::parent")};
    ASSERT_TRUE(loaded) << (loaded ? "" : nori::resource::to_string(loaded.error()));
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded), 7);
    const auto children{children_of_or_fail(*loaded)};
    ASSERT_EQ(children.size(), 1);
    EXPECT_EQ(nori::resource::type_of(children[0]), nori::resource::property_type::image);
    EXPECT_EQ(nori::resource::try_get<nori::resource::image>(children[0]).error(), nori::resource::error_code::io_failure);
    const auto loaded_document{nori::resource::parent_of(*loaded)};
    const auto failed_load{nori::resource::load(loaded_document)};
    ASSERT_FALSE(failed_load);
    EXPECT_EQ(failed_load.error(), nori::resource::error_code::io_failure);
}

TEST_F(loading_test, child_name_lookup_tracks_runtime_changes)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(document, "value", std::int32_t{7})};
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(document, "names.nori"));
    EXPECT_EQ(nori::resource::get(document, "value"), value);
    ASSERT_TRUE(nori::resource::rename(value, "renamed"));
    EXPECT_FALSE(nori::resource::try_get(document, "value"));
    EXPECT_EQ(nori::resource::get(document, "renamed"), value);
    const nori::resource::handle added{nori::resource::create(document, "value", std::int32_t{9})};
    ASSERT_TRUE(added);
    EXPECT_EQ(nori::resource::get(document, "value"), added);
    nori::resource::remove(added);
    EXPECT_FALSE(nori::resource::try_get(document, "value"));
    const auto holding{nori::resource::create(document, "holding", nori::resource::empty)};
    ASSERT_TRUE(nori::resource::move(value, holding));
    EXPECT_FALSE(nori::resource::try_get(document, "renamed"));
    ASSERT_TRUE(nori::resource::move(value, document));
    EXPECT_EQ(nori::resource::get(document, "renamed"), value);
    ASSERT_TRUE(nori::resource::unload(document));
    EXPECT_EQ(nori::resource::get<std::int32_t>("names.nori::value"), 7);
}

TEST_F(loading_test, set_existing_file)
{
    const nori::resource::handle root{create_resource("existing.nori")};
    const nori::resource::handle preserved{create_resource("existing.nori::preserved")};
    nori::resource::set(preserved, std::int32_t{1});
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const nori::resource::handle added{create_resource("existing.nori::added")};
    nori::resource::set(added, std::int32_t{2});
    EXPECT_EQ(nori::resource::get<std::int32_t>("existing.nori::preserved"), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>("existing.nori::added"), 2);
}

TEST_F(loading_test, create_existing_file)
{
    const nori::resource::handle root{create_resource("create-existing.nori")};
    const nori::resource::handle preserved{create_resource("create-existing.nori::preserved")};
    nori::resource::set(preserved, std::int32_t{1});
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto reloaded_root{nori::resource::try_get("create-existing.nori")};
    ASSERT_TRUE(reloaded_root);
    ASSERT_TRUE(nori::resource::load(*reloaded_root));
    const nori::resource::handle added{nori::resource::create(*reloaded_root, "added", nori::resource::empty)};
    ASSERT_TRUE(added);
    nori::resource::set(added, std::int32_t{2});
    ASSERT_TRUE(nori::resource::load(*reloaded_root));
    ASSERT_TRUE(nori::resource::save(*reloaded_root));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    EXPECT_EQ(nori::resource::get<std::int32_t>("create-existing.nori::preserved"), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>("create-existing.nori::added"), 2);
}

TEST_F(loading_test, empty_file)
{
    const nori::resource::handle root{create_resource("empty-root.nori")};
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto reloaded{nori::resource::try_get("empty-root.nori")};
    ASSERT_TRUE(reloaded);
    EXPECT_TRUE(children_of_or_fail(*reloaded).empty());
}

TEST_F(loading_test, empty_child_lists_remain_complete_across_unload_and_full_load)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(document, "branch", nori::resource::empty)};
    const nori::resource::handle vacant{nori::resource::create(document, "vacant", nori::resource::empty)};
    const nori::resource::handle leaf{nori::resource::create(branch, "leaf", "payload")};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(vacant);
    ASSERT_TRUE(leaf);
    ASSERT_TRUE(nori::resource::save_as(document, "empty-children.nori"));
    ASSERT_TRUE(nori::resource::unload(document));

    const auto root{nori::resource::get("empty-children.nori")};
    const auto children{children_of_or_fail(root)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children.front()), "branch");
    EXPECT_EQ(nori::resource::name_of(children.back()), "vacant");
    EXPECT_TRUE(children_of_or_fail(children.back()).empty());
    ASSERT_TRUE(nori::resource::unload(children.back()));
    EXPECT_TRUE(children_of_or_fail(children.back()).empty());
    ASSERT_TRUE(nori::resource::load(root));
    EXPECT_EQ(nori::resource::get<std::string>(children.front(), "leaf"), "payload");

    ASSERT_TRUE(nori::resource::unload(children.front()));
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(std::filesystem::remove(directory_ / "empty-children.nori"));
    ASSERT_TRUE(nori::resource::load(root));
    EXPECT_EQ(children_of_or_fail(root), children);
    EXPECT_TRUE(children_of_or_fail(children.back()).empty());
    EXPECT_TRUE(children_of_or_fail(nori::resource::get(children.front(), "leaf")).empty());
    EXPECT_EQ(nori::resource::get<std::string>(children.front(), "leaf"), "payload");
}

TEST_F(loading_test, unbound_subtree_cannot_be_unloaded)
{
    const nori::resource::handle owner_root{nori::resource::create_root()};
    const nori::resource::handle root{nori::resource::create(owner_root, "root", std::int32_t{7})};
    const nori::resource::handle child{nori::resource::create(root, "child", std::int32_t{42})};
    ASSERT_TRUE(root);
    ASSERT_TRUE(child);

    const auto unloaded{nori::resource::unload(root)};
    ASSERT_FALSE(unloaded);
    EXPECT_EQ(unloaded.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(root), 7);
    EXPECT_EQ(nori::resource::get(root, "child"), child);
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 42);
    const auto child_unloaded{nori::resource::unload(child)};
    ASSERT_FALSE(child_unloaded);
    EXPECT_EQ(child_unloaded.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 42);
}

TEST_F(loading_test, document_unload_invalidates_all_handles)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(document, "child", std::int32_t{7})};
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(document, "unload-root.nori"));
    ASSERT_TRUE(nori::resource::unload(document));
    const auto loaded{nori::resource::get("unload-root.nori")};
    const auto loaded_child{nori::resource::get(loaded, "child")};
    EXPECT_NE(loaded, document);
    EXPECT_NE(loaded_child, child);
    EXPECT_EQ(nori::resource::get<std::int32_t>(loaded_child), 7);
    const nori::resource::handle created{nori::resource::create_root()};
    nori::resource::remove(created);
}

TEST_F(loading_test, materialized_missing_path_is_not_reloaded)
{
    const nori::resource::handle root{create_resource("runtime-remove.nori")};
    const nori::resource::handle child{create_resource("runtime-remove.nori::child")};
    nori::resource::set(child, std::int32_t{7});
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));

    nori::resource::remove(child);
    const auto missing{nori::resource::try_get("runtime-remove.nori::child")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);

    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));
    EXPECT_EQ(nori::resource::try_get("runtime-remove.nori::child").error(), nori::resource::error_code::not_found);
}

TEST_F(loading_test, lazy_handle)
{
    nori::resource::handle root{create_resource("handle-load.nori")};
    nori::resource::handle branch{create_resource("handle-load.nori::branch")};
    nori::resource::handle value{create_resource("handle-load.nori::branch/value")};
    nori::resource::set(branch, std::int32_t{7});
    nori::resource::set(value, std::string_view{"preserved"});
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));
    root = nori::resource::get("handle-load.nori");
    branch = nori::resource::get(root, "branch");
    value = nori::resource::get(root, "branch/value");

    ASSERT_TRUE(nori::resource::unload(branch));
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::int32);
    EXPECT_EQ(nori::resource::get<std::int32_t>(branch), 7);

    const std::vector<nori::resource::handle> loaded_children{children_of_or_fail(branch)};
    ASSERT_EQ(loaded_children.size(), 1u);
    EXPECT_NE(loaded_children.front(), value);
    EXPECT_EQ(nori::resource::get<std::string>(loaded_children.front()), "preserved");
}

TEST_F(loading_test, lazy_sibling_order)
{
    nori::resource::handle root{create_resource("unload-order.nori")};
    nori::resource::handle first{create_resource("unload-order.nori::first")};
    nori::resource::handle middle{create_resource("unload-order.nori::middle")};
    nori::resource::handle last{create_resource("unload-order.nori::last")};
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));
    root = nori::resource::get("unload-order.nori");
    first = nori::resource::get(root, "first");
    middle = nori::resource::get(root, "middle");
    last = nori::resource::get(root, "last");

    ASSERT_TRUE(nori::resource::unload(middle));
    const std::vector<nori::resource::handle> unloaded_children{children_of_or_fail(root)};
    ASSERT_EQ(unloaded_children.size(), 3u);
    EXPECT_EQ(unloaded_children[0], first);
    EXPECT_EQ(unloaded_children[1], middle);
    EXPECT_EQ(unloaded_children[2], last);

    const auto reloaded_middle{nori::resource::try_get("unload-order.nori::middle")};
    ASSERT_TRUE(reloaded_middle);
    EXPECT_EQ(*reloaded_middle, middle);

    const std::vector<nori::resource::handle> restored_children{children_of_or_fail(root)};
    ASSERT_EQ(restored_children.size(), 3u);
    EXPECT_EQ(restored_children[0], first);
    EXPECT_EQ(restored_children[1], *reloaded_middle);
    EXPECT_EQ(restored_children[2], last);
    ASSERT_TRUE(nori::resource::load(root));
    EXPECT_TRUE(nori::resource::save(root));
}

TEST_F(loading_test, partial_parent)
{
    nori::resource::handle root{create_resource("unload-multiple.nori")};
    nori::resource::handle first{create_resource("unload-multiple.nori::first")};
    nori::resource::handle second{create_resource("unload-multiple.nori::second")};
    nori::resource::handle third{create_resource("unload-multiple.nori::third")};
    nori::resource::handle fourth{create_resource("unload-multiple.nori::fourth")};
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));
    root = nori::resource::get("unload-multiple.nori");
    first = nori::resource::get(root, "first");
    second = nori::resource::get(root, "second");
    third = nori::resource::get(root, "third");
    fourth = nori::resource::get(root, "fourth");

    ASSERT_TRUE(nori::resource::unload(second));
    ASSERT_TRUE(nori::resource::unload(fourth));

    const auto reloaded_second{nori::resource::try_get("unload-multiple.nori::second")};
    ASSERT_TRUE(reloaded_second);
    EXPECT_EQ(*reloaded_second, second);

    ASSERT_TRUE(nori::resource::load(root));
    const auto loaded_save{nori::resource::save(root)};
    ASSERT_TRUE(loaded_save);

    const auto reloaded_fourth{nori::resource::try_get("unload-multiple.nori::fourth")};
    ASSERT_TRUE(reloaded_fourth);
    EXPECT_EQ(*reloaded_fourth, fourth);

    const std::vector<nori::resource::handle> children{children_of_or_fail(root)};
    ASSERT_EQ(children.size(), 4u);
    EXPECT_EQ(children[0], first);
    EXPECT_EQ(children[1], *reloaded_second);
    EXPECT_EQ(children[2], third);
    EXPECT_EQ(children[3], *reloaded_fourth);
    ASSERT_TRUE(nori::resource::load(root));
    EXPECT_TRUE(nori::resource::save(root));
}

TEST_F(loading_test, lazy_mutation)
{
    const nori::resource::handle root{create_resource("partial-operations.nori")};
    const nori::resource::handle source{create_resource("partial-operations.nori::source")};
    const nori::resource::handle child{create_resource("partial-operations.nori::source/child")};
    create_resource("partial-operations.nori::destination");
    nori::resource::set(source, std::int32_t{7});
    nori::resource::set(child, std::string_view{"preserved"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "partial-operations.nori"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_child{nori::resource::try_get("partial-operations.nori::source/child")};
    ASSERT_TRUE(loaded_child);
    const nori::resource::handle loaded_source{nori::resource::parent_of(*loaded_child)};
    const nori::resource::handle loaded_root{nori::resource::parent_of(loaded_source)};
    const auto loaded_destination{nori::resource::try_get("partial-operations.nori::destination")};
    ASSERT_TRUE(loaded_destination);

    ASSERT_TRUE(nori::resource::load(loaded_root));
    ASSERT_TRUE(nori::resource::rename(loaded_source, "renamed"));
    const auto old_name{nori::resource::try_get("partial-operations.nori::source")};
    ASSERT_FALSE(old_name);
    EXPECT_EQ(old_name.error(), nori::resource::error_code::not_found);
    EXPECT_EQ(nori::resource::get<std::string>("partial-operations.nori::renamed/child"), "preserved");

    const auto copied{nori::resource::copy(loaded_source, *loaded_destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::parent_of(*copied), *loaded_destination);
    nori::resource::remove(*copied);

    ASSERT_TRUE(nori::resource::move(loaded_source, *loaded_destination));
    EXPECT_EQ(nori::resource::get("partial-operations.nori::destination/renamed"), loaded_source);
    EXPECT_EQ(nori::resource::get<std::string>("partial-operations.nori::destination/renamed/child"), "preserved");
    ASSERT_TRUE(nori::resource::load(loaded_root));
    EXPECT_TRUE(nori::resource::save(loaded_root));
}

TEST_F(loading_test, reload)
{
    const nori::resource::handle root{create_resource("reload-state.nori")};
    const nori::resource::handle parent{create_resource("reload-state.nori::parent")};
    const nori::resource::handle child{create_resource("reload-state.nori::parent/child")};
    nori::resource::set(parent, std::int32_t{7});
    nori::resource::set(child, std::string_view{"value"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "reload-state.nori"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_root{nori::resource::try_get("reload-state.nori")};
    ASSERT_TRUE(loaded_root);
    const auto loaded_parent{nori::resource::try_get("reload-state.nori::parent")};
    ASSERT_TRUE(loaded_parent);
    ASSERT_TRUE(nori::resource::unload(*loaded_parent));

    ASSERT_TRUE(nori::resource::load(*loaded_root));
    ASSERT_TRUE(nori::resource::save(*loaded_root));

    const auto reloaded_parent{nori::resource::try_get("reload-state.nori::parent")};
    ASSERT_TRUE(reloaded_parent);
    EXPECT_EQ(*reloaded_parent, *loaded_parent);
    ASSERT_TRUE(nori::resource::load(*loaded_root));
    ASSERT_TRUE(nori::resource::save(*loaded_root));
}

TEST_F(loading_test, reload_failure)
{
    nori::resource::handle root{create_resource("placeholder-reload.nori")};
    nori::resource::handle child{create_resource("placeholder-reload.nori::child")};
    nori::resource::set(child, std::string_view{"saved"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "placeholder-reload.nori"));
    ASSERT_TRUE(nori::resource::unload(root));
    root = nori::resource::get("placeholder-reload.nori");
    child = nori::resource::get(root, "child");

    ASSERT_TRUE(nori::resource::unload(child));

    const std::filesystem::path file{directory_ / "placeholder-reload.nori"};
    {
        std::ofstream out{file, std::ios::binary};
        constexpr std::array invalid_signature{'b', 'a', 'd', '!'};
        out.write(invalid_signature.data(), invalid_signature.size());
    }

    const auto failed{nori::resource::try_get<std::string>(child)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::parent_of(child), root);

    write_single_property_file(
        file,
        1,
        0,
        static_cast<std::uint8_t>(nori::resource::property_type::int32),
        "child",
        42
    );
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));
    const auto reloaded{nori::resource::try_get("placeholder-reload.nori::child")};
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(nori::resource::type_of(*reloaded), nori::resource::property_type::int32);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*reloaded), 42);
}

TEST_F(loading_test, same_basename_documents)
{
    std::filesystem::create_directories(directory_ / "first");
    std::filesystem::create_directories(directory_ / "second");
    const nori::resource::handle first{nori::resource::create_root()};
    const nori::resource::handle second{nori::resource::create_root()};
    const nori::resource::handle first_value{nori::resource::create(first, "value", std::int32_t{1})};
    const nori::resource::handle second_value{nori::resource::create(second, "value", std::int32_t{2})};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(first_value);
    ASSERT_TRUE(second_value);
    ASSERT_TRUE(nori::resource::save_as(first, "first/shared.nori"));
    ASSERT_TRUE(nori::resource::save_as(second, "second/shared.nori"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_first{nori::resource::try_get("first/shared.nori")};
    const auto loaded_second{nori::resource::try_get("second/shared.nori")};
    ASSERT_TRUE(loaded_first);
    ASSERT_TRUE(loaded_second);
    EXPECT_NE(*loaded_first, *loaded_second);
    EXPECT_FALSE(nori::resource::parent_of(*loaded_first));
    EXPECT_FALSE(nori::resource::parent_of(*loaded_second));
    EXPECT_EQ(nori::resource::path_of(*loaded_first), bound_resource_path(directory_ / "first/shared.nori"));
    EXPECT_EQ(nori::resource::path_of(*loaded_second), bound_resource_path(directory_ / "second/shared.nori"));
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded_first, "value"), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded_second, "value"), 2);
    EXPECT_EQ(nori::resource::get("first/../first/shared.nori"), *loaded_first);
    EXPECT_EQ(nori::resource::get("second/../second/shared.nori"), *loaded_second);
}

TEST_F(loading_test, lazy_order)
{
    const nori::resource::handle root{create_resource("lazy.nori")};
    const nori::resource::handle top_first{create_resource("lazy.nori::top_first")};
    const nori::resource::handle parent{create_resource("lazy.nori::parent")};
    const nori::resource::handle top_last{create_resource("lazy.nori::top_last")};
    const nori::resource::handle first{create_resource("lazy.nori::parent/first")};
    const nori::resource::handle second{create_resource("lazy.nori::parent/second")};
    const nori::resource::handle third{create_resource("lazy.nori::parent/third")};
    nori::resource::set(top_first, std::string{"top_first"});
    nori::resource::set(parent, std::int32_t{7});
    nori::resource::set(top_last, std::string{"top_last"});
    nori::resource::set(first, std::string{"first"});
    nori::resource::set(second, std::string{"second"});
    nori::resource::set(third, std::string{"third"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "lazy.nori"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_second{nori::resource::try_get("lazy.nori::parent/second")};
    ASSERT_TRUE(loaded_second);
    EXPECT_EQ(nori::resource::get<std::string>(*loaded_second), "second");

    const nori::resource::handle placeholder_parent{nori::resource::parent_of(*loaded_second)};
    const auto loaded_parent{nori::resource::try_get("lazy.nori::parent")};
    ASSERT_TRUE(loaded_parent);
    EXPECT_EQ(*loaded_parent, placeholder_parent);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded_parent), 7);

    const std::vector<nori::resource::handle> loaded_children{children_of_or_fail(*loaded_parent)};
    ASSERT_EQ(loaded_children.size(), 3);
    EXPECT_EQ(nori::resource::name_of(loaded_children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(loaded_children[1]), "second");
    EXPECT_EQ(nori::resource::name_of(loaded_children[2]), "third");

    const auto loaded_root{nori::resource::try_get("lazy.nori")};
    ASSERT_TRUE(loaded_root);
    const std::vector<nori::resource::handle> top_level{children_of_or_fail(*loaded_root)};
    ASSERT_EQ(top_level.size(), 3);
    EXPECT_EQ(nori::resource::name_of(top_level[0]), "top_first");
    EXPECT_EQ(nori::resource::name_of(top_level[1]), "parent");
    EXPECT_EQ(nori::resource::name_of(top_level[2]), "top_last");
}

TEST_F(loading_test, reverse_lazy_lookup_preserves_handles_and_views_when_completing_children)
{
    constexpr std::size_t child_count{256};
    constexpr std::size_t missing_index{child_count / 2};
    const auto file{directory_ / "reverse.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", nori::resource::empty_t{}});
        encoded_properties.push_back({1, "child_0", nori::resource::binary{.data = {std::byte{42}}}});
        for (std::size_t index{1}; index < child_count; ++index)
        {
            encoded_properties.push_back({1, "child_" + std::to_string(index), static_cast<std::int32_t>(index)});
        }
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }

    for (const bool full_load : {false, true})
    {
        const auto root{nori::resource::get("reverse.nori")};
        const auto branch{nori::resource::get(root, "branch")};
        std::vector<nori::resource::handle> existing(child_count);
        for (std::size_t index{child_count}; index > 0; --index)
        {
            if (index - 1 != missing_index)
            {
                existing[index - 1] = nori::resource::get(branch, "child_" + std::to_string(index - 1));
            }
        }
        const auto before{nori::resource::get<nori::resource::binary_view>(existing.front())};
        if (full_load)
        {
            ASSERT_TRUE(nori::resource::load(root));
        }
        const auto children{children_of_or_fail(branch)};
        ASSERT_EQ(children.size(), child_count);
        for (std::size_t index{0}; index < child_count; ++index)
        {
            EXPECT_EQ(nori::resource::name_of(children[index]), "child_" + std::to_string(index));
            EXPECT_EQ(nori::resource::parent_of(children[index]), branch);
            if (existing[index])
            {
                EXPECT_EQ(children[index], existing[index]);
            }
            if (index != 0)
            {
                EXPECT_EQ(nori::resource::get<std::int32_t>(children[index]), static_cast<std::int32_t>(index));
            }
        }
        EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(children.front()).data.data(), before.data.data());
        EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(children.front()).data.front(), std::byte{42});
        EXPECT_EQ(children_of_or_fail(branch), children);
        EXPECT_EQ(nori::resource::get(branch, "child_" + std::to_string(missing_index)), children[missing_index]);
        ASSERT_TRUE(nori::resource::unload(root));
    }
}

TEST_F(loading_test, concurrent_lazy_reads_merge_children_and_preserve_borrowed_views)
{
    constexpr std::size_t worker_count{8};
    const auto file{directory_ / "concurrent-lazy.nori"};
    std::vector<fixture_property> properties{
        {0, "branch", std::int32_t{7}},
        {1, "binary", nori::resource::binary{.data = {std::byte{1}, std::byte{2}, std::byte{3}}}},
        {1, "image", nori::resource::image{
                         .format = nori::resource::image_format::rgba8_unorm,
                         .width = 1,
                         .height = 1,
                         .mip_levels = 1,
                         .data = {std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}},
                     }},
    };
    for (std::size_t index{0}; index < 32; ++index)
    {
        properties.push_back({1, "child_" + std::to_string(index), static_cast<std::int32_t>(index)});
    }
    ASSERT_TRUE(save_fixture(file, properties));

    const auto other_file{directory_ / "concurrent-other.nori"};
    std::vector<fixture_property> other_properties{{0, "branch", std::int32_t{11}}};
    for (std::size_t index{0}; index < 32; ++index)
    {
        other_properties.push_back({1, "other_" + std::to_string(index), static_cast<std::int32_t>(index)});
    }
    ASSERT_TRUE(save_fixture(other_file, other_properties));

    const nori::resource::handle branch{nori::resource::get("concurrent-lazy.nori::branch")};
    const nori::resource::handle other_branch{nori::resource::get("concurrent-other.nori::branch")};
    const nori::resource::handle binary{nori::resource::get(branch, "binary")};
    const nori::resource::binary_view before{nori::resource::get<nori::resource::binary_view>(binary)};
    const std::vector<std::byte> before_data{before.data.begin(), before.data.end()};
    const nori::resource::handle image{nori::resource::get(branch, "image")};
    const nori::resource::image_view image_before{nori::resource::get<nori::resource::image_view>(image)};
    const std::vector<std::byte> image_before_data{image_before.data.begin(), image_before.data.end()};

    std::barrier start{static_cast<std::ptrdiff_t>(worker_count)};
    std::array<nori::resource::result<nori::resource::handle>, worker_count> handles;
    std::array<nori::resource::handle, worker_count> parents;
    std::array<std::vector<nori::resource::handle>, worker_count> child_lists;
    std::array<std::int32_t, worker_count> values{};
    std::array<std::thread, worker_count> workers;
    for (std::size_t index{0}; index < worker_count; ++index)
    {
        workers[index] = std::thread{[&, index]()
        {
            start.arrive_and_wait();
            parents[index] = index % 2 == 0 ? branch : other_branch;
            const std::string_view child_name{index % 2 == 0 ? "child_0" : "other_0"};
            handles[index] = nori::resource::try_get(parents[index], child_name);
            child_lists[index] = children_of_or_fail(parents[index]);
            values[index] = nori::resource::get<std::int32_t>(parents[index]);
        }};
    }
    for (auto& worker : workers)
    {
        worker.join();
    }

    for (std::size_t index{0}; index < worker_count; ++index)
    {
        ASSERT_TRUE(handles[index]);
        EXPECT_EQ(nori::resource::name_of(*handles[index]), index % 2 == 0 ? "child_0" : "other_0");
        EXPECT_EQ(*handles[index], *handles[index % 2 == 0 ? 0 : 1]);
        EXPECT_EQ(values[index], index % 2 == 0 ? 7 : 11);
        ASSERT_EQ(child_lists[index].size(), index % 2 == 0 ? 34u : 32u);
        if (index % 2 == 0)
        {
            EXPECT_EQ(child_lists[index][0], binary);
            EXPECT_EQ(child_lists[index][1], image);
        }
        for (std::size_t child_index{0}; child_index < child_lists[index].size(); ++child_index)
        {
            const std::string expected_name{index % 2 == 0 ? (child_index == 0 ? "binary" : child_index == 1 ? "image" :
                                                                                                               "child_" + std::to_string(child_index - 2)) :
                                                             "other_" + std::to_string(child_index)};
            EXPECT_EQ(nori::resource::name_of(child_lists[index][child_index]), expected_name);
        }
    }
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(binary).data.data(), before.data.data());
    EXPECT_TRUE(std::ranges::equal(nori::resource::get<nori::resource::binary_view>(binary).data, before_data));
    const nori::resource::image_view image_after{nori::resource::get<nori::resource::image_view>(image)};
    EXPECT_EQ(image_after.data.data(), image_before.data.data());
    EXPECT_EQ(image_after.format, image_before.format);
    EXPECT_EQ(image_after.width, image_before.width);
    EXPECT_EQ(image_after.height, image_before.height);
    EXPECT_EQ(image_after.mip_levels, image_before.mip_levels);
    EXPECT_TRUE(std::ranges::equal(image_after.data, image_before_data));
}

TEST_F(loading_test, runtime_changes)
{
    const nori::resource::handle root{create_resource("lazy-modified.nori")};
    const nori::resource::handle parent{create_resource("lazy-modified.nori::parent")};
    const nori::resource::handle first{create_resource("lazy-modified.nori::parent/first")};
    const nori::resource::handle second{create_resource("lazy-modified.nori::parent/second")};
    nori::resource::set(parent, std::int32_t{7});
    nori::resource::set(first, std::string{"first"});
    nori::resource::set(second, std::string{"second"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "lazy-modified.nori"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_second{nori::resource::try_get("lazy-modified.nori::parent/second")};
    ASSERT_TRUE(loaded_second);
    ASSERT_TRUE(nori::resource::load(nori::resource::get("lazy-modified.nori")));
    nori::resource::set(*loaded_second, std::string_view{"modified"});

    const nori::resource::handle placeholder_parent{nori::resource::parent_of(*loaded_second)};
    nori::resource::set(placeholder_parent, std::int32_t{99});

    const auto loaded_root{nori::resource::try_get("lazy-modified.nori")};
    ASSERT_TRUE(loaded_root);
    EXPECT_EQ(nori::resource::get<std::int32_t>(placeholder_parent), 99);
    EXPECT_EQ(nori::resource::get<std::string>(*loaded_second), "modified");

    const std::vector<nori::resource::handle> loaded_children{children_of_or_fail(placeholder_parent)};
    ASSERT_EQ(loaded_children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(loaded_children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(loaded_children[1]), "second");
    EXPECT_EQ(nori::resource::get<std::string>(loaded_children[0]), "first");
}

TEST_F(loading_test, parent_navigation)
{
    const nori::resource::handle root{create_resource("lazy-dotdot.nori")};
    const nori::resource::handle child{create_resource("lazy-dotdot.nori::child")};
    const nori::resource::handle nested{create_resource("lazy-dotdot.nori::child/nested")};
    const nori::resource::handle sibling{create_resource("lazy-dotdot.nori::sibling")};
    nori::resource::set(child, std::string_view{"child"});
    nori::resource::set(nested, std::int32_t{42});
    nori::resource::set(sibling, std::string_view{"sibling"});
    ASSERT_TRUE(nori::resource::save_as(root, directory_ / "lazy-dotdot.nori"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto fresh_missing{nori::resource::try_get("lazy-dotdot.nori::child/missing/../..")};
    ASSERT_FALSE(fresh_missing);
    EXPECT_EQ(fresh_missing.error(), nori::resource::error_code::not_found);

    const auto fresh_parent{nori::resource::try_get("lazy-dotdot.nori::child/./nested/../..")};
    ASSERT_TRUE(fresh_parent);
    const auto loaded_root{nori::resource::try_get("lazy-dotdot.nori")};
    ASSERT_TRUE(loaded_root);
    EXPECT_EQ(*fresh_parent, *loaded_root);

    const auto loaded_sibling{nori::resource::try_get(*loaded_root, "sibling")};
    ASSERT_TRUE(loaded_sibling);
    EXPECT_EQ(nori::resource::try_get(*loaded_root, "child/nested/../../sibling"), loaded_sibling);

    const auto loaded_child{nori::resource::try_get(*loaded_root, "child/nested/..")};
    ASSERT_TRUE(loaded_child);
    EXPECT_EQ(nori::resource::try_get(*loaded_root, "child"), loaded_child);
    EXPECT_EQ(nori::resource::try_get(*loaded_root, "child/../child"), loaded_child);

    const auto partial_missing{nori::resource::try_get(*loaded_child, "missing/..")};
    ASSERT_FALSE(partial_missing);
    EXPECT_EQ(partial_missing.error(), nori::resource::error_code::not_found);

    EXPECT_EQ(nori::resource::try_get(*loaded_child, "../sibling"), loaded_sibling);
    EXPECT_EQ(nori::resource::get<std::string>(*loaded_sibling), "sibling");

    ASSERT_TRUE(nori::resource::load(*loaded_root));
    const auto materialized_missing{nori::resource::try_get(*loaded_child, "missing/..")};
    ASSERT_FALSE(materialized_missing);
    EXPECT_EQ(materialized_missing.error(), nori::resource::error_code::not_found);
    EXPECT_EQ(nori::resource::try_get(*loaded_root, "child/nested/../../sibling"), loaded_sibling);
}

TEST_F(loading_test, failed_parent_navigation_leaves_document_unbound)
{
    const nori::resource::handle root{create_resource("failed-navigation.nori")};
    create_resource("failed-navigation.nori::child");
    ASSERT_TRUE(nori::resource::save(root));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto missing{nori::resource::try_get("failed-navigation.nori::child/missing/../..")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(std::filesystem::remove(directory_ / "failed-navigation.nori"));
    const auto reopened{nori::resource::try_get("failed-navigation.nori")};
    ASSERT_FALSE(reopened);
    EXPECT_EQ(reopened.error(), nori::resource::error_code::not_found);
}

TEST_F(loading_test, failed_value_read_can_be_retried_after_restoring_the_file)
{
    const auto file{directory_ / "retry.nori"};
    const std::array properties{fixture_property{0, "text", std::string{"payload"}}};
    ASSERT_TRUE(save_fixture(file, properties));
    const auto original_bytes{file_bytes(file)};
    const auto target{nori::resource::try_get("retry.nori::text")};
    ASSERT_TRUE(target);
    const auto hidden{directory_ / "hidden.nori"};
    std::filesystem::rename(file, hidden);
    const auto missing{nori::resource::try_get<std::string>(*target)};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::io_failure);

    std::filesystem::rename(hidden, file);
    const auto size{std::filesystem::file_size(file)};
    std::filesystem::resize_file(file, size - 1);
    for (int attempt{0}; attempt < 2; ++attempt)
    {
        const auto changed{nori::resource::try_get<std::string>(*target)};
        ASSERT_FALSE(changed);
        EXPECT_EQ(changed.error(), nori::resource::error_code::io_failure);
    }
    {
        std::ofstream bytes{file, std::ios::binary | std::ios::trunc};
        bytes.write(original_bytes.data(), static_cast<std::streamsize>(original_bytes.size()));
    }
    EXPECT_EQ(nori::resource::try_get<std::string>(*target), "payload");
}

TEST_F(loading_test, failed_metadata_read_can_be_retried_after_restoring_the_file)
{
    const auto file{directory_ / "metadata-retry.nori"};
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(root, "branch", nori::resource::empty)};
    const nori::resource::handle leaf{nori::resource::create(branch, "leaf", std::int32_t{42})};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(leaf);
    ASSERT_TRUE(nori::resource::save_as(root, file));
    const auto original_bytes{file_bytes(file)};
    ASSERT_TRUE(nori::resource::unload(branch));

    const auto hidden{directory_ / "metadata-retry-hidden.nori"};
    std::filesystem::rename(file, hidden);
    const auto missing{nori::resource::try_get(branch, "leaf")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);

    std::filesystem::rename(hidden, file);
    const auto retried{nori::resource::try_get(branch, "leaf")};
    ASSERT_TRUE(retried);
    EXPECT_EQ(nori::resource::name_of(*retried), "leaf");
    EXPECT_EQ(nori::resource::get<std::int32_t>(*retried), 42);
    EXPECT_EQ(file_bytes(file), original_bytes);
}

TEST_F(loading_test, save_as_switches_lazy_reads_to_the_new_file_path)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(root, "value", std::int32_t{42})};
    ASSERT_TRUE(value);

    const auto original_file{directory_ / "path-switch-original.nori"};
    const auto destination_file{directory_ / "path-switch-destination.nori"};
    ASSERT_TRUE(nori::resource::save_as(root, original_file));
    ASSERT_TRUE(nori::resource::unload(value));
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);

    ASSERT_TRUE(nori::resource::save_as(root, destination_file));
    const std::array replacement{fixture_property{0, "value", std::int32_t{99}}};
    const auto replacement_file{directory_ / "path-switch-replacement.nori"};
    ASSERT_TRUE(save_fixture(replacement_file, replacement));
    std::filesystem::copy_file(replacement_file, destination_file, std::filesystem::copy_options::overwrite_existing);

    ASSERT_TRUE(nori::resource::unload(value));
    const auto reloaded{nori::resource::try_get<std::int32_t>(value)};
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(*reloaded, 99);
}

TEST_F(loading_test, position_propagates_child_list_read_errors_and_retries)
{
    const auto file{directory_ / "position-retry.nori"};
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(root, "branch", std::int32_t{5})};
    const nori::resource::handle leaf{nori::resource::create(branch, "leaf", std::int32_t{42})};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(leaf);
    ASSERT_TRUE(nori::resource::save_as(root, file));
    ASSERT_TRUE(nori::resource::unload(branch));

    const auto loaded_leaf{nori::resource::try_get(branch, "leaf")};
    ASSERT_TRUE(loaded_leaf);
    EXPECT_EQ(nori::resource::get<std::int32_t>(branch), 5);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded_leaf), 42);
    ASSERT_TRUE(nori::resource::save(root));

    const auto hidden{directory_ / "position-retry-hidden.nori"};
    std::filesystem::rename(file, hidden);
    const auto failed_children{nori::resource::children_of(branch)};
    ASSERT_FALSE(failed_children);
    EXPECT_EQ(failed_children.error(), nori::resource::error_code::not_found);
    const auto failed{nori::resource::position_of(*loaded_leaf)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::not_found);

    std::filesystem::rename(hidden, file);
    const auto retried_children{nori::resource::children_of(branch)};
    ASSERT_TRUE(retried_children);
    ASSERT_EQ(retried_children->size(), 1u);
    EXPECT_EQ(retried_children->front(), *loaded_leaf);
    const auto retried{nori::resource::position_of(*loaded_leaf)};
    ASSERT_TRUE(retried);
    EXPECT_EQ(*retried, 0u);
}

TEST_F(loading_test, cross_hierarchy_move_loads_only_the_requested_source_subtree)
{
    const auto file{directory_ / "copy-subtree.nori"};
    const std::array properties{
        fixture_property{0, "branch", std::int32_t{7}},
        fixture_property{1, "leaf", std::string{"available"}},
        fixture_property{0, "broken", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto branch{nori::resource::try_get("copy-subtree.nori::branch")};
    ASSERT_TRUE(branch);
    ASSERT_EQ(nori::resource::get<std::int32_t>(*branch), 7);

    std::filesystem::resize_file(file, std::filesystem::file_size(file) - 1);
    const nori::resource::handle destination{nori::resource::create_root()};
    const auto source_root{nori::resource::root_of(*branch)};
    const auto copied{nori::resource::move(*branch, destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::get<std::string>(*copied, "leaf"), "available");
    const auto broken{nori::resource::try_get(source_root, "broken")};
    ASSERT_TRUE(broken);
    EXPECT_EQ(nori::resource::try_get<nori::resource::image>(*broken).error(), nori::resource::error_code::io_failure);
}

TEST_F(loading_test, cross_hierarchy_move_failure_preserves_source_and_can_be_retried)
{
    const auto file{directory_ / "copy-retry.nori"};
    const std::array properties{
        fixture_property{0, "branch", std::int32_t{7}},
        fixture_property{1, "leaf", std::string{"available"}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto original_bytes{file_bytes(file)};
    const auto branch{nori::resource::try_get("copy-retry.nori::branch")};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(nori::resource::unload(*branch));

    const auto hidden{directory_ / "copy-retry-hidden.nori"};
    std::filesystem::rename(file, hidden);
    const nori::resource::handle destination{nori::resource::create_root()};
    const auto failed{nori::resource::move(*branch, destination)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_TRUE(children_of_or_fail(destination).empty());
    EXPECT_EQ(nori::resource::parent_of(*branch), nori::resource::root_of(*branch));
    EXPECT_EQ(nori::resource::name_of(*branch), "branch");

    std::filesystem::rename(hidden, file);
    const auto source_root{nori::resource::root_of(*branch)};
    const auto copied{nori::resource::move(*branch, destination)};
    ASSERT_TRUE(copied);
    EXPECT_TRUE(children_of_or_fail(source_root).empty());
    EXPECT_EQ(nori::resource::get<std::string>(*copied, "leaf"), "available");
    EXPECT_EQ(file_bytes(file), original_bytes);
}

TEST_F(loading_test, cross_hierarchy_move_is_not_recreated_by_later_source_loading)
{
    const auto file{directory_ / "move-partial.nori"};
    const std::array properties{
        fixture_property{0, "parent", nori::resource::empty_t{}},
        fixture_property{1, "branch", std::int32_t{7}},
        fixture_property{2, "leaf", std::string{"preserved"}},
        fixture_property{1, "sibling", std::int32_t{42}},
        fixture_property{0, "other", std::string{"unloaded"}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto branch{nori::resource::get("move-partial.nori::parent/branch")};
    const auto source_root{nori::resource::root_of(branch)};
    const auto destination{nori::resource::create_root()};
    const auto moved{nori::resource::move(branch, destination)};
    ASSERT_TRUE(moved);
    EXPECT_FALSE(nori::resource::try_get(source_root, "parent/branch"));
    ASSERT_TRUE(nori::resource::load(source_root));
    EXPECT_FALSE(nori::resource::try_get(source_root, "parent/branch"));
    EXPECT_EQ(nori::resource::get<std::int32_t>(source_root, "parent/sibling"), 42);
    EXPECT_EQ(nori::resource::get<std::string>(source_root, "other"), "unloaded");
    ASSERT_TRUE(nori::resource::save(source_root));
    ASSERT_TRUE(nori::resource::unload(source_root));
    EXPECT_FALSE(nori::resource::try_get("move-partial.nori::parent/branch"));
    EXPECT_EQ(nori::resource::get<std::string>(*moved, "leaf"), "preserved");
}

TEST_F(loading_test, copy_completes_destination_children_before_merging_and_appending)
{
    const auto source_root{nori::resource::create_root()};
    const auto source{nori::resource::create(source_root, "existing", "new")};
    const auto file{directory_ / "copy-destination.nori"};
    const std::array properties{
        fixture_property{0, "existing", std::string{"old"}},
        fixture_property{0, "last", std::int32_t{7}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto destination{nori::resource::get("copy-destination.nori")};

    const auto conflict{nori::resource::copy(source, destination)};
    ASSERT_TRUE(conflict);
    EXPECT_EQ(nori::resource::get<std::string>(destination, "existing"), "new");
    ASSERT_TRUE(nori::resource::rename(source, "appended"));
    const auto copied{nori::resource::copy(source, destination)};
    ASSERT_TRUE(copied);
    const auto children{children_of_or_fail(destination)};
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(nori::resource::name_of(children[0]), "existing");
    EXPECT_EQ(nori::resource::name_of(children[1]), "last");
    EXPECT_EQ(children[2], *copied);
    EXPECT_EQ(nori::resource::get<std::string>(*copied), "new");
}

TEST_F(loading_test, copy_failure_leaves_source_and_destination_unchanged_and_can_be_retried)
{
    const auto file{directory_ / "copy-retry.nori"};
    const std::array properties{
        fixture_property{0, "branch", std::int32_t{7}},
        fixture_property{1, "leaf", std::string{"available"}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto branch{nori::resource::try_get("copy-retry.nori::branch")};
    ASSERT_TRUE(branch);
    ASSERT_TRUE(nori::resource::unload(*branch));
    const nori::resource::handle root{nori::resource::root_of(*branch)};
    const std::vector<nori::resource::handle> original_children{children_of_or_fail(root)};
    ASSERT_EQ(original_children.size(), 1);
    EXPECT_EQ(original_children.front(), *branch);

    const auto destination{nori::resource::create_root()};
    const auto hidden{directory_ / "copy-retry-hidden.nori"};
    std::filesystem::rename(file, hidden);
    const auto failed{nori::resource::copy(*branch, destination)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_TRUE(children_of_or_fail(destination).empty());
    EXPECT_EQ(children_of_or_fail(root), original_children);
    EXPECT_EQ(nori::resource::parent_of(*branch), root);

    std::filesystem::rename(hidden, file);
    const auto copied{nori::resource::copy(*branch, destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::parent_of(*copied), destination);
    EXPECT_EQ(nori::resource::root_of(*copied), destination);
    EXPECT_EQ(nori::resource::get<std::string>(*copied, "leaf"), "available");
    EXPECT_EQ(children_of_or_fail(root), original_children);
}

TEST_F(loading_test, merge_failure_preserves_values_views_and_links_until_retry)
{
    for (const bool consume_source : {false, true})
    {
        const std::string name{consume_source ? "merge-move.nori" : "merge-copy.nori"};
        const auto file{directory_ / name};
        const std::array properties{
            fixture_property{0, "branch", std::int32_t{7}},
            fixture_property{1, "incoming", std::string{"new"}},
        };
        ASSERT_TRUE(save_fixture(file, properties));
        const auto source{nori::resource::get(name + "::branch")};
        const auto source_root{nori::resource::root_of(source)};
        ASSERT_EQ(children_of_or_fail(source_root).size(), 1u);
        const auto root{nori::resource::create_root()};
        const auto target{nori::resource::create(root, "branch", nori::resource::binary{{std::byte{42}}})};
        const auto kept{nori::resource::create(target, "kept", true)};
        const auto view{nori::resource::get<nori::resource::binary_view>(target)};
        const auto hidden{directory_ / (name + ".hidden")};
        std::filesystem::rename(file, hidden);

        const auto failed{consume_source ? nori::resource::move(source, root) : nori::resource::copy(source, root)};
        ASSERT_FALSE(failed);
        EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
        EXPECT_EQ(nori::resource::parent_of(source), source_root);
        EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(target).data.data(), view.data.data());
        EXPECT_EQ(view.data.front(), std::byte{42});
        EXPECT_EQ(children_of_or_fail(root), std::vector{target});
        EXPECT_EQ(children_of_or_fail(target), std::vector{kept});

        std::filesystem::rename(hidden, file);
        const auto retried{consume_source ? nori::resource::move(source, root) : nori::resource::copy(source, root)};
        ASSERT_TRUE(retried);
        EXPECT_EQ(*retried, target);
        EXPECT_EQ(nori::resource::get<std::int32_t>(target), 7);
        EXPECT_TRUE(nori::resource::get<bool>(target, "kept"));
        EXPECT_EQ(nori::resource::get<std::string>(target, "incoming"), "new");
        if (consume_source)
        {
            EXPECT_TRUE(children_of_or_fail(source_root).empty());
        }
        else
        {
            EXPECT_EQ(nori::resource::parent_of(source), source_root);
        }
    }
}

TEST_F(loading_test, same_hierarchy_move_loads_source_before_changing_its_file_path)
{
    const auto file{directory_ / "move-lazy.nori"};
    const std::array properties{
        fixture_property{0, "source", std::int32_t{7}},
        fixture_property{1, "leaf", std::string{"preserved"}},
        fixture_property{0, "destination", nori::resource::empty_t{}},
    };
    ASSERT_TRUE(save_fixture(file, properties));
    const auto source{nori::resource::get("move-lazy.nori::source")};
    const auto root{nori::resource::root_of(source)};
    const auto destination{nori::resource::get(root, "destination")};
    const auto moved{nori::resource::move(source, destination)};
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, source);
    EXPECT_EQ(nori::resource::get<std::string>(source, "leaf"), "preserved");
    ASSERT_TRUE(nori::resource::load(root));
    EXPECT_FALSE(nori::resource::try_get(root, "source"));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));
    EXPECT_FALSE(nori::resource::try_get("move-lazy.nori::source"));
    EXPECT_EQ(nori::resource::get<std::string>("move-lazy.nori::destination/source/leaf"), "preserved");
}

TEST_F(loading_test, value_loading_allows_file_size_changes_when_the_payload_is_readable)
{
    const auto file{directory_ / "extended.nori"};
    const std::array properties{fixture_property{0, "text", std::string{"payload"}}};
    ASSERT_TRUE(save_fixture(file, properties));
    const auto target{nori::resource::try_get("extended.nori::text")};
    ASSERT_TRUE(target);

    std::filesystem::resize_file(file, std::filesystem::file_size(file) + 1);
    EXPECT_EQ(nori::resource::try_get<std::string>(*target), "payload");
}

TEST_F(loading_test, full_load_failure_preserves_memory_state)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle broken{nori::resource::create(document, "broken", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)})};
    const nori::resource::handle first{nori::resource::create(document, "first", "cached")};
    const nori::resource::handle second{nori::resource::create(document, "second", "staged")};
    ASSERT_TRUE(broken);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(nori::resource::save_as(document, "failed-load.nori"));
    ASSERT_TRUE(nori::resource::unload(document));
    const auto loaded{nori::resource::get("failed-load.nori")};
    const auto loaded_first{nori::resource::get(loaded, "first")};
    EXPECT_EQ(nori::resource::get<std::string>(loaded_first), "cached");
    std::filesystem::resize_file(directory_ / "failed-load.nori", std::filesystem::file_size(directory_ / "failed-load.nori") - 1);
    const auto failed{nori::resource::load(loaded)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::get(loaded, "first"), loaded_first);
    ASSERT_TRUE(std::filesystem::remove(directory_ / "failed-load.nori"));
    EXPECT_EQ(nori::resource::get<std::string>(loaded_first), "cached");
    EXPECT_EQ(nori::resource::try_get<std::string>(loaded, "second").error(), nori::resource::error_code::io_failure);
}

TEST_F(loading_test, failed_load_can_be_repaired_and_retried)
{
    const auto file{directory_ / "partial.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", std::int32_t{10}});
        encoded_properties.push_back({1, "first", std::int32_t{42}});
        encoded_properties.push_back({1, "second", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto original_bytes{file_bytes(file)};
    const auto branch{nori::resource::try_get("partial.nori::branch")};
    ASSERT_TRUE(branch);
    const auto first{nori::resource::get(*branch, "first")};
    const auto second{nori::resource::get(*branch, "second")};
    EXPECT_EQ(nori::resource::get<std::int32_t>(first), 42);
    const auto root{nori::resource::parent_of(*branch)};
    std::filesystem::resize_file(file, original_bytes.size() - 1);
    const auto failed{nori::resource::load(root)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::get(*branch, "first"), first);
    EXPECT_EQ(nori::resource::get(*branch, "second"), second);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*branch), 10);
    EXPECT_EQ(nori::resource::get<std::int32_t>(first), 42);
    {
        std::ofstream bytes{file, std::ios::binary | std::ios::trunc};
        bytes.write(original_bytes.data(), static_cast<std::streamsize>(original_bytes.size()));
    }
    ASSERT_TRUE(nori::resource::load(root));
    nori::resource::set(first, std::int32_t{43});
    const auto copy{nori::resource::copy(*branch, nori::resource::create_root())};
    ASSERT_TRUE(copy);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copy, "first"), 43);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(*copy, "second").width, 1u);
    nori::resource::shutdown();
}

TEST_F(loading_test, failed_load_preserves_cached_values_before_retry)
{
    const auto file{directory_ / "staged.nori"};
    {
        std::vector<fixture_property> encoded_properties;
        encoded_properties.push_back({0, "branch", std::string{"branch payload"}});
        encoded_properties.push_back({1, "pending", std::string{"child payload"}});
        encoded_properties.push_back({1, "cached", nori::resource::binary{.data = {std::byte{42}}}});
        encoded_properties.push_back({1, "empty", nori::resource::empty_t{}});
        encoded_properties.push_back({1, "icon", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4, std::byte{7})}});
        encoded_properties.push_back({1, "broken", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}});
        ASSERT_TRUE(save_fixture(file, encoded_properties));
    }
    const auto original_bytes{file_bytes(file)};
    const auto cached{nori::resource::get("staged.nori::branch/cached")};
    const auto before{nori::resource::get<nori::resource::binary_view>(cached)};
    const auto branch{nori::resource::parent_of(cached)};
    const auto root{nori::resource::root_of(branch)};
    const auto icon{nori::resource::get(branch, "icon")};
    const auto icon_before{nori::resource::get<nori::resource::image_view>(icon)};

    std::filesystem::resize_file(file, original_bytes.size() - 1);
    const auto failed{nori::resource::load(root)};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    const auto hidden{directory_ / "hidden.nori"};
    std::filesystem::rename(file, hidden);
    EXPECT_EQ(nori::resource::try_get<std::string>(branch).error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::try_get<std::string>(branch, "pending").error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::get(branch, "cached"), cached);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(cached).data.data(), before.data.data());
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(icon).data.data(), icon_before.data.data());

    std::filesystem::rename(hidden, file);
    {
        std::ofstream bytes{file, std::ios::binary | std::ios::trunc};
        bytes.write(original_bytes.data(), static_cast<std::streamsize>(original_bytes.size()));
    }
    ASSERT_TRUE(nori::resource::load(root));
    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(branch, "broken").width, 1u);
    EXPECT_EQ(nori::resource::get<std::string>(branch), "branch payload");
    EXPECT_EQ(nori::resource::get<std::string>(branch, "pending"), "child payload");
    EXPECT_EQ(nori::resource::type_of(nori::resource::get(branch, "empty")), nori::resource::property_type::empty);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(cached).data.data(), before.data.data());
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(icon).data.data(), icon_before.data.data());
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(icon).data.front(), std::byte{7});
}
} // namespace
