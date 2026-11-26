#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
TEST_F(structure_test, create_connects_nodes_to_the_requested_parent)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(root, "parent", nori::resource::empty)};
    ASSERT_TRUE(parent);
    const nori::resource::handle first{nori::resource::create(parent, "same", std::int32_t{42})};
    const nori::resource::handle conflicting{nori::resource::create(parent, "same", "other")};
    const nori::resource::handle other_parent{nori::resource::create(root, "other", nori::resource::empty)};
    ASSERT_TRUE(other_parent);
    const nori::resource::handle same_name{nori::resource::create(other_parent, "same", "other")};
    ASSERT_TRUE(first);
    ASSERT_TRUE(same_name);
    EXPECT_FALSE(conflicting);
    EXPECT_EQ(nori::resource::root_of(first), root);
    EXPECT_EQ(nori::resource::parent_of(first), parent);
    EXPECT_EQ(nori::resource::path_of(first), nori::resource::path{"parent/same"});
    EXPECT_EQ(children_of_or_fail(parent), std::vector{first});
    EXPECT_EQ(nori::resource::parent_of(same_name), other_parent);
    EXPECT_EQ(children_of_or_fail(other_parent), std::vector{same_name});
    EXPECT_EQ(nori::resource::get<std::int32_t>(first), 42);
}

TEST_F(structure_test, copy_recursively_merges_values_children_and_preserves_destination_order)
{
    const auto source_root{nori::resource::create_root()};
    const auto destination_root{nori::resource::create_root()};
    const auto source{nori::resource::create(source_root, "branch", std::int32_t{7})};
    const auto source_shared{nori::resource::create(source, "shared", nori::resource::empty)};
    nori::resource::create(source_shared, "new_leaf", "new");
    nori::resource::create(source, "appended", true);
    const auto destination{nori::resource::create(destination_root, "branch", "old")};
    const auto kept{nori::resource::create(destination, "kept", std::int32_t{42})};
    const auto shared{nori::resource::create(destination, "shared", "old")};
    const auto old_leaf{nori::resource::create(shared, "old_leaf", "preserved")};

    const auto copied{nori::resource::copy(source, destination_root)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(*copied, destination);
    EXPECT_EQ(nori::resource::get<std::int32_t>(destination), 7);
    EXPECT_EQ(nori::resource::type_of(shared), nori::resource::property_type::empty);
    const auto children{children_of_or_fail(destination)};
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], kept);
    EXPECT_EQ(children[1], shared);
    EXPECT_EQ(nori::resource::name_of(children[2]), "appended");
    const auto leaves{children_of_or_fail(shared)};
    ASSERT_EQ(leaves.size(), 2u);
    EXPECT_EQ(leaves[0], old_leaf);
    EXPECT_EQ(nori::resource::get<std::string>(leaves[1]), "new");
    EXPECT_EQ(nori::resource::parent_of(source), source_root);
}

TEST_F(structure_test, copy_snapshots_source_when_destination_overlaps_it)
{
    const auto root{nori::resource::create_root()};
    const auto outer{nori::resource::create(root, "node", std::int32_t{1})};
    const auto source{nori::resource::create(outer, "node", std::int32_t{2})};
    const auto child{nori::resource::create(source, "node", std::int32_t{3})};
    nori::resource::create(child, "leaf", std::int32_t{4});

    const auto copied{nori::resource::copy(source, root)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(*copied, outer);
    EXPECT_EQ(nori::resource::get<std::int32_t>(outer), 2);
    EXPECT_EQ(nori::resource::get<std::int32_t>(source), 3);
    EXPECT_EQ(nori::resource::get<std::int32_t>(source, "leaf"), 4);
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 3);
}

TEST_F(structure_test, move_merges_collisions_and_preserves_nonconflicting_handles_in_the_same_hierarchy)
{
    const auto root{nori::resource::create_root()};
    const auto source{nori::resource::create(root, "branch", std::int32_t{7})};
    const auto incoming{nori::resource::create(source, "shared", "new")};
    const auto moved{nori::resource::create(source, "moved", nori::resource::binary{{std::byte{42}}})};
    const auto leaf{nori::resource::create(moved, "leaf", true)};
    const auto view{nori::resource::get<nori::resource::binary_view>(moved)};
    const auto destination{nori::resource::create(root, "destination", nori::resource::empty)};
    const auto existing{nori::resource::create(destination, "branch", "old")};
    const auto kept{nori::resource::create(existing, "kept", std::int32_t{42})};
    const auto shared{nori::resource::create(existing, "shared", "old")};

    const auto result{nori::resource::move(source, destination)};
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, existing);
    EXPECT_FALSE(nori::resource::try_get(root, "branch"));
    EXPECT_EQ(nori::resource::get<std::int32_t>(existing), 7);
    EXPECT_EQ(nori::resource::get<std::string>(shared), "new");
    EXPECT_EQ(children_of_or_fail(existing), (std::vector{kept, shared, moved}));
    EXPECT_EQ(nori::resource::get(moved, "leaf"), leaf);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(moved).data.data(), view.data.data());
    EXPECT_EQ(nori::resource::get(existing, "shared"), shared);
    EXPECT_NE(shared, incoming);
}

TEST_F(structure_test, copy_and_move_to_current_parent_preserve_handles_values_and_order)
{
    const auto root{nori::resource::create_root()};
    const auto first{nori::resource::create(root, "first", nori::resource::empty)};
    const auto source{nori::resource::create(root, "source", nori::resource::binary{{std::byte{7}}})};
    const auto last{nori::resource::create(root, "last", true)};
    const auto view{nori::resource::get<nori::resource::binary_view>(source)};
    EXPECT_EQ(nori::resource::copy(source, root), source);
    EXPECT_EQ(nori::resource::move(source, root), source);
    EXPECT_EQ(children_of_or_fail(root), (std::vector{first, source, last}));
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(source).data.data(), view.data.data());
}

