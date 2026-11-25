#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cli_test_helpers.hpp"
#include "command/common.hpp"

namespace
{
class command_test : public cli_test_fixture
{
};

TEST_F(command_test, diff_replace_add_remove_reorder)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::answer"}, {"type", "int32"}, {"value", 41}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::first"}, {"type", "empty"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::second"}, {"type", "empty"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::removed"}, {"type", "empty"}}));

    ASSERT_TRUE(call("set", {{"path", rhs_ + "::answer"}, {"type", "int32"}, {"value", 42}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::first"}, {"type", "empty"}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::second"}, {"type", "empty"}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::added"}, {"type", "empty"}}));
    ASSERT_TRUE(call("reorder", {{"path", rhs_ + "::second"}, {"position", 0}}));

    const auto diff{call("diff", {{"before", lhs_}, {"after", rhs_}})};
    ASSERT_TRUE(diff);
    const auto& changes{*diff};
    EXPECT_TRUE(std::ranges::any_of(changes, [](const nlohmann::json& change)
    {
        return change.at("op") == "replace" && change.at("path") == "/answer";
    }));
    EXPECT_TRUE(std::ranges::any_of(changes, [](const nlohmann::json& change)
    {
        return change.at("op") == "add" && change.at("path") == "/added";
    }));
    EXPECT_TRUE(std::ranges::any_of(changes, [](const nlohmann::json& change)
    {
        return change.at("op") == "remove" && change.at("path") == "/removed";
    }));
    EXPECT_TRUE(std::ranges::any_of(changes, [](const nlohmann::json& change)
    {
        return change.at("op") == "reorder" && change.at("path") == "/second" && change.at("before") == 2 && change.at("after") == 0;
    }));
    EXPECT_EQ(
        std::ranges::count_if(changes, [](const nlohmann::json& change)
    {
        return change.at("op") == "reorder";
    }),
        1
    );
}

TEST_F(command_test, diff_reports_no_changes)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::answer"}, {"type", "int32"}, {"value", 42}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::answer"}, {"type", "int32"}, {"value", 42}}));

    const auto diff{call("diff", {{"before", lhs_}, {"after", rhs_}})};
    ASSERT_TRUE(diff);
    EXPECT_TRUE(diff->empty());
}

TEST_F(command_test, exists_reports_presence)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));

    const auto exists{call("exists", {{"path", lhs_ + "::player/name"}})};
    ASSERT_TRUE(exists);
    EXPECT_EQ(*exists, nlohmann::json(true));

    const auto missing{call("exists", {{"path", lhs_ + "::missing"}})};
    ASSERT_TRUE(missing);
    EXPECT_EQ(*missing, nlohmann::json(false));
}

TEST_F(command_test, get_returns_stored_value)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));

    const auto value{call("get", {{"path", lhs_ + "::player/name"}})};
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, nlohmann::json({{"type", "string"}, {"value", "hero"}}));
}

TEST_F(command_test, get_returns_opaque_summary)
{
    write_bytes(binary_path_, {0, 1, 2, 3});
    ASSERT_TRUE(call("import", {{"type", "binary"}, {"path", lhs_ + "::binary"}, {"source", binary_path_.generic_string()}}));

    const auto image_handle{nori::cli::command::create(lhs_ + "::image")};
    ASSERT_TRUE(image_handle);
    const nori::resource::image stored_image{
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = std::vector<std::byte>(4)
    };
    nori::resource::set(*image_handle, stored_image);

    const auto binary_result{call("get", {{"path", lhs_ + "::binary"}})};
    const auto image_result{call("get", {{"path", lhs_ + "::image"}})};
    ASSERT_TRUE(binary_result);
    ASSERT_TRUE(image_result);
    EXPECT_EQ(binary_result->dump(), R"({"type":"binary","size":4})");
    EXPECT_EQ(image_result->dump(), R"({"type":"image","format":"rgba8","width":1,"height":1,"mip_levels":1})");
}

TEST_F(command_test, get_rejects_missing_path)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));

    const auto missing{call("get", {{"path", lhs_ + "::player/missing"}})};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::cli::error_code::not_found);
}

