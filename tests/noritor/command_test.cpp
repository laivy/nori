#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "clipboard.hpp"
#include "command.hpp"
#include "document.hpp"
#include "exit_workflow.hpp"
#include "log.hpp"
#include "noritor_test_helpers.hpp"
#include "preview.hpp"
#include "temporary_storage.hpp"

using namespace nori::editor;

namespace
{
std::filesystem::path test_normalized_path(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path normalized{std::filesystem::weakly_canonical(path, ec)};
    if (ec)
    {
        ADD_FAILURE() << "weakly_canonical failed: " << ec.message();
        return {};
    }
    return normalized.lexically_normal();
}

class command_test : public noritor_test_fixture
{
};

TEST_F(command_test, rename_rejects_invalid_property_name)
{
    const auto target{create_integer("value", 1)};
    command_manager commands;
    for (const std::string_view name : {"", "invalid/name", "invalid::name", ".."})
    {
        const auto renamed{commands.execute(std::make_unique<set_name_command>(root_, target, std::string{name}))};
        ASSERT_FALSE(renamed.execution.result);
        EXPECT_EQ(renamed.execution.result.error(), nori::resource::error_code::invalid_path);
        EXPECT_EQ(nori::resource::name_of(target), "value");
        EXPECT_EQ(integer("value"), 1);
    }
}

TEST_F(command_test, set_undo_preflight)
{
    create_integer("one", 1);
    const nori::resource::handle two{create_integer("two", 1)};
    command_manager commands;

    const command_result executed{commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{property("one"), two}, 2))};
    ASSERT_TRUE(executed.execution.result);
    ASSERT_TRUE(executed.execution.changed);
    nori::resource::remove(two);

    const command_result undone{commands.undo()};
    EXPECT_FALSE(undone.execution.result);
    EXPECT_EQ(integer("one"), 2);
    EXPECT_TRUE(commands.can_undo());
}

TEST_F(command_test, external_targets)
{
    const nori::resource::handle value{create_integer("value", 1)};
    const nori::resource::handle other_root{nori::resource::create_root()};
    const nori::resource::handle other{create_relative(other_root, "value").value()};
    nori::resource::set(other, std::int32_t{2});
    command_manager commands;

    const command_result result{commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value, other}, 3))};

    EXPECT_FALSE(result.execution.result);
    EXPECT_EQ(result.execution.result.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>(other), 2);
    EXPECT_FALSE(commands.can_undo());
}

TEST_F(command_test, type_undo)
{
    const nori::resource::handle integer{create_integer("integer", 7)};
    const nori::resource::handle folder{create_value("folder", std::monostate{})};
    const nori::resource::handle child{create_integer("folder/child", 9)};
    const nori::resource::image image{
        .width = 2,
        .height = 1,
        .mip_levels = 1,
        .data = std::vector<std::byte>(8, std::byte{0x7F})
    };
    const nori::resource::binary binary{.data = std::vector<std::byte>(3, std::byte{0x2A})};
    const nori::resource::handle image_handle{create_value("image", image)};
    const nori::resource::handle binary_handle{create_value("binary", binary)};
    command_manager commands;

    const command_result changed{commands.execute(std::make_unique<set_type_command>(root_, std::vector{integer, folder, image_handle, binary_handle}, nori::resource::property_type::int64))};
    ASSERT_TRUE(changed.execution.result);
    EXPECT_TRUE(changed.execution.changed);
    EXPECT_EQ(nori::resource::type_of(integer), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::type_of(folder), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::type_of(image_handle), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::type_of(binary_handle), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::get<std::int64_t>(integer), 0);
    EXPECT_EQ(nori::resource::get<std::int64_t>(folder), 0);
    EXPECT_EQ(children_of_or_fail(folder), std::vector<nori::resource::handle>{child});

    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    EXPECT_EQ(nori::resource::type_of(integer), nori::resource::property_type::int32);
    EXPECT_EQ(nori::resource::get<std::int32_t>(integer), 7);
    EXPECT_EQ(nori::resource::type_of(folder), nori::resource::property_type::empty);
    EXPECT_EQ(children_of_or_fail(folder), std::vector<nori::resource::handle>{child});
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 9);
    EXPECT_EQ(nori::resource::type_of(image_handle), nori::resource::property_type::image);
    EXPECT_EQ(std::get<nori::resource::image>(*copy_property_snapshot(image_handle)), image);
    EXPECT_EQ(nori::resource::type_of(binary_handle), nori::resource::property_type::binary);
    EXPECT_EQ(std::get<nori::resource::binary>(*copy_property_snapshot(binary_handle)), binary);

    const command_result redone{commands.redo()};
    ASSERT_TRUE(redone.execution.result);
    EXPECT_EQ(nori::resource::type_of(image_handle), nori::resource::property_type::int64);
    EXPECT_EQ(nori::resource::get<std::int64_t>(image_handle), 0);
}

TEST_F(command_test, saved_revision)
{
    const nori::resource::handle value{create_integer("value", 1)};
    command_manager commands;
    commands.mark_saved();

    ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 2)).execution.result);
    EXPECT_TRUE(commands.dirty());
    commands.mark_saved();
    EXPECT_FALSE(commands.dirty());

    ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 3)).execution.result);
    EXPECT_TRUE(commands.dirty());
    ASSERT_TRUE(commands.undo().execution.result);
    EXPECT_FALSE(commands.dirty());
    EXPECT_EQ(integer("value"), 2);
    ASSERT_TRUE(commands.redo().execution.result);
    EXPECT_TRUE(commands.dirty());
    EXPECT_EQ(integer("value"), 3);
}

TEST_F(command_test, remove_undo)
{
    temporary_storage temporary;
    const auto parent{create_relative(root_, "parent")};
    ASSERT_TRUE(parent);
    nori::resource::set(*parent, nori::resource::empty);
    const auto child{create_relative(root_, "parent/child")};
    ASSERT_TRUE(child);
    nori::resource::set(*child, std::int32_t{7});
    command_manager commands;

    const command_result removed{commands.execute(std::make_unique<remove_command>(&temporary, root_, std::vector{*parent}))};
    ASSERT_TRUE(removed.execution.result);
    EXPECT_EQ(nori::resource::try_get(root_, "parent").error(), nori::resource::error_code::not_found);
    EXPECT_EQ(nori::resource::try_get(root_, "parent/child").error(), nori::resource::error_code::not_found);

    const command_result restored{commands.undo()};
    ASSERT_TRUE(restored.execution.result);
    const auto restored_child{nori::resource::try_get<std::int32_t>(root_, "parent/child")};
    ASSERT_TRUE(restored_child);
    EXPECT_EQ(*restored_child, 7);
}