TEST_F(structure_test, move_rejects_cycles_and_merging_into_an_ancestor_without_changes)
{
    const auto root{nori::resource::create_root()};
    const auto outer{nori::resource::create(root, "same", std::int32_t{1})};
    const auto inner{nori::resource::create(outer, "same", std::int32_t{2})};
    const auto leaf{nori::resource::create(inner, "leaf", true)};
    EXPECT_EQ(nori::resource::move(inner, inner).error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::move(inner, leaf).error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::move(inner, root).error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(outer), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>(inner), 2);
    EXPECT_EQ(nori::resource::parent_of(inner), outer);
    EXPECT_EQ(nori::resource::parent_of(leaf), inner);
    EXPECT_EQ(nori::resource::move(leaf, root), leaf);
}

TEST_F(structure_test, copy_into_a_descendant_is_rejected_without_changes)
{
    const auto root{nori::resource::create_root()};
    const auto source{nori::resource::create(root, "branch", std::int32_t{7})};
    const auto destination{nori::resource::create(source, "destination", true)};
    const auto collision{nori::resource::create(destination, "branch", "old")};
    const auto kept{nori::resource::create(collision, "kept", std::int32_t{42})};

    const auto copied{nori::resource::copy(source, destination)};
    ASSERT_FALSE(copied);
    EXPECT_EQ(copied.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(source), 7);
    EXPECT_EQ(nori::resource::get<std::string>(collision), "old");
    EXPECT_EQ(children_of_or_fail(source), std::vector{destination});
    EXPECT_EQ(children_of_or_fail(destination), std::vector{collision});
    EXPECT_EQ(children_of_or_fail(collision), std::vector{kept});
}

TEST_F(structure_test, create_supports_every_value_type)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const std::array values{
        nori::resource::create(root, "empty", nori::resource::empty),
        nori::resource::create(root, "boolean", true),
        nori::resource::create(root, "int32", std::int32_t{42}),
        nori::resource::create(root, "int64", std::int64_t{43}),
        nori::resource::create(root, "float", 1.5f),
        nori::resource::create(root, "string", "text"),
        nori::resource::create(root, "view", std::string_view{"view"}),
        nori::resource::create(root, "image", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4)}),
        nori::resource::create(root, "binary", nori::resource::binary{{std::byte{1}}}),
    };
    for (const auto value : values)
    {
        ASSERT_TRUE(value);
        EXPECT_EQ(nori::resource::root_of(value), root);
        EXPECT_EQ(nori::resource::parent_of(value), root);
    }
    EXPECT_EQ(children_of_or_fail(root).size(), values.size());
}

TEST_F(structure_test, cross_hierarchy_move_transfers_a_complete_subtree)
{
    const nori::resource::handle source_root{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(source_root, "branch", std::int32_t{7})};
    const nori::resource::handle first{nori::resource::create(source, "first", "preserved")};
    const nori::resource::handle last{nori::resource::create(source, "last", true)};
    const nori::resource::handle leaf{nori::resource::create(first, "leaf", nori::resource::binary{{std::byte{42}}})};

    const auto moved{nori::resource::move(source, destination)};
    ASSERT_TRUE(moved);
    EXPECT_NE(*moved, source);
    EXPECT_TRUE(children_of_or_fail(source_root).empty());
    EXPECT_EQ(nori::resource::parent_of(*moved), destination);
    EXPECT_EQ(nori::resource::root_of(*moved), destination);
    const auto children{children_of_or_fail(*moved)};
    ASSERT_EQ(children.size(), 2u);
    EXPECT_NE(children[0], first);
    EXPECT_NE(children[1], last);
    EXPECT_EQ(nori::resource::name_of(children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(children[1]), "last");
    const nori::resource::handle moved_leaf{nori::resource::get(children[0], "leaf")};
    EXPECT_NE(moved_leaf, leaf);
    nori::resource::remove(source_root);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*moved), 7);
    EXPECT_EQ(nori::resource::get<std::string>(children[0]), "preserved");
    EXPECT_TRUE(nori::resource::get<bool>(children[1]));
    EXPECT_EQ(nori::resource::get<nori::resource::binary>(moved_leaf).data, std::vector{std::byte{42}});
    ASSERT_TRUE(nori::resource::save_as(destination, "moved.nori"));
    ASSERT_TRUE(nori::resource::unload(destination));
    EXPECT_EQ(nori::resource::get<std::string>("moved.nori::branch/first"), "preserved");
}

TEST_F(structure_test, cross_hierarchy_move_merges_into_existing_destination)
{
    const auto source_root{nori::resource::create_root()};
    const auto destination{nori::resource::create_root()};
    const auto source{nori::resource::create(source_root, "branch", "source")};
    const auto leaf{nori::resource::create(source, "leaf", std::int32_t{42})};
    const auto existing{nori::resource::create(destination, "branch", "existing")};
    const auto moved{nori::resource::move(source, destination)};
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, existing);
    EXPECT_TRUE(children_of_or_fail(source_root).empty());
    EXPECT_EQ(nori::resource::get<std::int32_t>(existing, "leaf"), 42);
    EXPECT_NE(nori::resource::get(existing, "leaf"), leaf);
    EXPECT_EQ(nori::resource::get<std::string>(existing), "source");
}

TEST_F(structure_test, cross_hierarchy_copy_preserves_source_root)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(source, "branch", std::int32_t{7})};
    ASSERT_TRUE(branch);
    const nori::resource::handle descendant{nori::resource::create(branch, "descendant", "value")};

    const auto copied{nori::resource::copy(descendant, destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::get<std::string>(*copied), "value");
    EXPECT_EQ(nori::resource::root_of(branch), source);
    EXPECT_EQ(nori::resource::root_of(descendant), source);
    nori::resource::remove(source);
}