TEST_F(command_test, get_rejects_document_path)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value"}, {"type", "string"}, {"value", "data"}}));

    const auto document{call("get", {{"path", lhs_}})};
    ASSERT_FALSE(document);
    EXPECT_EQ(document.error(), nori::cli::error_code::type_mismatch);
}

TEST_F(command_test, root_aliases_are_rejected_by_property_commands)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/value"}, {"type", "int32"}, {"value", 7}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::value"}, {"type", "int32"}, {"value", 8}}));
    const std::string dot_root{lhs_ + "::."};
    const std::string parent_root{lhs_ + "::player/.."};
    for (const std::string& alias : {dot_root, parent_root})
    {
        const auto value{call("get", {{"path", alias}})};
        ASSERT_FALSE(value);
        EXPECT_EQ(value.error(), nori::cli::error_code::type_mismatch);
        EXPECT_EQ(call("set", {{"path", alias}, {"type", "string"}, {"value", "bad"}}).error(), nori::cli::error_code::invalid_argument);
        EXPECT_EQ(call("rename", {{"path", alias}, {"name", "renamed"}}).error(), nori::cli::error_code::invalid_argument);
        EXPECT_EQ(call("reorder", {{"path", alias}, {"position", 0}}).error(), nori::cli::error_code::invalid_argument);
        EXPECT_EQ(call("copy", {{"source", alias}, {"destination", rhs_}}).error(), nori::cli::error_code::invalid_argument);
        EXPECT_EQ(call("move", {{"source", alias}, {"destination", rhs_}}).error(), nori::cli::error_code::invalid_argument);
        const auto diff{call("diff", {{"before", alias}, {"after", rhs_ + "::value"}})};
        ASSERT_FALSE(diff);
        EXPECT_EQ(diff.error(), nori::cli::error_code::type_mismatch);
    }
    EXPECT_EQ(nori::resource::get<std::int32_t>(lhs_ + "::player/value"), 7);

    write_bytes(binary_path_, {0, 1, 2});
    EXPECT_EQ(call("import", {{"type", "binary"}, {"path", dot_root}, {"source", binary_path_.generic_string()}}).error(), nori::cli::error_code::invalid_argument);
    const std::string missing_root{"missing-root.nori::."};
    EXPECT_EQ(call("set", {{"path", missing_root}, {"type", "string"}, {"value", "bad"}}).error(), nori::cli::error_code::invalid_argument);
    EXPECT_EQ(call("import", {{"type", "binary"}, {"path", missing_root}, {"source", binary_path_.generic_string()}}).error(), nori::cli::error_code::invalid_argument);
    EXPECT_FALSE(nori::resource::try_get("missing-root.nori"));
    EXPECT_FALSE(std::filesystem::exists("missing-root.nori"));

    const std::string missing_child_root{lhs_ + "::missing/.."};
    EXPECT_EQ(call("set", {{"path", missing_child_root}, {"type", "string"}, {"value", "bad"}}).error(), nori::cli::error_code::invalid_argument);
    EXPECT_EQ(call("import", {{"type", "binary"}, {"path", missing_child_root}, {"source", binary_path_.generic_string()}}).error(), nori::cli::error_code::invalid_argument);
    EXPECT_FALSE(nori::resource::try_get(lhs_ + "::missing"));
}

TEST_F(command_test, tree_depth)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/stats/hp"}, {"type", "int32"}, {"value", 100}}));

    const auto tree{call("tree", {{"path", lhs_}, {"depth", 2}})};
    ASSERT_TRUE(tree);
    ASSERT_TRUE(tree->contains("children"));
    const auto& player{tree->at("children").at(0)};
    EXPECT_EQ(player.at("name"), "player");
    EXPECT_EQ(player.at("type"), "empty");
    ASSERT_EQ(player.at("children").size(), 2u);
    EXPECT_EQ(player.at("children").at(0).at("name"), "name");
    EXPECT_TRUE(player.at("children").at(1).at("truncated").get<bool>());

    const auto shallow{call("tree", {{"path", lhs_ + "::player"}, {"depth", 0}})};
    ASSERT_TRUE(shallow);
    EXPECT_EQ(shallow->at("name"), "player");
    EXPECT_TRUE(shallow->at("truncated").get<bool>());
    EXPECT_FALSE(shallow->contains("children"));
}