TEST_F(command_test, no_op_redo)
{
    const nori::resource::handle value{create_integer("value", 1)};
    command_manager commands;
    commands.mark_saved();

    ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 2)).execution.result);
    ASSERT_TRUE(commands.undo().execution.result);
    ASSERT_TRUE(commands.can_redo());

    const command_result no_op{commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 1))};
    ASSERT_TRUE(no_op.execution.result);
    EXPECT_FALSE(no_op.execution.changed);
    EXPECT_TRUE(commands.can_redo());
    EXPECT_FALSE(commands.dirty());

    ASSERT_TRUE(commands.redo().execution.result);
    EXPECT_EQ(integer("value"), 2);
}

TEST_F(command_test, merged_no_op)
{
    const nori::resource::handle value{create_integer("value", 10)};
    command_manager commands;
    commands.mark_saved();

    ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 11)).execution.result);
    ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 10)).execution.result);

    EXPECT_EQ(integer("value"), 10);
    EXPECT_FALSE(commands.can_undo());
    EXPECT_FALSE(commands.can_redo());
    EXPECT_FALSE(commands.dirty());
}

TEST_F(command_test, history_limit)
{
    const nori::resource::handle target{create_integer("value", 0)};
    command_manager commands;
    for (std::int32_t value{1}; value <= 257; ++value)
    {
        ASSERT_TRUE(commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{target}, value)).execution.result);
        commands.break_merge();
    }

    std::size_t undo_count{};
    while (commands.can_undo())
    {
        ASSERT_TRUE(commands.undo().execution.result);
        ++undo_count;
    }
    EXPECT_EQ(undo_count, command_manager::max_size);
    EXPECT_EQ(integer("value"), 1);
}

TEST_F(command_test, duplicate_scalar_parent)
{
    const nori::resource::handle parent{create_integer("parent", 1)};
    const nori::resource::handle source{create_value("parent/source", std::string{"value"})};
    temporary_storage temporary;
    command_manager commands;

    const command_result copied{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{source, parent, "source Copy"}}))};
    ASSERT_TRUE(copied.execution.result);
    ASSERT_TRUE(copied.execution.changed);
    const auto duplicate{nori::resource::try_get<std::string>(parent, "source Copy")};
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(*duplicate, "value");

    ASSERT_TRUE(commands.undo().execution.result);
    EXPECT_EQ(nori::resource::try_get(parent, "source Copy").error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(commands.redo().execution.result);
    EXPECT_EQ(*nori::resource::try_get<std::string>(parent, "source Copy"), "value");
}

TEST_F(command_test, overwrite_backup)
{
    temporary_storage temporary;
    const nori::resource::handle source{create_value("source", std::string{"new"})};
    const nori::resource::handle destination{create_value("destination", std::monostate{})};
    const nori::resource::handle old{create_value("destination/value", std::string{"old"})};
    command_manager commands;

    const command_result executed{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{source, destination, "value"}}))};
    ASSERT_TRUE(executed.execution.result);
    ASSERT_EQ(executed.execution.effect.invalidated.front(), old);
    const nori::resource::handle first_copy{executed.execution.effect.created.front()};
    EXPECT_EQ(nori::resource::get<std::string>(first_copy), "new");

    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    const nori::resource::handle restored{undone.execution.effect.created.front()};
    EXPECT_EQ(nori::resource::get<std::string>(restored), "old");

    const command_result redone{commands.redo()};
    ASSERT_TRUE(redone.execution.result);
    EXPECT_EQ(redone.execution.effect.invalidated.front(), restored);
    EXPECT_EQ(nori::resource::get<std::string>(*nori::resource::try_get(destination, "value")), "new");
    EXPECT_NE(first_copy, *nori::resource::try_get(destination, "value"));
}

TEST_F(command_test, copy_conflict)
{
    temporary_storage temporary;
    const nori::resource::handle one{create_integer("one", 1)};
    const nori::resource::handle two{create_integer("two", 2)};
    const nori::resource::handle destination{create_value("destination", std::monostate{})};
    command_manager commands;

    const command_result duplicate_name{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{one, destination, "value"}, {two, destination, "value"}}))};
    ASSERT_FALSE(duplicate_name.execution.result);
    EXPECT_EQ(duplicate_name.execution.result.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::try_get(destination, "value").error(), nori::resource::error_code::not_found);

    create_value("destination/value", std::string{"preserved"});
    const command_result type_conflict{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{one, destination, "value"}, {two, destination, "other"}}))};
    ASSERT_FALSE(type_conflict.execution.result);
    EXPECT_EQ(type_conflict.execution.result.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::get<std::string>(*nori::resource::try_get(destination, "value")), "preserved");
    EXPECT_EQ(nori::resource::try_get(destination, "other").error(), nori::resource::error_code::not_found);
}

TEST_F(command_test, move_undo)
{
    const nori::resource::handle old_parent{create_integer("old", 1)};
    const nori::resource::handle child{create_integer("old/child", 2)};
    const nori::resource::handle destination{create_value("destination", std::int32_t{3})};
    command_manager commands;

    const command_result moved{commands.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{child, destination}}))};
    ASSERT_TRUE(moved.execution.result);
    EXPECT_EQ(moved.execution.effect.updated, std::vector{child});
    EXPECT_EQ(nori::resource::parent_of(child), destination);
    EXPECT_EQ(nori::resource::get<std::int32_t>(destination), 3);

    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    EXPECT_EQ(undone.execution.effect.updated, std::vector{child});
    EXPECT_EQ(nori::resource::parent_of(child), old_parent);
    EXPECT_EQ(nori::resource::get<std::int32_t>(destination), 3);
}