TEST_F(structure_test, copy_preserves_source_and_connects_an_independent_complete_subtree)
{
    const nori::resource::handle source_root{nori::resource::create_root()};
    const nori::resource::handle destination_root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(source_root, "branch", std::int32_t{7})};
    ASSERT_TRUE(source);
    const nori::resource::handle first{nori::resource::create(source, "first", "original")};
    ASSERT_TRUE(first);
    const nori::resource::handle last{nori::resource::create(source, "last", true)};
    ASSERT_TRUE(last);
    const nori::resource::handle grandchild{nori::resource::create(first, "grandchild", std::int64_t{9})};
    EXPECT_EQ(nori::resource::path_of(first), nori::resource::path{"branch/first"});
    const auto copied{nori::resource::copy(source, destination_root)};
    ASSERT_TRUE(copied);
    EXPECT_NE(*copied, source);
    EXPECT_EQ(nori::resource::parent_of(*copied), destination_root);
    EXPECT_EQ(nori::resource::root_of(*copied), destination_root);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copied), 7);
    const auto children{children_of_or_fail(*copied)};
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(children[1]), "last");
    EXPECT_NE(children[0], first);
    const auto copied_grandchild{nori::resource::get(children[0], "grandchild")};
    EXPECT_NE(copied_grandchild, grandchild);
    EXPECT_EQ(nori::resource::get<std::int64_t>(copied_grandchild), 9);
    nori::resource::set(children[0], "changed");
    EXPECT_EQ(nori::resource::get<std::string>(first), "original");
    EXPECT_EQ(nori::resource::copy(source, destination_root), copied);
    EXPECT_EQ(children_of_or_fail(destination_root), std::vector{*copied});
    EXPECT_EQ(nori::resource::parent_of(source), source_root);
    const auto nested{nori::resource::copy(source, first)};
    ASSERT_FALSE(nested);
    EXPECT_EQ(nested.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(children_of_or_fail(first), std::vector{grandchild});
}

TEST_F(structure_test, copy_to_self_is_rejected_without_changes)
{
    const auto root{nori::resource::create_root()};
    const auto source{nori::resource::create(root, "source", std::int32_t{7})};
    const auto first{nori::resource::create(source, "first", "preserved")};
    const auto last{nori::resource::create(source, "last", true)};

    const auto copied{nori::resource::copy(source, source)};
    ASSERT_FALSE(copied);
    EXPECT_EQ(copied.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(children_of_or_fail(source), (std::vector{first, last}));
    EXPECT_EQ(nori::resource::get<std::int32_t>(source), 7);
    EXPECT_EQ(nori::resource::get<std::string>(first), "preserved");
    EXPECT_TRUE(nori::resource::get<bool>(last));
}

TEST_F(structure_test, document_copy_uses_a_new_root_and_copies_each_top_level_subtree)
{
    const nori::resource::handle source_root{nori::resource::create_root()};
    const nori::resource::handle destination_root{nori::resource::create_root()};
    const nori::resource::handle first{nori::resource::create(source_root, "first", "source")};
    const nori::resource::handle child{nori::resource::create(first, "child", std::int32_t{42})};
    const nori::resource::handle second{nori::resource::create(source_root, "second", nori::resource::empty)};
    ASSERT_TRUE(second);

    for (const nori::resource::handle top_level : children_of_or_fail(source_root))
    {
        ASSERT_TRUE(nori::resource::copy(top_level, destination_root));
    }

    const nori::resource::handle copied_first{nori::resource::get(destination_root, "first")};
    const nori::resource::handle copied_child{nori::resource::get(copied_first, "child")};
    EXPECT_EQ(nori::resource::get<std::string>(copied_first), "source");
    EXPECT_EQ(nori::resource::get<std::int32_t>(copied_child), 42);
    EXPECT_EQ(nori::resource::get<std::string>(first), "source");
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 42);
    EXPECT_NE(copied_first, first);
    EXPECT_NE(copied_child, child);
    EXPECT_EQ(children_of_or_fail(destination_root).size(), 2u);
}

TEST_F(structure_test, saved_document_reopens_with_memory_changes)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(source, "value", std::int32_t{42})};
    ASSERT_TRUE(nori::resource::save_as(source, "source.nori"));
    ASSERT_TRUE(nori::resource::save_as(destination, "destination.nori"));
    const auto copied{nori::resource::copy(value, destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copied), 42);
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);
    ASSERT_TRUE(nori::resource::save(destination));
    ASSERT_TRUE(nori::resource::unload(source));
    const auto reloaded{nori::resource::get("source.nori::value")};
    EXPECT_NE(reloaded, value);
    EXPECT_EQ(nori::resource::get<std::int32_t>(reloaded), 42);
}

TEST_F(structure_test, create_independent_root)
{
    const nori::resource::handle first{nori::resource::create_root()};
    const nori::resource::handle second{nori::resource::create_root()};
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first, second);
    EXPECT_EQ(nori::resource::root_of(first), first);
    EXPECT_EQ(nori::resource::root_of(second), second);
    EXPECT_FALSE(nori::resource::parent_of(first));
    EXPECT_FALSE(nori::resource::parent_of(second));
    EXPECT_TRUE(nori::resource::path_of(first).empty());
    EXPECT_TRUE(children_of_or_fail(first).empty());
    EXPECT_TRUE(std::filesystem::is_empty(directory_));
}