TEST_F(command_test, tree_preserves_sibling_order)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/third"}, {"type", "string"}, {"value", "3"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/first"}, {"type", "string"}, {"value", "1"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/second"}, {"type", "string"}, {"value", "2"}}));
    ASSERT_TRUE(call("reorder", {{"path", lhs_ + "::player/first"}, {"position", 0}}));

    const auto player{nori::resource::try_get(nori::resource::path{lhs_ + "::player"})};
    ASSERT_TRUE(player);
    const auto stored_children{children_of_or_fail(*player)};
    ASSERT_EQ(stored_children.size(), 3u);
    EXPECT_EQ(nori::resource::name_of(stored_children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(stored_children[1]), "third");
    EXPECT_EQ(nori::resource::name_of(stored_children[2]), "second");

    const auto tree{call("tree", {{"path", lhs_ + "::player"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    const auto& children{tree->at("children")};
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children.at(0).at("name"), "first");
    EXPECT_EQ(children.at(0).at("value"), "1");
    EXPECT_EQ(children.at(1).at("name"), "third");
    EXPECT_EQ(children.at(1).at("value"), "3");
    EXPECT_EQ(children.at(2).at("name"), "second");
    EXPECT_EQ(children.at(2).at("value"), "2");
}

TEST_F(command_test, tree_rejects_missing_path)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));

    const auto missing{call("tree", {{"path", lhs_ + "::player/missing"}, {"depth", 1}})};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::cli::error_code::not_found);
}

TEST_F(command_test, set_stores_scalar_types)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/enabled"}, {"type", "bool"}, {"value", true}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/hp"}, {"type", "int32"}, {"value", 100}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/id"}, {"type", "int64"}, {"value", std::int64_t{1} << 40}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/speed"}, {"type", "float"}, {"value", 3.5}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/name"}, {"type", "string"}, {"value", "hero"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::player/empty"}, {"type", "empty"}}));

    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/enabled"}})->dump(), R"({"type":"bool","value":true})");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/hp"}})->dump(), R"({"type":"int32","value":100})");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/id"}})->dump(), R"({"type":"int64","value":1099511627776})");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/speed"}})->dump(), R"({"type":"float","value":3.5})");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/name"}})->dump(), R"({"type":"string","value":"hero"})");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::player/empty"}})->dump(), R"({"type":"empty"})");
}

TEST_F(command_test, set_type_change_preserves_children)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value"}, {"type", "string"}, {"value", "first"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value/child"}, {"type", "int32"}, {"value", 1}}));

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value"}, {"type", "string"}, {"value", "second"}}));
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::value/child"}})->get<bool>());

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value"}, {"type", "int32"}, {"value", 2}}));
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::value/child"}})->get<bool>());
}

TEST_F(command_test, copy_merges_children)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/value"}, {"type", "string"}, {"value", "preserved"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/value/child"}, {"type", "int32"}, {"value", 7}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::destination/source"}, {"type", "string"}, {"value", "old"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::destination/source/keep"}, {"type", "string"}, {"value", "destination"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::destination/source/value"}, {"type", "string"}, {"value", "old-value"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::destination/source/value/keep"}, {"type", "string"}, {"value", "nested-destination"}}));

    ASSERT_TRUE(call("copy", {{"source", lhs_ + "::source"}, {"destination", lhs_ + "::destination"}}));
    EXPECT_EQ(call("get", {{"path", lhs_ + "::destination/source"}})->at("value"), "source");
    ASSERT_TRUE(call("get", {{"path", lhs_ + "::destination/source/value"}}));
    EXPECT_EQ(call("get", {{"path", lhs_ + "::destination/source/value"}})->at("value"), "preserved");
    ASSERT_TRUE(call("get", {{"path", lhs_ + "::destination/source/value/keep"}}));
    ASSERT_TRUE(call("get", {{"path", lhs_ + "::destination/source/keep"}}));
    ASSERT_TRUE(call("get", {{"path", lhs_ + "::source/value"}}));
    EXPECT_EQ(call("get", {{"path", lhs_ + "::source/value"}})->at("value"), "preserved");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::source/value/child"}})->at("value"), 7);

    const auto merged_tree{call("tree", {{"path", lhs_ + "::destination/source"}, {"depth", 1}})};
    ASSERT_TRUE(merged_tree);
    ASSERT_EQ(merged_tree->at("children").size(), 2u);
    EXPECT_EQ(merged_tree->at("children").at(0).at("name"), "keep");
    EXPECT_EQ(merged_tree->at("children").at(1).at("name"), "value");
}