TEST_F(command_test, block_move)
{
    const nori::resource::handle left{create_value("left", std::monostate{})};
    const nori::resource::handle one{create_integer("left/one", 1)};
    const nori::resource::handle two{create_integer("left/two", 2)};
    const nori::resource::handle three{create_integer("left/three", 3)};
    const nori::resource::handle right{create_value("right", std::monostate{})};
    const nori::resource::handle before{create_integer("right/before", 4)};
    const nori::resource::handle after{create_integer("right/after", 5)};
    command_manager commands;

    const command_result moved{commands.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{two, right, 1}, {three, right, 2}}))};
    ASSERT_TRUE(moved.execution.result);
    EXPECT_TRUE(moved.execution.changed);
    const std::vector expected_left_after{one};
    const std::vector expected_right_after{before, two, three, after};
    EXPECT_EQ(children_of_or_fail(left), expected_left_after);
    EXPECT_EQ(children_of_or_fail(right), expected_right_after);

    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    const std::vector expected_left_before{one, two, three};
    const std::vector expected_right_before{before, after};
    EXPECT_EQ(children_of_or_fail(left), expected_left_before);
    EXPECT_EQ(children_of_or_fail(right), expected_right_before);

    const command_result redone{commands.redo()};
    ASSERT_TRUE(redone.execution.result);
    EXPECT_EQ(children_of_or_fail(left), expected_left_after);
    EXPECT_EQ(children_of_or_fail(right), expected_right_after);
}

TEST_F(command_test, block_order)
{
    const nori::resource::handle first{create_integer("first", 1)};
    const nori::resource::handle second{create_integer("second", 2)};
    const nori::resource::handle third{create_integer("third", 3)};
    const nori::resource::handle fourth{create_integer("fourth", 4)};
    const nori::resource::handle fifth{create_integer("fifth", 5)};
    command_manager commands;

    ASSERT_TRUE(commands.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{second, root_, 2}, {third, root_, 3}})).execution.result);
    const std::vector expected_after{first, fourth, second, third, fifth};
    const std::vector expected_before{first, second, third, fourth, fifth};
    EXPECT_EQ(children_of_or_fail(root_), expected_after);

    ASSERT_TRUE(commands.undo().execution.result);
    EXPECT_EQ(children_of_or_fail(root_), expected_before);
    ASSERT_TRUE(commands.redo().execution.result);
    EXPECT_EQ(children_of_or_fail(root_), expected_after);
}

TEST_F(command_test, remove_undo_order)
{
    temporary_storage temporary;
    const nori::resource::handle first{create_integer("first", 1)};
    const nori::resource::handle second{create_integer("second", 2)};
    const nori::resource::handle third{create_integer("third", 3)};
    const nori::resource::handle fourth{create_integer("fourth", 4)};
    command_manager commands;

    ASSERT_TRUE(commands.execute(std::make_unique<remove_command>(&temporary, root_, std::vector{fourth, second})).execution.result);
    const std::vector expected_remaining{first, third};
    EXPECT_EQ(children_of_or_fail(root_), expected_remaining);
    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    const std::vector<nori::resource::handle> children{children_of_or_fail(root_)};
    ASSERT_EQ(children.size(), 4);
    EXPECT_EQ(nori::resource::name_of(children[0]), "first");
    EXPECT_EQ(nori::resource::name_of(children[1]), "second");
    EXPECT_EQ(nori::resource::name_of(children[2]), "third");
    EXPECT_EQ(nori::resource::name_of(children[3]), "fourth");
}

TEST_F(command_test, overwrite_position)
{
    temporary_storage temporary;
    const nori::resource::handle source_one{create_integer("source_one", 1)};
    const nori::resource::handle source_two{create_integer("source_two", 2)};
    const nori::resource::handle destination{create_value("destination", std::monostate{})};
    const nori::resource::handle before{create_integer("destination/before", 3)};
    create_integer("destination/source_one", 9);
    const nori::resource::handle after{create_integer("destination/after", 4)};
    command_manager commands;

    const command_result copied{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{source_one, destination, "source_one", 1}, {source_two, destination, "source_two", 2}}))};
    ASSERT_TRUE(copied.execution.result);
    const std::vector<nori::resource::handle> copied_children{children_of_or_fail(destination)};
    ASSERT_EQ(copied_children.size(), 4);
    EXPECT_EQ(copied_children[0], before);
    EXPECT_EQ(nori::resource::name_of(copied_children[1]), "source_one");
    EXPECT_EQ(nori::resource::name_of(copied_children[2]), "source_two");
    EXPECT_EQ(copied_children[3], after);

    const command_result undone{commands.undo()};
    ASSERT_TRUE(undone.execution.result);
    const std::vector<nori::resource::handle> restored_children{children_of_or_fail(destination)};
    ASSERT_EQ(restored_children.size(), 3);
    EXPECT_EQ(restored_children[0], before);
    EXPECT_EQ(nori::resource::name_of(restored_children[1]), "source_one");
    EXPECT_EQ(nori::resource::get<std::int32_t>(restored_children[1]), 9);
    EXPECT_EQ(restored_children[2], after);

    ASSERT_TRUE(commands.redo().execution.result);
    const std::vector<nori::resource::handle> redone_children{children_of_or_fail(destination)};
    ASSERT_EQ(redone_children.size(), 4);
    EXPECT_EQ(nori::resource::name_of(redone_children[1]), "source_one");
    EXPECT_EQ(nori::resource::name_of(redone_children[2]), "source_two");
}

TEST_F(command_test, move_rollback)
{
    const nori::resource::handle first{create_value("first", std::monostate{})};
    const nori::resource::handle second{create_value("second", std::monostate{})};
    document_editor target{directory_ / "document.nori", root_};

    const auto moved{target.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{first, second}, {second, first}}))};

    ASSERT_FALSE(moved);
    EXPECT_EQ(moved.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::parent_of(first), root_);
    EXPECT_EQ(nori::resource::parent_of(second), root_);
    EXPECT_EQ(nori::resource::try_get(root_, "first"), first);
    EXPECT_EQ(nori::resource::try_get(root_, "second"), second);
    EXPECT_FALSE(target.can_undo());
}