TEST_F(structure_test, opened_root_reloads_children_without_root_metadata)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(source, "child", std::int32_t{42})};
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(source, "root.nori"));
    nori::resource::remove(source);

    const auto root{nori::resource::get("root.nori")};
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(directory_ / "root.nori"));
    ASSERT_TRUE(nori::resource::load(root));
    const auto loaded_child{nori::resource::get(root, "child")};
    EXPECT_EQ(nori::resource::path_of(loaded_child), bound_resource_path(directory_ / "root.nori", "child"));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::unload(root));

    const auto reopened{nori::resource::get("root.nori")};
    EXPECT_NE(reopened, root);
    EXPECT_EQ(nori::resource::path_of(reopened), bound_resource_path(directory_ / "root.nori"));
    EXPECT_EQ(nori::resource::get<std::int32_t>(reopened, "child"), 42);
}

TEST_F(structure_test, copy_belongs_to_its_destination_owner)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(source, "child", std::int32_t{42})};
    ASSERT_TRUE(child);
    ASSERT_TRUE(nori::resource::save_as(source, "source.nori"));
    ASSERT_TRUE(nori::resource::unload(source));

    const auto root{nori::resource::get("source.nori")};
    const nori::resource::handle source_child{nori::resource::get(root, "child")};
    const auto destination{nori::resource::create_root()};
    const auto copied{nori::resource::copy(source_child, destination)};
    ASSERT_TRUE(copied);
    EXPECT_NE(*copied, root);
    EXPECT_EQ(nori::resource::root_of(*copied), destination);
    EXPECT_EQ(nori::resource::path_of(*copied), nori::resource::path{"child"});
    nori::resource::remove(root);
    EXPECT_EQ(nori::resource::save(*copied).error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copied), 42);
    EXPECT_EQ(nori::resource::parent_of(*copied), destination);
}

TEST_F(structure_test, root_of_tracks_subtree_membership)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(source, "branch", nori::resource::empty)};
    ASSERT_TRUE(branch);
    const nori::resource::handle child{nori::resource::create(branch, "child", std::int32_t{42})};
    EXPECT_EQ(nori::resource::root_of(source), source);
    EXPECT_EQ(nori::resource::root_of(destination), destination);
    EXPECT_EQ(nori::resource::root_of(branch), source);
    EXPECT_EQ(nori::resource::root_of(child), source);
    EXPECT_EQ(nori::resource::root_of(branch), source);
    EXPECT_EQ(nori::resource::root_of(child), source);

    const auto parent{nori::resource::create(source, "parent", nori::resource::empty)};
    ASSERT_TRUE(nori::resource::move(branch, parent));
    EXPECT_EQ(nori::resource::root_of(branch), source);
    EXPECT_EQ(nori::resource::root_of(child), source);
    EXPECT_EQ(nori::resource::parent_of(branch), parent);
}

TEST_F(structure_test, invalid_rename)
{
    const nori::resource::handle root{create_resource("tree.nori")};
    const nori::resource::handle child{create_resource("tree.nori::child")};
    const nori::resource::handle sibling{create_resource("tree.nori::sibling")};

    const auto conflict{nori::resource::rename(child, "sibling")};
    ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::get("tree.nori::child"), child);

    const auto colon_name{nori::resource::rename(child, "valid:name")};
    ASSERT_TRUE(colon_name);
    EXPECT_EQ(nori::resource::get("tree.nori::valid:name"), child);
    ASSERT_TRUE(nori::resource::rename(child, "child"));
    EXPECT_EQ(nori::resource::parent_of(sibling), root);
}

TEST_F(structure_test, move_parent_conflict_retains_destination_handle)
{
    const auto root{create_resource("tree.nori")};
    const auto existing{create_resource("tree.nori::child")};
    const auto other{create_resource("tree.nori::other")};
    const auto source{create_resource("tree.nori::other/child")};
    const auto moved{nori::resource::move(source, root)};
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, existing);
    EXPECT_TRUE(children_of_or_fail(other).empty());
}

TEST_F(structure_test, move_name)
{
    const nori::resource::handle child{create_resource("tree.nori::source/child")};
    const nori::resource::handle source{nori::resource::get("tree.nori::source")};
    const nori::resource::handle destination{create_resource("tree.nori::destination")};
    const nori::resource::handle value{create_resource("tree.nori::value")};
    nori::resource::set(child, std::string{"preserved"});
    nori::resource::set(value, std::int32_t{42});

    const auto moved{nori::resource::move(source, value)};
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, source);
    EXPECT_EQ(nori::resource::parent_of(source), value);
    EXPECT_EQ(nori::resource::get("tree.nori::value/source/child"), child);
    ASSERT_TRUE(nori::resource::move(source, nori::resource::get("tree.nori")));
    EXPECT_EQ(nori::resource::parent_of(source), nori::resource::get("tree.nori"));

    ASSERT_TRUE(nori::resource::move(source, destination));
    EXPECT_EQ(nori::resource::name_of(source), "source");
    EXPECT_EQ(nori::resource::parent_of(source), destination);
    EXPECT_EQ(nori::resource::get("tree.nori::destination/source"), source);
    EXPECT_EQ(nori::resource::get("tree.nori::destination/source/child"), child);
    EXPECT_EQ(nori::resource::get<std::string>(child), "preserved");

    const auto cycle{nori::resource::move(destination, source)};
    ASSERT_FALSE(cycle);
    EXPECT_EQ(cycle.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::parent_of(destination), nori::resource::get("tree.nori"));
}