TEST_F(command_test, copy_merge_type_rules)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/child"}, {"type", "string"}, {"value", "source-child"}}));

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::same/source"}, {"type", "string"}, {"value", "old"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::same/source/keep"}, {"type", "string"}, {"value", "keep"}}));
    ASSERT_TRUE(call("copy", {{"source", lhs_ + "::source"}, {"destination", lhs_ + "::same"}}));
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::same/source/keep"}})->get<bool>());
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::same/source/child"}})->get<bool>());

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::different/source"}, {"type", "int32"}, {"value", 1}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::different/source/existing"}, {"type", "string"}, {"value", "existing"}}));
    ASSERT_TRUE(call("copy", {{"source", lhs_ + "::source"}, {"destination", lhs_ + "::different"}}));
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::different/source/existing"}})->get<bool>());
    EXPECT_TRUE(call("exists", {{"path", lhs_ + "::different/source/child"}})->get<bool>());
}

TEST_F(command_test, copy_no_op_to_same_parent)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/other"}, {"type", "string"}, {"value", "other"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source/child"}, {"type", "string"}, {"value", "child"}}));

    ASSERT_TRUE(call("copy", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent"}}));

    const auto tree{call("tree", {{"path", lhs_ + "::parent"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    ASSERT_EQ(tree->at("children").size(), 2u);
    EXPECT_EQ(tree->at("children").at(0).at("name"), "source");
    EXPECT_EQ(tree->at("children").at(1).at("name"), "other");
}

TEST_F(command_test, copy_rejects_invalid_relationship)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source/child"}, {"type", "string"}, {"value", "child"}}));

    const auto self_copy{call("copy", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent/source"}})};
    ASSERT_FALSE(self_copy);
    EXPECT_EQ(self_copy.error(), nori::cli::error_code::invalid_argument);

    const auto descendant_copy{call("copy", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent/source/child"}})};
    ASSERT_FALSE(descendant_copy);
    EXPECT_EQ(descendant_copy.error(), nori::cli::error_code::invalid_argument);
}

TEST_F(command_test, copy_rejects_new_descendant_without_side_effects)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));

    const auto descendant_copy{call("copy", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent/source/new"}})};
    ASSERT_FALSE(descendant_copy);
    EXPECT_EQ(descendant_copy.error(), nori::cli::error_code::invalid_argument);
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::parent/source/new"}})->get<bool>(), false);
}

TEST_F(command_test, copy_to_document_root)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::copied/value"}, {"type", "string"}, {"value", "copy"}}));
    ASSERT_TRUE(call("copy", {{"source", lhs_ + "::copied"}, {"destination", rhs_}}));
    EXPECT_EQ(call("get", {{"path", rhs_ + "::copied/value"}})->at("value"), "copy");
}

TEST_F(command_test, move_across_documents)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/value"}, {"type", "string"}, {"value", "preserved"}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::moved/source"}, {"type", "string"}, {"value", "old"}}));
    ASSERT_TRUE(call("set", {{"path", rhs_ + "::moved/source/keep"}, {"type", "string"}, {"value", "destination"}}));

    ASSERT_TRUE(call("move", {{"source", lhs_ + "::source"}, {"destination", rhs_ + "::moved"}}));
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::source"}})->get<bool>(), false);
    EXPECT_EQ(call("get", {{"path", rhs_ + "::moved/source/value"}})->at("value"), "preserved");
    EXPECT_EQ(call("get", {{"path", rhs_ + "::moved/source/keep"}})->at("value"), "destination");
}

TEST_F(command_test, move_no_op_to_same_parent)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/other"}, {"type", "string"}, {"value", "other"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source/child"}, {"type", "string"}, {"value", "child"}}));

    ASSERT_TRUE(call("move", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent"}}));

    const auto tree{call("tree", {{"path", lhs_ + "::parent"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    ASSERT_EQ(tree->at("children").size(), 2u);
    EXPECT_EQ(tree->at("children").at(0).at("name"), "source");
    EXPECT_EQ(tree->at("children").at(1).at("name"), "other");
}

TEST_F(command_test, move_rejects_invalid_relationship)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source/child"}, {"type", "string"}, {"value", "child"}}));

    const auto descendant_move{call("move", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent/source/child"}})};
    ASSERT_FALSE(descendant_move);
    EXPECT_EQ(descendant_move.error(), nori::cli::error_code::invalid_argument);
}

TEST_F(command_test, move_rejects_new_descendant_without_side_effects)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/source"}, {"type", "string"}, {"value", "source"}}));

    const auto descendant_move{call("move", {{"source", lhs_ + "::parent/source"}, {"destination", lhs_ + "::parent/source/new"}})};
    ASSERT_FALSE(descendant_move);
    EXPECT_EQ(descendant_move.error(), nori::cli::error_code::invalid_argument);
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::parent/source/new"}})->get<bool>(), false);
    EXPECT_EQ(call("get", {{"path", lhs_ + "::parent/source"}})->at("value"), "source");
}

TEST_F(command_test, move_creates_missing_parents)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/value"}, {"type", "string"}, {"value", "data"}}));
    ASSERT_TRUE(call("move", {{"source", lhs_ + "::source"}, {"destination", lhs_ + "::new/nested"}}));
    EXPECT_EQ(call("get", {{"path", lhs_ + "::new/nested/source/value"}})->at("value"), "data");
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::source"}})->get<bool>(), false);
}

TEST_F(command_test, move_to_document_root)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::moved/value"}, {"type", "string"}, {"value", "move"}}));
    ASSERT_TRUE(call("move", {{"source", lhs_ + "::moved"}, {"destination", rhs_}}));
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::moved"}})->get<bool>(), false);
    EXPECT_EQ(call("get", {{"path", rhs_ + "::moved/value"}})->at("value"), "move");
}

TEST_F(command_test, move_creates_missing_document)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::source/value"}, {"type", "string"}, {"value", "data"}}));
    ASSERT_FALSE(std::filesystem::exists(rhs_path_));

    ASSERT_TRUE(call("move", {{"source", lhs_ + "::source"}, {"destination", rhs_ + "::new/nested"}}));
    EXPECT_TRUE(std::filesystem::exists(rhs_path_));
    EXPECT_EQ(call("get", {{"path", rhs_ + "::new/nested/source/value"}})->at("value"), "data");
    EXPECT_FALSE(call("exists", {{"path", lhs_ + "::source"}})->get<bool>());
}

TEST_F(command_test, rename_updates_name)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/value"}, {"type", "string"}, {"value", "data"}}));

    ASSERT_TRUE(call("rename", {{"path", lhs_ + "::parent/value"}, {"name", "renamed"}}));
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::parent/value"}})->get<bool>(), false);
    EXPECT_EQ(call("get", {{"path", lhs_ + "::parent/renamed"}})->at("value"), "data");
}

TEST_F(command_test, rename_no_op_same_name)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/first"}, {"type", "string"}, {"value", "first"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/second"}, {"type", "string"}, {"value", "second"}}));

    ASSERT_TRUE(call("rename", {{"path", lhs_ + "::parent/first"}, {"name", "first"}}));

    const auto tree{call("tree", {{"path", lhs_ + "::parent"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    EXPECT_EQ(tree->at("children").at(0).at("name"), "first");
    EXPECT_EQ(tree->at("children").at(1).at("name"), "second");
}

TEST_F(command_test, rename_rejects_invalid_property_name)
{
    const std::string path{lhs_ + "::value"};
    ASSERT_TRUE(call("set", {{"path", path}, {"type", "string"}, {"value", "data"}}));
    for (const std::string_view name : {"", "invalid/name", "invalid::name", ".."})
    {
        const auto renamed{call("rename", {{"path", path}, {"name", name}})};
        ASSERT_FALSE(renamed);
        EXPECT_EQ(renamed.error(), nori::cli::error_code::invalid_argument);
        EXPECT_EQ(call("get", {{"path", path}})->at("value"), "data");
    }
}

TEST_F(command_test, rename_rejects_name_conflict)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/first"}, {"type", "string"}, {"value", "first"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/second"}, {"type", "string"}, {"value", "second"}}));

    const auto conflict{call("rename", {{"path", lhs_ + "::parent/first"}, {"name", "second"}})};
    ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error(), nori::cli::error_code::already_exists);
    EXPECT_EQ(call("get", {{"path", lhs_ + "::parent/first"}})->at("value"), "first");
    EXPECT_EQ(call("get", {{"path", lhs_ + "::parent/second"}})->at("value"), "second");
}