TEST_F(command_test, undo_order)
{
    temporary_storage temporary;
    const nori::resource::handle source_one{create_value("source_one", std::string{"new one"})};
    const nori::resource::handle source_two{create_value("source_two", std::string{"new two"})};
    const nori::resource::handle destination{create_value("destination", std::monostate{})};
    create_value("destination/one", std::string{"old one"});
    create_value("destination/two", std::string{"old two"});
    command_manager commands;

    const command_result copied{commands.execute(std::make_unique<copy_command>(&temporary, root_, std::vector<copy_source>{{source_one, destination, "one"}, {source_two, destination, "two"}}))};
    ASSERT_TRUE(copied.execution.result);
    ASSERT_EQ(copied.execution.effect.created.size(), 2);
    const std::vector<nori::resource::handle> copied_handles{copied.execution.effect.created};

    const command_result copy_undo{commands.undo()};
    ASSERT_TRUE(copy_undo.execution.result);
    ASSERT_EQ(copy_undo.execution.effect.invalidated.size(), 2);
    EXPECT_EQ(copy_undo.execution.effect.invalidated, copied_handles);
    ASSERT_EQ(copy_undo.execution.effect.created.size(), 2);
    EXPECT_EQ(nori::resource::get<std::string>(copy_undo.execution.effect.created[0]), "old one");
    EXPECT_EQ(nori::resource::get<std::string>(copy_undo.execution.effect.created[1]), "old two");

    const nori::resource::handle left{create_value("left", std::monostate{})};
    const nori::resource::handle right{create_value("right", std::monostate{})};
    const nori::resource::handle first{create_integer("left/first", 1)};
    const nori::resource::handle second{create_integer("left/second", 2)};
    command_manager moves;
    ASSERT_TRUE(moves.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{first, right}, {second, right}})).execution.result);
    const command_result move_undo{moves.undo()};
    ASSERT_TRUE(move_undo.execution.result);
    const std::vector<nori::resource::handle> expected_updated{first, second};
    EXPECT_EQ(move_undo.execution.effect.updated, expected_updated);
    EXPECT_EQ(nori::resource::parent_of(first), left);
    EXPECT_EQ(nori::resource::parent_of(second), left);
    EXPECT_EQ(nori::resource::type_of(right), nori::resource::property_type::empty);
}

TEST_F(command_test, top_level_selection)
{
    const nori::resource::handle parent{create_value("parent", std::monostate{})};
    const nori::resource::handle child{create_integer("parent/child", 1)};
    const nori::resource::handle other{create_integer("other", 2)};
    const std::vector<nori::resource::handle> input{child, other, parent, parent};
    const std::vector<nori::resource::handle> expected{other, parent};
    selection_manager selection{root_};
    selection.select_all(input);
    EXPECT_EQ(selection.top_level(), expected);
    EXPECT_EQ(selection.selected(), parent);
    EXPECT_TRUE(selection.contains(child));

    selection.clear();
    EXPECT_TRUE(selection.top_level().empty());
}

TEST_F(command_test, no_op_effects)
{
    const nori::resource::handle value{create_integer("value", 3)};
    const nori::resource::handle folder{create_value("folder", std::monostate{})};
    const nori::resource::handle child{create_integer("folder/child", 4)};
    command_manager commands;

    const command_result same_name{commands.execute(std::make_unique<set_name_command>(root_, value, "value"))};
    ASSERT_TRUE(same_name.execution.result);
    EXPECT_FALSE(same_name.execution.changed);

    const command_result same_value{commands.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{value}, 3))};
    ASSERT_TRUE(same_value.execution.result);
    EXPECT_FALSE(same_value.execution.changed);

    const command_result same_type{commands.execute(std::make_unique<set_type_command>(root_, std::vector{value}, nori::resource::property_type::int32))};
    ASSERT_TRUE(same_type.execution.result);
    EXPECT_FALSE(same_type.execution.changed);

    const command_result same_parent{commands.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{child, folder}}))};
    ASSERT_TRUE(same_parent.execution.result);
    EXPECT_FALSE(same_parent.execution.changed);
    EXPECT_EQ(nori::resource::parent_of(child), *nori::resource::try_get(root_, "folder"));

    const std::vector root_children{children_of_or_fail(root_)};
    ASSERT_EQ(root_children.size(), 2);
    const command_result same_position{commands.execute(std::make_unique<move_command>(root_, std::vector<move_command::entry>{{folder, root_, 1}}))};
    ASSERT_TRUE(same_position.execution.result);
    EXPECT_FALSE(same_position.execution.changed);
    EXPECT_EQ(children_of_or_fail(root_), root_children);
    EXPECT_FALSE(commands.can_undo());
    EXPECT_FALSE(commands.can_redo());

    const command_result renamed{commands.execute(std::make_unique<set_name_command>(root_, value, "renamed"))};
    ASSERT_TRUE(renamed.execution.result);
    EXPECT_EQ(renamed.execution.effect.updated, std::vector{value});
}

TEST_F(command_test, undo_redo)
{
    temporary_storage temporary;
    command_manager create_commands;
    command_manager remove_commands;
    command_manager rename_commands;

    ASSERT_TRUE(create_commands.execute(std::make_unique<create_command<std::int32_t>>(root_, root_, "created", 5)).execution.result);
    ASSERT_TRUE(create_commands.undo().execution.result);
    EXPECT_EQ(nori::resource::try_get(root_, "created").error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(create_commands.redo().execution.result);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(root_, "created")), 5);

    ASSERT_TRUE(rename_commands.execute(std::make_unique<set_name_command>(root_, property("created"), "renamed")).execution.result);
    ASSERT_TRUE(rename_commands.undo().execution.result);
    EXPECT_TRUE(nori::resource::try_get(root_, "created"));
    ASSERT_TRUE(rename_commands.redo().execution.result);
    EXPECT_TRUE(nori::resource::try_get(root_, "renamed"));

    ASSERT_TRUE(remove_commands.execute(std::make_unique<remove_command>(&temporary, root_, std::vector{property("renamed")})).execution.result);
    ASSERT_TRUE(remove_commands.undo().execution.result);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(root_, "renamed")), 5);
    ASSERT_TRUE(remove_commands.redo().execution.result);
    EXPECT_EQ(nori::resource::try_get(root_, "renamed").error(), nori::resource::error_code::not_found);
}