TEST_F(structure_test, same_parent_move_preserves_order_and_dirty_state)
{
    const nori::resource::handle root{create_resource("move-noop.nori")};
    const nori::resource::handle first{create_resource("move-noop.nori::first")};
    const nori::resource::handle last{create_resource("move-noop.nori::last")};
    const nori::resource::handle child{create_resource("move-noop.nori::first/child")};
    const nori::resource::handle sibling{create_resource("move-noop.nori::first/sibling")};
    nori::resource::set(child, std::int32_t{7});

    ASSERT_TRUE(nori::resource::move(first, root));
    ASSERT_TRUE(nori::resource::save(root));

    ASSERT_TRUE(nori::resource::move(first, root));
    ASSERT_TRUE(nori::resource::move(child, first));
    ASSERT_TRUE(nori::resource::move(last, root));
    EXPECT_EQ(children_of_or_fail(root), (std::vector<nori::resource::handle>{first, last}));
    EXPECT_EQ(children_of_or_fail(first), (std::vector<nori::resource::handle>{child, sibling}));
    EXPECT_EQ(nori::resource::parent_of(first), root);
    EXPECT_EQ(nori::resource::parent_of(child), first);
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 7);
    EXPECT_TRUE(nori::resource::unload(root));
}

TEST_F(structure_test, cross_root_copy)
{
    const nori::resource::handle source{create_resource("source.nori::folder/value")};
    const nori::resource::handle destination{create_resource("destination.nori::target")};
    nori::resource::set(source, std::int32_t{42});

    const nori::resource::handle destination_document{nori::resource::get("destination.nori")};
    const nori::resource::handle folder{nori::resource::get("source.nori::folder")};
    const auto copied_folder{nori::resource::copy(folder, destination)};
    ASSERT_TRUE(copied_folder);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copied_folder, "value"), 42);
    EXPECT_EQ(nori::resource::path_of(folder), bound_resource_path(directory_ / "source.nori", "folder"));
    EXPECT_EQ(nori::resource::path_of(source), bound_resource_path(directory_ / "source.nori", "folder/value"));
    EXPECT_EQ(nori::resource::get(nori::resource::path_of(source)), source);
    EXPECT_EQ(nori::resource::get<std::int32_t>(source), 42);
    ASSERT_TRUE(nori::resource::save(destination_document));
}

TEST_F(structure_test, reorder)
{
    const nori::resource::handle root{create_resource("ordered.nori")};
    const nori::resource::handle first{create_resource("ordered.nori::first")};
    const nori::resource::handle second{create_resource("ordered.nori::second")};
    const nori::resource::handle third{create_resource("ordered.nori::third")};
    const nori::resource::handle nested_first{create_resource("ordered.nori::first/a")};
    const nori::resource::handle nested_second{create_resource("ordered.nori::first/b")};

    ASSERT_TRUE(nori::resource::reorder(third, 0));
    ASSERT_TRUE(nori::resource::reorder(second, 2));
    ASSERT_TRUE(nori::resource::reorder(nested_second, 0));
    const std::vector expected_root{third, first, second};
    const std::vector expected_nested{nested_second, nested_first};
    EXPECT_EQ(children_of_or_fail(root), expected_root);
    EXPECT_EQ(children_of_or_fail(first), expected_nested);
    EXPECT_EQ(nori::resource::position_of(third), 0);
    EXPECT_EQ(nori::resource::position_of(first), 1);
    EXPECT_EQ(nori::resource::position_of(second), 2);
    EXPECT_EQ(nori::resource::position_of(nested_second), 0);
    EXPECT_EQ(nori::resource::parent_of(first), root);
    EXPECT_EQ(nori::resource::get("ordered.nori::first"), first);

    ASSERT_TRUE(nori::resource::reorder(first, 1));
    EXPECT_EQ(children_of_or_fail(root), expected_root);

    EXPECT_EQ(children_of_or_fail(root), expected_root);

    const std::filesystem::path path{directory_ / "ordered.nori"};
    ASSERT_TRUE(nori::resource::save_as(root, path));
    nori::resource::remove(root);

    const auto loaded_root{nori::resource::try_get("ordered.nori")};
    ASSERT_TRUE(loaded_root);
    const std::vector<nori::resource::handle> loaded_children{children_of_or_fail(*loaded_root)};
    ASSERT_EQ(loaded_children.size(), 3);
    EXPECT_EQ(nori::resource::name_of(loaded_children[0]), "third");
    EXPECT_EQ(nori::resource::name_of(loaded_children[1]), "first");
    EXPECT_EQ(nori::resource::name_of(loaded_children[2]), "second");
    const std::vector<nori::resource::handle> loaded_nested{children_of_or_fail(loaded_children[1])};
    ASSERT_EQ(loaded_nested.size(), 2);
    EXPECT_EQ(nori::resource::name_of(loaded_nested[0]), "b");
    EXPECT_EQ(nori::resource::name_of(loaded_nested[1]), "a");
}

TEST_F(structure_test, reorder_requires_parent)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const auto document_reorder{nori::resource::reorder(document, 0)};
    ASSERT_FALSE(document_reorder);
    EXPECT_EQ(document_reorder.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::position_of(document).error(), nori::resource::error_code::invalid_relationship);
}