TEST_F(command_test, reorder_moves_to_front)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/first"}, {"type", "string"}, {"value", "first"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/second"}, {"type", "string"}, {"value", "second"}}));

    ASSERT_TRUE(call("reorder", {{"path", lhs_ + "::parent/second"}, {"position", 0}}));

    const auto tree{call("tree", {{"path", lhs_ + "::parent"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    ASSERT_EQ(tree->at("children").size(), 2u);
    EXPECT_EQ(tree->at("children").at(0).at("name"), "second");
    EXPECT_EQ(tree->at("children").at(1).at("name"), "first");
}

TEST_F(command_test, reorder_no_op_same_position)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/first"}, {"type", "string"}, {"value", "first"}}));
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/second"}, {"type", "string"}, {"value", "second"}}));

    ASSERT_TRUE(call("reorder", {{"path", lhs_ + "::parent/first"}, {"position", 0}}));

    const auto tree{call("tree", {{"path", lhs_ + "::parent"}, {"depth", 1}})};
    ASSERT_TRUE(tree);
    EXPECT_EQ(tree->at("children").at(0).at("name"), "first");
    EXPECT_EQ(tree->at("children").at(1).at("name"), "second");
}

TEST_F(command_test, reorder_rejects_negative_position)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/first"}, {"type", "string"}, {"value", "first"}}));

    const auto negative{call("reorder", {{"path", lhs_ + "::parent/first"}, {"position", -1}})};
    ASSERT_FALSE(negative);
    EXPECT_EQ(negative.error(), nori::cli::error_code::invalid_argument);
}

TEST_F(command_test, del_removes_property)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/value"}, {"type", "string"}, {"value", "data"}}));

    ASSERT_TRUE(call("del", {{"path", lhs_ + "::parent/value"}}));
    EXPECT_EQ(call("exists", {{"path", lhs_ + "::parent/value"}})->get<bool>(), false);
}

TEST_F(command_test, del_removes_document)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::value"}, {"type", "string"}, {"value", "data"}}));
    ASSERT_TRUE(std::filesystem::exists(lhs_path_));
    ASSERT_TRUE(call("del", {{"path", lhs_}}));
    EXPECT_FALSE(std::filesystem::exists(lhs_path_));
    EXPECT_EQ(call("exists", {{"path", lhs_}})->get<bool>(), false);
}

TEST_F(command_test, del_rejects_missing_path)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::parent/value"}, {"type", "string"}, {"value", "data"}}));

    const auto missing{call("del", {{"path", lhs_ + "::parent/missing"}})};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::cli::error_code::not_found);
    EXPECT_EQ(call("get", {{"path", lhs_ + "::parent/value"}})->at("value"), "data");
}
} // namespace