TEST_F(command_test, model_import_undo)
{
    const nori::resource::handle destination{create_value("assets", std::monostate{})};
    nori::asset::cooked_model model{
        .model = nori::resource::binary{.data = {std::byte{1}, std::byte{2}}},
        .meshes = {
            nori::asset::cooked_binary{
                .name = "mesh-0000",
                .binary = nori::resource::binary{.data = {std::byte{3}}}
            }
        },
        .animations = {nori::asset::cooked_binary{.name = "clip-0000", .binary = nori::resource::binary{.data = {std::byte{4}}}}},
        .textures = {nori::asset::cooked_texture{.name = "texture-0000-bc7-srgb", .image = nori::resource::image{.width = 1, .height = 1, .mip_levels = 1, .data = {std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}}}}}
    };
    command_manager commands;

    const command_result imported{commands.execute(std::make_unique<import_model_command>(root_, destination, "character", std::move(model)))};
    ASSERT_TRUE(imported.execution.result);
    ASSERT_TRUE(imported.execution.changed);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(root_, "assets/character").data.size(), 2u);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(root_, "assets/character/meshes/mesh-0000").data.size(), 1u);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(root_, "assets/character/animations/clip-0000").data.size(), 1u);
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(root_, "assets/character/textures/texture-0000-bc7-srgb").data.size(), 4u);

    ASSERT_TRUE(commands.undo().execution.result);
    EXPECT_EQ(nori::resource::try_get(root_, "assets/character").error(), nori::resource::error_code::not_found);

    ASSERT_TRUE(commands.redo().execution.result);
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(root_, "assets/character/meshes/mesh-0000").data.size(), 1u);
}

TEST_F(command_test, model_import_conflict)
{
    const nori::resource::handle destination{create_value("assets", std::monostate{})};
    const nori::resource::handle existing{create_value("assets/character", std::string{"preserved"})};
    nori::asset::cooked_model model{
        .model = nori::resource::binary{.data = {std::byte{1}}}
    };
    command_manager commands;

    const command_result imported{commands.execute(std::make_unique<import_model_command>(root_, destination, "character", std::move(model)))};
    EXPECT_FALSE(imported.execution.result);
    EXPECT_EQ(imported.execution.result.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::get<std::string>(existing), "preserved");
    EXPECT_FALSE(commands.can_undo());
}

TEST_F(command_test, blob_history)
{
    const nori::resource::image before_image{
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}
    };
    const nori::resource::image after_image{
        .width = 1,
        .height = 1,
        .mip_levels = 1,
        .data = {std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}}
    };
    const nori::resource::handle image_handle{create_value("image", before_image)};
    const nori::resource::handle binary_handle{create_value(
        "binary",
        nori::resource::binary{.data = {std::byte{9}, std::byte{10}}}
    )};
    command_manager image_commands;
    command_manager binary_commands;

    ASSERT_TRUE(image_commands.execute(std::make_unique<set_value_command<nori::resource::image>>(root_, std::vector{image_handle}, after_image)).execution.result);
    ASSERT_TRUE(binary_commands.execute(std::make_unique<set_value_command<nori::resource::binary>>(root_, std::vector{binary_handle}, nori::resource::binary{.data = {std::byte{11}, std::byte{12}}})).execution.result);

    ASSERT_TRUE(image_commands.undo().execution.result);
    const auto restored_image{nori::resource::try_get<nori::resource::image_view>(root_, "image")};
    ASSERT_TRUE(restored_image);
    EXPECT_TRUE(std::ranges::equal(restored_image->data, before_image.data));

    ASSERT_TRUE(binary_commands.undo().execution.result);
    const auto restored_binary{nori::resource::try_get<nori::resource::binary_view>(root_, "binary")};
    ASSERT_TRUE(restored_binary);
    EXPECT_TRUE(std::ranges::equal(restored_binary->data, std::array{std::byte{9}, std::byte{10}}));
}

TEST_F(command_test, document_state)
{
    const nori::resource::handle parent{create_value("parent", std::monostate{})};
    const nori::resource::handle child{create_integer("parent/child", 7)};
    temporary_storage temporary;
    document_editor target{directory_ / "document.nori", root_};
    target.selection().select(child);
    document_workspace workspace{root_};
    const auto subscription{target.changed().add([&workspace](const command_effect& effect)
    {
        workspace.apply(effect);
    })};
    workspace.hierarchy().open_nodes.insert(parent);
    workspace.preview().source = child;

    const auto removed{target.execute(std::make_unique<remove_command>(&temporary, root_, std::vector{parent}))};
    ASSERT_TRUE(removed);
    EXPECT_TRUE(target.selection().selection().empty());
    EXPECT_FALSE(workspace.hierarchy().open_nodes.contains(parent));
    EXPECT_TRUE(workspace.preview().refresh);
    preview view;
    ASSERT_TRUE(view.update(target, workspace));
    EXPECT_FALSE(workspace.preview().source);
    EXPECT_FALSE(workspace.preview().refresh);
}