TEST_F(structure_test, copy_preserves_resource_values)
{
    const nori::resource::handle source{create_resource("tree.nori::source/value")};
    const nori::resource::handle image{create_resource("tree.nori::source/image")};
    const nori::resource::handle binary{create_resource("tree.nori::source/binary")};
    const nori::resource::handle destination{create_resource("tree.nori::destination")};
    nori::resource::set(source, std::int32_t{42});
    nori::resource::set(
        image,
        nori::resource::image{
            .width = 1,
            .height = 1,
            .mip_levels = 1,
            .data = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}
        }
    );
    nori::resource::set(binary, nori::resource::binary{.data = {std::byte{5}, std::byte{6}}});

    const auto copied{nori::resource::copy(nori::resource::get("tree.nori::source"), destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::name_of(*copied), "source");
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(*copied, "value")), 42);

    const nori::resource::image_view copied_image{
        nori::resource::get<nori::resource::image_view>(*nori::resource::try_get(*copied, "image"))
    };
    EXPECT_EQ(copied_image.format, nori::resource::image_format::rgba8_unorm);
    EXPECT_EQ(copied_image.width, 1u);
    EXPECT_EQ(copied_image.height, 1u);
    EXPECT_TRUE(std::ranges::equal(copied_image.data, std::array{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}));

    const nori::resource::binary_view copied_binary{
        nori::resource::get<nori::resource::binary_view>(*nori::resource::try_get(*copied, "binary"))
    };
    EXPECT_TRUE(std::ranges::equal(copied_binary.data, std::array{std::byte{5}, std::byte{6}}));

    nori::resource::set(source, std::int32_t{99});
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(*copied, "value")), 42);

    const nori::resource::handle scalar_destination{create_resource("tree.nori::scalar_destination")};
    nori::resource::set(scalar_destination, std::string{"scalar"});
    const auto copied_to_scalar{nori::resource::copy(source, scalar_destination)};
    ASSERT_TRUE(copied_to_scalar);
    EXPECT_EQ(nori::resource::parent_of(*copied_to_scalar), scalar_destination);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*copied_to_scalar), 99);
}

TEST_F(structure_test, copy_conflict_overwrites_value_and_preserves_source)
{
    const auto source{create_resource("destination.nori::source/value")};
    const auto destination{create_resource("destination.nori::destination")};
    nori::resource::set(source, "new");
    const auto existing{create_resource("destination.nori::destination/value")};
    nori::resource::set(existing, "old");

    const auto conflict{nori::resource::copy(source, destination)};
    ASSERT_TRUE(conflict);
    EXPECT_EQ(*conflict, existing);
    EXPECT_EQ(children_of_or_fail(destination), std::vector{existing});
    EXPECT_EQ(nori::resource::get<std::string>(existing), "new");
    EXPECT_EQ(nori::resource::get<std::string>(source), "new");
    nori::resource::remove(existing);
    ASSERT_TRUE(nori::resource::copy(source, destination));
}

TEST_F(structure_test, hierarchy_root_membership)
{
    const nori::resource::handle file_document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(file_document, "parent", nori::resource::empty)};
    const nori::resource::handle child{nori::resource::create(parent, "child", std::int32_t{42})};
    EXPECT_FALSE(nori::resource::parent_of(file_document));
    EXPECT_EQ(nori::resource::root_of(file_document), file_document);
    EXPECT_EQ(nori::resource::root_of(parent), file_document);
    EXPECT_EQ(nori::resource::root_of(child), file_document);
    const auto destination{nori::resource::create(file_document, "destination", nori::resource::empty)};
    ASSERT_TRUE(nori::resource::move(parent, destination));
    EXPECT_EQ(nori::resource::parent_of(parent), destination);
    EXPECT_EQ(nori::resource::parent_of(child), parent);
    EXPECT_EQ(nori::resource::root_of(child), file_document);
}

TEST_F(structure_test, hierarchy_sibling_order_and_conflicts)
{
    const auto root{nori::resource::create_root()};
    const auto first{nori::resource::create(root, "same", std::int32_t{1})};
    const auto last{nori::resource::create(root, "last", nori::resource::empty)};
    const auto duplicate{nori::resource::create(last, "same", std::int32_t{2})};
    EXPECT_EQ(nori::resource::rename(last, "same").error(), nori::resource::error_code::name_conflict);
    ASSERT_TRUE(nori::resource::reorder(last, 0));
    EXPECT_EQ(children_of_or_fail(root), (std::vector{last, first}));
    ASSERT_TRUE(nori::resource::move(last, root));
    EXPECT_EQ(children_of_or_fail(root), (std::vector{last, first}));
    ASSERT_TRUE(nori::resource::move(duplicate, first));
    EXPECT_EQ(nori::resource::move(first, duplicate).error(), nori::resource::error_code::invalid_relationship);
}

TEST_F(structure_test, hierarchy_name_lookup_survives_renaming_and_node_growth)
{
    const nori::resource::handle parent{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(parent, "short", nori::resource::empty)};
    const nori::resource::handle sibling{nori::resource::create(parent, "sibling", true)};
    ASSERT_EQ(nori::resource::try_get(parent, "short"), child);

    for (int i{0}; i < 1024; ++i)
    {
        const nori::resource::handle temporary{nori::resource::create(parent, "temporary" + std::to_string(i), nori::resource::empty)};
        ASSERT_TRUE(temporary);
        if (i % 2 == 0)
        {
            nori::resource::remove(temporary);
        }
    }
    EXPECT_EQ(nori::resource::try_get(parent, "short"), child);

    const std::array names{std::string(256, 'a'), std::string(512, 'b'), std::string{"tiny"}};
    for (const auto& name : names)
    {
        const std::string previous{nori::resource::name_of(child)};
        ASSERT_TRUE(nori::resource::rename(child, name));
        EXPECT_FALSE(nori::resource::try_get(parent, nori::resource::path{previous}));
        EXPECT_EQ(nori::resource::try_get(parent, nori::resource::path{name}), child);
        EXPECT_EQ(nori::resource::try_get(parent, "sibling"), sibling);
    }

    ASSERT_TRUE(nori::resource::rename(child, nori::resource::name_of(child).substr(1)));
    EXPECT_FALSE(nori::resource::try_get(parent, "tiny"));
    EXPECT_EQ(nori::resource::try_get(parent, "iny"), child);
}