TEST_F(command_test, closing_document)
{
    std::filesystem::current_path(directory_);
    temporary_storage temporary;
    clipboard clip{&temporary};
    document_manager manager;
    document_editor* const closing_document{*manager.create()};
    manager.begin_close(*closing_document);
    const auto rejected{closing_document->execute(std::make_unique<set_value_command<std::int32_t>>(closing_document->root(), std::vector<nori::resource::handle>{}, 1))};
    EXPECT_EQ(rejected.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(manager.save(*closing_document).error(), nori::resource::error_code::invalid_relationship);

    document_editor* const undo_document{*manager.create()};
    const nori::resource::handle undo_root{undo_document->root()};
    const auto undo_value{create_relative(undo_root, "value")};
    ASSERT_TRUE(undo_value);
    nori::resource::set(*undo_value, std::int32_t{1});
    ASSERT_TRUE(undo_document->execute(std::make_unique<set_value_command<std::int32_t>>(undo_root, std::vector{nori::resource::try_get(undo_root, "value").value()}, 2)));

    document_editor* const redo_document_created{*manager.create()};
    const nori::resource::handle redo_root{redo_document_created->root()};
    document_editor* const redo_document{manager.find_root(redo_root)};
    const auto redo_value{create_relative(redo_root, "value")};
    ASSERT_TRUE(redo_value);
    nori::resource::set(*redo_value, std::int32_t{1});
    ASSERT_TRUE(redo_document->execute(std::make_unique<set_value_command<std::int32_t>>(redo_root, std::vector{nori::resource::try_get(redo_root, "value").value()}, 2)));
    ASSERT_TRUE(redo_document->undo());

    const nori::resource::handle source_root{nori::resource::create_root()};
    const nori::resource::handle source_value{*create_relative(source_root, "value")};
    nori::resource::set(source_value, std::int32_t{3});
    document_editor source{directory_ / "source.nori", source_root};
    source.selection().select(source_value);
    ASSERT_TRUE(clip.copy(source));

    manager.begin_close(*manager.find_root(undo_root));
    manager.begin_close(*manager.find_root(redo_root));
    EXPECT_EQ(manager.find_root(undo_root)->undo().error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(manager.find_root(redo_root)->redo().error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(
        clip.paste(*manager.find_root(undo_root), undo_root).error(),
        nori::resource::error_code::invalid_relationship
    );
    manager.close_all();
}

TEST_F(command_test, stale_frame_request)
{
    const nori::resource::handle original{create_integer("target", 1)};
    const resource_request_target request{root_, original, "target"};
    nori::resource::remove(original);
    const nori::resource::handle replacement{create_integer("target", 2)};

    const auto resolved{resolve_property(request.document_root, request.path)};
    ASSERT_TRUE(resolved);
    EXPECT_EQ(*resolved, replacement);
    EXPECT_NE(*resolved, request.expected);
}

TEST_F(command_test, clipboard_lifetime)
{
    temporary_storage temporary;
    clipboard clip{&temporary};
    document_manager manager;
    document_editor* const source{*manager.create()};
    const nori::resource::handle source_root{source->root()};
    const nori::resource::handle source_value{*create_relative(source_root, "value")};
    nori::resource::set(source_value, std::int32_t{9});
    source->selection().select(source_value);
    ASSERT_TRUE(clip.copy(*source));
    manager.begin_close(*source);
    manager.close(*source);

    document_editor* const destination{*manager.create()};
    const nori::resource::handle destination_root{destination->root()};
    ASSERT_TRUE(clip.paste(*destination, destination_root));
    clip.clear();
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(destination_root, "value")), 9);
    ASSERT_TRUE(destination->undo());
    EXPECT_EQ(nori::resource::try_get(destination_root, "value").error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(destination->redo());
    EXPECT_EQ(nori::resource::get<std::int32_t>(*nori::resource::try_get(destination_root, "value")), 9);
    manager.close_all();
}

TEST_F(command_test, clipboard_destinations)
{
    temporary_storage temporary;
    clipboard clip{&temporary};
    const nori::resource::handle parent{create_value("parent", std::monostate{})};
    const nori::resource::handle child{create_integer("parent/child", 1)};
    const nori::resource::handle sibling{create_integer("sibling", 2)};
    const nori::resource::handle folder{create_value("folder", std::monostate{})};
    document_editor source{directory_ / "document.nori", root_};
    source.selection().select_all(std::vector<nori::resource::handle>{child, parent, sibling});

    ASSERT_TRUE(clip.copy(source));
    const auto pasted_to_folder{clip.paste(source, folder)};
    ASSERT_TRUE(pasted_to_folder);
    ASSERT_EQ(pasted_to_folder->created.size(), 2);
    EXPECT_TRUE(nori::resource::try_get(folder, "parent/child"));
    EXPECT_TRUE(nori::resource::try_get(folder, "sibling"));

    ASSERT_TRUE(source.undo());
    source.selection().select_all(std::vector<nori::resource::handle>{parent, child});
    const auto cut{clip.cut(source)};
    ASSERT_TRUE(cut);
    EXPECT_EQ(nori::resource::try_get(root_, "parent").error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(source.undo());
    EXPECT_TRUE(nori::resource::try_get(root_, "parent/child"));

    clip.clear();
    const auto empty_paste{clip.paste(source, root_)};
    ASSERT_TRUE(empty_paste);
    EXPECT_TRUE(empty_paste->created.empty());
    EXPECT_TRUE(empty_paste->updated.empty());
    EXPECT_TRUE(empty_paste->invalidated.empty());
}

TEST_F(command_test, independent_documents)
{
    const nori::resource::handle other_root{nori::resource::create_root()};
    const nori::resource::handle first_value{create_integer("value", 1)};
    const nori::resource::handle second_value{*create_relative(other_root, "value")};
    nori::resource::set(second_value, std::int32_t{10});
    document_editor first{directory_ / "document.nori", root_};
    document_editor second{directory_ / "other.nori", other_root};
    first.selection().select(first_value);
    second.selection().select(second_value);
    document_workspace first_workspace{root_};
    document_workspace second_workspace{other_root};
    first_workspace.hierarchy().filter = "first";
    second_workspace.hierarchy().filter = "second";

    ASSERT_TRUE(first.execute(std::make_unique<set_value_command<std::int32_t>>(root_, std::vector{property("value")}, 2)));
    ASSERT_TRUE(second.execute(std::make_unique<set_value_command<std::int32_t>>(other_root, std::vector{nori::resource::try_get(other_root, "value").value()}, 20)));
    ASSERT_TRUE(first.undo());

    EXPECT_EQ(nori::resource::get<std::int32_t>(first_value), 1);
    EXPECT_EQ(nori::resource::get<std::int32_t>(second_value), 20);
    EXPECT_EQ(first.selection().selected(), first_value);
    EXPECT_EQ(second.selection().selected(), second_value);
    EXPECT_EQ(first_workspace.hierarchy().filter, "first");
    EXPECT_EQ(second_workspace.hierarchy().filter, "second");
    EXPECT_FALSE(first.can_undo());
    EXPECT_TRUE(second.can_undo());
}

TEST_F(command_test, document_reuse)
{
    const std::filesystem::path file{directory_ / "opened.nori"};
    const std::string key{nori::core::path_to_utf8(file)};
    const nori::resource::handle created_root{nori::resource::create_root()};
    ASSERT_TRUE(created_root);
    const nori::resource::handle saved_root{created_root};
    ASSERT_TRUE(nori::resource::save_as(saved_root, file));
    nori::resource::remove(saved_root);

    document_manager manager;
    const auto opened_result{manager.open(file)};
    ASSERT_TRUE(opened_result) << nori::resource::to_string(opened_result.error());
    document_editor* const opened{*opened_result};
    EXPECT_FALSE(opened->dirty());
    EXPECT_EQ(*manager.open(file), opened);
    EXPECT_EQ(manager.documents().size(), 1);
    const document_manager& read_only{manager};
    EXPECT_EQ(read_only.find_root(opened->root()), opened);
    EXPECT_EQ(read_only.find(opened->root()), opened);
    EXPECT_EQ(read_only.find_root(root_), nullptr);
    EXPECT_EQ(read_only.find(root_), nullptr);
    EXPECT_EQ(read_only.active(), opened);
    EXPECT_EQ(read_only.documents().front().root(), opened->root());
    manager.close_all();
}

TEST_F(command_test, save_as_rollback)
{
    const std::filesystem::path file{directory_ / "opened.nori"};
    const std::string key{nori::core::path_to_utf8(file)};
    const nori::resource::handle created_root{nori::resource::create_root()};
    ASSERT_TRUE(created_root);
    const nori::resource::handle saved_root{created_root};
    const auto saved_value{create_relative(saved_root, "value")};
    ASSERT_TRUE(saved_value);
    nori::resource::set(*saved_value, std::int32_t{1});
    ASSERT_TRUE(nori::resource::save_as(saved_root, file));
    nori::resource::remove(saved_root);

    document_manager manager;
    const auto opened_result{manager.open(file)};
    ASSERT_TRUE(opened_result) << nori::resource::to_string(opened_result.error());
    document_editor* const opened{*opened_result};
    const nori::resource::handle opened_root{opened->root()};
    ASSERT_TRUE(opened->execute(std::make_unique<set_value_command<std::int32_t>>(opened_root, std::vector{nori::resource::try_get(opened_root, "value").value()}, 2)));
    EXPECT_TRUE(opened->dirty());

    const auto failed{manager.save_as(*opened, directory_ / "missing" / "failed.nori")};
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(opened->root(), opened_root);
    EXPECT_EQ(opened->path(), test_normalized_path(file));
    EXPECT_EQ(*nori::resource::try_get(key), opened_root);
    EXPECT_TRUE(opened->dirty());
    manager.close_all();
}

TEST_F(command_test, save_as_handles)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    document_editor* const target{*manager.create()};
    const std::filesystem::path old_path{target->path()};
    const nori::resource::handle root{target->root()};
    const auto value{create_relative(root, "value")};
    ASSERT_TRUE(value);
    nori::resource::set(*value, std::int32_t{7});
    const std::filesystem::path new_path{directory_ / "renamed.nori"};

    const auto saved{manager.save_as(*target, new_path)};
    ASSERT_TRUE(saved);
    EXPECT_EQ(*saved, save_as_status::saved);
    EXPECT_EQ(target->path(), test_normalized_path(new_path));
    EXPECT_EQ(target->root(), root);
    EXPECT_EQ(*nori::resource::try_get(nori::core::path_to_utf8(new_path)), root);
    EXPECT_EQ(*nori::resource::try_get(root, "value"), *value);
    EXPECT_EQ(
        nori::resource::try_get(nori::core::path_to_utf8(old_path)).error(),
        nori::resource::error_code::not_found
    );
    EXPECT_FALSE(target->dirty());
    manager.close_all();
}

TEST_F(command_test, save_as_confirmation)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    document_editor* const target{*manager.create()};
    const std::filesystem::path old_path{target->path()};
    const nori::resource::handle root{target->root()};
    const auto value{create_relative(root, "value")};
    ASSERT_TRUE(value);
    nori::resource::set(*value, std::int32_t{7});

    const std::filesystem::path destination{directory_ / "existing.nori"};
    const std::string destination_key{nori::core::path_to_utf8(destination)};
    const nori::resource::handle existing_root{nori::resource::create_root()};
    ASSERT_TRUE(existing_root);
    const auto marker{create_relative(existing_root, "marker")};
    ASSERT_TRUE(marker);
    nori::resource::set(*marker, std::int32_t{99});
    ASSERT_TRUE(nori::resource::save_as(existing_root, destination));
    nori::resource::remove(existing_root);

    const auto confirmation{manager.save_as(*target, destination)};
    ASSERT_TRUE(confirmation);
    EXPECT_EQ(*confirmation, save_as_status::overwrite_required);
    EXPECT_EQ(target->path(), old_path);
    EXPECT_EQ(target->root(), root);
    EXPECT_EQ(
        nori::resource::try_get(nori::core::path_to_utf8(old_path)).error(),
        nori::resource::error_code::not_found
    );
    EXPECT_TRUE(nori::resource::path_of(root).empty());
    const auto existing_marker{nori::resource::try_get<std::int32_t>(std::format("{}::marker", destination_key))};
    ASSERT_TRUE(existing_marker);
    EXPECT_EQ(*existing_marker, 99);
    nori::resource::remove(*nori::resource::try_get(destination_key));

    const auto overwritten{manager.save_as(*target, destination, save_as_policy::overwrite)};
    ASSERT_TRUE(overwritten);
    EXPECT_EQ(*overwritten, save_as_status::saved);
    EXPECT_EQ(target->path(), test_normalized_path(destination));
    EXPECT_EQ(target->root(), root);
    EXPECT_EQ(*nori::resource::try_get(root, "value"), *value);
    EXPECT_EQ(nori::resource::try_get(root, "marker").error(), nori::resource::error_code::not_found);
    EXPECT_FALSE(target->dirty());
    manager.close_all();
}

TEST_F(command_test, save_as_same_path)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    document_editor* const target{*manager.create()};

    const auto saved{manager.save_as(*target, target->path())};
    ASSERT_TRUE(saved);
    EXPECT_EQ(*saved, save_as_status::saved);
    manager.close_all();
}

TEST_F(command_test, save_as_conflict)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    document_editor* const first{*manager.create()};
    const nori::resource::handle first_root{first->root()};
    document_editor* const second{*manager.create()};
    const nori::resource::handle second_root{second->root()};

    const auto conflict{manager.save_as(
        *manager.find_root(first_root), manager.find_root(second_root)->path(), save_as_policy::overwrite
    )};
    ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(manager.find_root(first_root)->root(), first_root);
    EXPECT_EQ(manager.find_root(second_root)->root(), second_root);
    manager.close_all();
}

TEST_F(command_test, create_conflict)
{
    std::filesystem::current_path(directory_);
    const std::filesystem::path physical{directory_ / "untitled1.nori"};
    const nori::resource::handle created_root{nori::resource::create_root()};
    ASSERT_TRUE(created_root);
    const nori::resource::handle physical_root{created_root};
    ASSERT_TRUE(nori::resource::save_as(physical_root, physical));
    nori::resource::remove(physical_root);
    document_manager manager;
    document_editor* const first{*manager.create()};
    EXPECT_EQ(first->path().filename(), "untitled2.nori");
    document_editor* const second{*manager.create()};
    EXPECT_EQ(second->path().filename(), "untitled3.nori");
    document_editor* const third{*manager.create()};
    EXPECT_EQ(third->path().filename(), "untitled4.nori");
    manager.close_all();
}