TEST_F(structure_test, hierarchy_name_lookup_tracks_reparenting_and_slot_reuse)
{
    const nori::resource::handle owner_root{nori::resource::create_root()};
    const nori::resource::handle first_parent{nori::resource::create(owner_root, "first", nori::resource::empty)};
    const nori::resource::handle second_parent{nori::resource::create(owner_root, "second", nori::resource::empty)};
    ASSERT_TRUE(first_parent);
    ASSERT_TRUE(second_parent);
    const std::string name(256, 'x');
    const nori::resource::handle child{nori::resource::create(first_parent, name, true)};
    const nori::resource::handle sibling{nori::resource::create(second_parent, "sibling", false)};
    ASSERT_EQ(nori::resource::try_get(first_parent, nori::resource::path{name}), child);
    ASSERT_EQ(nori::resource::try_get(second_parent, "sibling"), sibling);

    ASSERT_TRUE(nori::resource::move(child, second_parent));
    EXPECT_FALSE(nori::resource::try_get(first_parent, nori::resource::path{name}));
    EXPECT_EQ(nori::resource::try_get(second_parent, nori::resource::path{name}), child);
    ASSERT_TRUE(nori::resource::move(child, first_parent));
    EXPECT_EQ(nori::resource::try_get(first_parent, nori::resource::path{name}), child);
    EXPECT_FALSE(nori::resource::try_get(second_parent, nori::resource::path{name}));

    nori::resource::remove(child);
    const nori::resource::handle replacement{nori::resource::create(first_parent, name, std::int32_t{42})};
    EXPECT_NE(replacement, child);
    EXPECT_EQ(nori::resource::try_get(first_parent, nori::resource::path{name}), replacement);
    EXPECT_EQ(nori::resource::try_get(second_parent, "sibling"), sibling);
}

TEST_F(structure_test, hierarchy_removal_preserves_other_roots)
{
    const nori::resource::handle file_document{nori::resource::create_root()};
    const nori::resource::handle other_document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(file_document, "parent", nori::resource::empty)};
    ASSERT_TRUE(parent);
    const nori::resource::handle child{nori::resource::create(parent, "child", true)};
    ASSERT_TRUE(child);
    nori::resource::remove(file_document);
    EXPECT_EQ(nori::resource::root_of(other_document), other_document);
    EXPECT_TRUE(children_of_or_fail(other_document).empty());
    const nori::resource::handle replacement{nori::resource::create_root()};
    for (const auto removed : {file_document, parent, child})
    {
        EXPECT_NE(replacement, removed);
    }
}

TEST_F(structure_test, hierarchy_copy_survives_source_removal)
{
    const nori::resource::handle file_document{nori::resource::create_root()};
    const nori::resource::handle parent{nori::resource::create(file_document, "parent", std::int32_t{42})};
    ASSERT_TRUE(parent);
    const nori::resource::handle child{nori::resource::create(parent, "child", std::string{"value"})};
    ASSERT_TRUE(child);
    const auto destination{nori::resource::create(file_document, "destination", nori::resource::empty)};
    const auto result{nori::resource::copy(parent, destination)};
    ASSERT_TRUE(result);
    const auto copied{*result};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::parent_of(copied), destination);
    EXPECT_EQ(nori::resource::root_of(copied), file_document);
    EXPECT_EQ(nori::resource::try_get<std::int32_t>(copied).value(), 42);
    const nori::resource::handle copied_child{nori::resource::try_get(copied, "child").value()};
    ASSERT_TRUE(copied_child);
    EXPECT_NE(copied_child, child);
    EXPECT_EQ(nori::resource::try_get<std::string>(copied_child).value(), "value");
    nori::resource::remove(parent);
    EXPECT_EQ(nori::resource::parent_of(copied_child), copied);
    EXPECT_EQ(nori::resource::try_get(copied, "child"), copied_child);
    EXPECT_EQ(nori::resource::try_get<std::string>(copied_child).value(), "value");
}

TEST_F(structure_test, same_hierarchy_move_preserves_handles_names_and_borrowed_views)
{
    const nori::resource::handle source{nori::resource::create_root()};
    const nori::resource::handle destination{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(source, "branch", nori::resource::empty)};
    const nori::resource::handle payload{nori::resource::create(branch, "tiny", nori::resource::binary{{std::byte{42}, std::byte{7}}})};
    const nori::resource::handle icon{nori::resource::create(branch, "icon", nori::resource::image{nori::resource::image_format::rgba8_unorm, 1, 1, 1, std::vector<std::byte>(4, std::byte{9})})};
    const nori::resource::handle leaf{nori::resource::create(payload, "leaf", std::string_view{"preserved"})};
    const auto payload_before{nori::resource::get<nori::resource::binary_view>(payload)};
    const auto icon_before{nori::resource::get<nori::resource::image_view>(icon)};

    const auto copied{nori::resource::copy(branch, destination)};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(*copied, "tiny"), payload_before);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(*copied, "icon"), icon_before);
    EXPECT_EQ(nori::resource::get(source, "branch/tiny/leaf"), leaf);
    EXPECT_EQ(nori::resource::parent_of(payload), branch);
    EXPECT_EQ(nori::resource::root_of(leaf), source);

    const auto holding{nori::resource::create(source, "holding", nori::resource::empty)};
    ASSERT_TRUE(nori::resource::move(branch, holding));
    EXPECT_EQ(nori::resource::root_of(branch), source);
    EXPECT_EQ(nori::resource::parent_of(branch), holding);
    ASSERT_TRUE(nori::resource::move(branch, source));

    EXPECT_EQ(nori::resource::get(source, "branch/tiny"), payload);
    EXPECT_EQ(nori::resource::get(source, "branch/icon"), icon);
    EXPECT_EQ(nori::resource::get(source, "branch/tiny/leaf"), leaf);
    EXPECT_EQ(nori::resource::get<std::string>(leaf), "preserved");
    EXPECT_EQ(children_of_or_fail(branch), (std::vector{payload, icon}));
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(payload).data.data(), payload_before.data.data());
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(icon).data.data(), icon_before.data.data());
    ASSERT_EQ(payload_before.data.size(), 2);
    EXPECT_EQ(payload_before.data[0], std::byte{42});
    EXPECT_EQ(payload_before.data[1], std::byte{7});
    ASSERT_EQ(icon_before.data.size(), 4);
    EXPECT_TRUE(std::ranges::all_of(icon_before.data, [](std::byte value)
    {
        return value == std::byte{9};
    }));
}