TEST_F(command_test, close_selection)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    document_editor* const first{*manager.create()};
    const nori::resource::handle first_root{first->root()};
    document_editor* const second{*manager.create()};
    const nori::resource::handle second_root{second->root()};
    const auto third{manager.create()};
    ASSERT_TRUE(third);

    manager.set_active(second_root);
    manager.begin_close(*manager.find_root(second_root));
    manager.close(*manager.find_root(second_root));
    ASSERT_NE(manager.active(), nullptr);
    EXPECT_EQ(manager.active()->root(), first_root);
    manager.close_all();
}

TEST_F(command_test, save_all_failure)
{
    const std::filesystem::path good_directory{directory_ / "good"};
    const std::filesystem::path bad_directory{directory_ / "bad"};
    std::filesystem::create_directories(good_directory);
    std::filesystem::create_directories(bad_directory);

    const auto create_file = [](const std::filesystem::path& path)
    {
        const std::string key{nori::core::path_to_utf8(path)};
        const nori::resource::handle root{nori::resource::create_root()};
        EXPECT_TRUE(root);
        const auto value{create_relative(root, "value")};
        EXPECT_TRUE(value);
        if (!root || !value)
        {
            return;
        }
        nori::resource::set(*value, std::int32_t{1});
        EXPECT_TRUE(nori::resource::save_as(root, path));
        nori::resource::remove(root);
    };
    const std::filesystem::path good_path{good_directory / "good.nori"};
    const std::filesystem::path bad_path{bad_directory / "bad.nori"};
    create_file(good_path);
    create_file(bad_path);

    document_manager manager;
    document_editor* const good{*manager.open(good_path)};
    const nori::resource::handle good_root{good->root()};
    document_editor* const bad{*manager.open(bad_path)};
    const nori::resource::handle bad_root{bad->root()};
    ASSERT_TRUE(manager.find_root(good_root)->execute(std::make_unique<set_value_command<std::int32_t>>(good_root, std::vector{nori::resource::try_get(good_root, "value").value()}, 2)));
    ASSERT_TRUE(manager.find_root(bad_root)->execute(std::make_unique<set_value_command<std::int32_t>>(bad_root, std::vector{nori::resource::try_get(bad_root, "value").value()}, 2)));
    std::error_code ec;
    std::filesystem::remove_all(bad_directory, ec);
    ASSERT_FALSE(ec);

    const auto saved{manager.save_all()};
    ASSERT_FALSE(saved);
    EXPECT_EQ(saved.error(), nori::resource::error_code::io_failure);
    EXPECT_FALSE(manager.find_root(good_root)->dirty());
    EXPECT_TRUE(manager.find_root(bad_root)->dirty());
    manager.close_all();
}

TEST_F(command_test, exit_cancellation)
{
    std::filesystem::current_path(directory_);
    document_manager manager;
    const nori::resource::handle first_root{(*manager.create())->root()};
    const nori::resource::handle second_root{(*manager.create())->root()};
    const nori::resource::handle third_root{(*manager.create())->root()};

    const auto make_dirty = [&manager](nori::resource::handle root)
    {
        const auto value{create_relative(root, "value")};
        ASSERT_TRUE(value);
        nori::resource::set(*value, std::int32_t{1});
        ASSERT_TRUE(manager.find_root(root)->execute(std::make_unique<set_value_command<std::int32_t>>(root, std::vector{nori::resource::try_get(root, "value").value()}, 2)));
    };
    make_dirty(first_root);
    make_dirty(second_root);

    exit_workflow workflow;
    workflow.request();
    EXPECT_EQ(workflow.next_confirmation(manager.documents()), first_root);

    const auto failed_save{manager.save_as(
        *manager.find_root(first_root), directory_ / "missing" / "failed.nori"
    )};
    ASSERT_FALSE(failed_save);
    EXPECT_EQ(workflow.next_confirmation(manager.documents()), first_root);
    EXPECT_FALSE(manager.find_root(first_root)->closing());
    EXPECT_FALSE(manager.find_root(second_root)->closing());
    EXPECT_FALSE(manager.find_root(third_root)->closing());

    workflow.approve(first_root);
    EXPECT_EQ(workflow.next_confirmation(manager.documents()), second_root);
    workflow.cancel();
    EXPECT_FALSE(workflow.requested());
    EXPECT_FALSE(workflow.closing());
    EXPECT_FALSE(workflow.next_confirmation(manager.documents()));
    EXPECT_FALSE(manager.find_root(first_root)->closing());

    workflow.request();
    EXPECT_EQ(workflow.next_confirmation(manager.documents()), first_root);
    workflow.approve(first_root);
    workflow.approve(second_root);
    EXPECT_EQ(workflow.next_confirmation(manager.documents()), third_root);
    workflow.approve(third_root);
    EXPECT_FALSE(workflow.next_confirmation(manager.documents()));
    workflow.begin_closing();
    for (document_editor& target : manager.documents())
    {
        manager.begin_close(target);
    }
    EXPECT_TRUE(workflow.closing());
    EXPECT_TRUE(manager.find_root(first_root)->closing());
    EXPECT_TRUE(manager.find_root(second_root)->closing());
    EXPECT_TRUE(manager.find_root(third_root)->closing());
    manager.close_all();
}

TEST_F(command_test, log_limit)
{
    for (std::size_t index{0}; index < 257; ++index)
    {
        log::info(std::format("message {}", index));
    }
    ASSERT_EQ(log::entries().size(), 256);
    EXPECT_EQ(log::entries().front().text, "message 1");
    EXPECT_EQ(log::entries().back().text, "message 256");
}
} // namespace