TEST_F(structure_test, copy_cannot_replace_its_owner_file_binding)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "source", std::string{"preserved"})};
    ASSERT_TRUE(nori::resource::save_as(root, "owner.nori"));
    const auto destination{nori::resource::create(root, "destination", nori::resource::empty)};
    const auto copied{nori::resource::copy(source, destination)};
    ASSERT_TRUE(copied);

    EXPECT_EQ(nori::resource::save_as(*copied, "detached.nori").error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(directory_ / "owner.nori"));
    EXPECT_EQ(nori::resource::path_of(source), bound_resource_path(directory_ / "owner.nori", "source"));
    EXPECT_EQ(nori::resource::get<std::string>(source), "preserved");
    EXPECT_EQ(nori::resource::get<std::string>(*copied), "preserved");
    EXPECT_FALSE(std::filesystem::exists(directory_ / "detached.nori"));
}

TEST_F(structure_test, hierarchy_value_view_lifetime)
{
    const nori::resource::handle owner_root{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(owner_root, "data", nori::resource::binary{.data = {std::byte{42}}})};
    const auto view{nori::resource::try_get<nori::resource::binary_view>(value).value()};
    for (int i{0}; i < 1024; ++i)
    {
        const nori::resource::handle temporary{nori::resource::create(owner_root, "temporary" + std::to_string(i), std::string{"value"})};
        ASSERT_TRUE(temporary);
        if (i % 2 == 0)
        {
            nori::resource::remove(temporary);
        }
    }
    EXPECT_EQ(view.data.data(), nori::resource::try_get<nori::resource::binary_view>(value)->data.data());
    ASSERT_EQ(view.data.size(), 1);
    EXPECT_EQ(view.data.front(), std::byte{42});
}

TEST_F(structure_test, hierarchy_child_order_and_removal_preserve_parent)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle branch{nori::resource::create(document, "branch", std::int32_t{42})};
    ASSERT_TRUE(branch);
    const nori::resource::handle last{nori::resource::create(branch, "last", std::string{"value"})};
    EXPECT_EQ(nori::resource::try_get(branch, "last"), last);
    const nori::resource::handle first{nori::resource::create(branch, "first", nori::resource::empty)};

    ASSERT_TRUE(nori::resource::reorder(first, 0));
    EXPECT_EQ(children_of_or_fail(branch).front(), first);
    EXPECT_EQ(children_of_or_fail(branch).back(), last);
    EXPECT_EQ(nori::resource::try_get(branch, "last"), last);

    for (const auto child : children_of_or_fail(branch))
    {
        nori::resource::remove(child);
    }
    EXPECT_EQ(nori::resource::parent_of(branch), document);
    EXPECT_EQ(children_of_or_fail(document).front(), branch);
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::int32);
    EXPECT_EQ(nori::resource::try_get<std::int32_t>(branch), 42);
    EXPECT_TRUE(children_of_or_fail(branch).empty());
    EXPECT_FALSE(nori::resource::try_get(branch, "first"));
    EXPECT_FALSE(nori::resource::try_get(branch, "last"));
    nori::resource::set(branch, nori::resource::empty);
    EXPECT_EQ(nori::resource::type_of(branch), nori::resource::property_type::empty);
}

TEST_F(structure_test, hierarchy_copy_connection)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "source", std::int32_t{42})};
    ASSERT_TRUE(source);
    const nori::resource::handle child{nori::resource::create(source, "child", std::string{"value"})};
    ASSERT_TRUE(child);
    const nori::resource::handle destination{nori::resource::create(root, "destination", nori::resource::empty)};
    ASSERT_TRUE(destination);

    const auto result{nori::resource::copy(source, destination)};
    ASSERT_TRUE(result);
    const auto copied{*result};
    ASSERT_TRUE(copied);
    EXPECT_EQ(nori::resource::parent_of(copied), destination);
    EXPECT_EQ(nori::resource::name_of(copied), "source");
    ASSERT_EQ(children_of_or_fail(copied).size(), 1);
    EXPECT_EQ(nori::resource::try_get<std::string>(children_of_or_fail(copied).front()).value(), "value");

    ASSERT_TRUE(nori::resource::move(copied, destination));
    EXPECT_EQ(nori::resource::parent_of(copied), destination);
    const nori::resource::handle found{nori::resource::try_get(destination, nori::resource::path{"source/child"}).value()};
    ASSERT_TRUE(found);
    EXPECT_EQ(found, children_of_or_fail(copied).front());
}

TEST_F(structure_test, hierarchy_normalized_paths)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle folder{nori::resource::create(root, "folder", nori::resource::empty)};
    const nori::resource::handle value{nori::resource::create(folder, "value", std::int32_t{42})};
    ASSERT_TRUE(folder);
    ASSERT_TRUE(value);

    const nori::resource::handle found{nori::resource::try_get(root, nori::resource::path{"folder/value"}).value()};
    ASSERT_TRUE(found);
    EXPECT_EQ(found, value);

    const auto missing{nori::resource::try_get(root, nori::resource::path{"missing/value"})};
    ASSERT_FALSE(missing);
}
} // namespace
