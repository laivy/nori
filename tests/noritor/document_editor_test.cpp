#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "context.hpp"
#include "document_editor.hpp"
#include "noritor_test_helpers.hpp"
#include "temporary_storage.hpp"

using namespace nori::editor;

namespace
{
class document_editor_test : public noritor_test_fixture
{
};

class failing_command final : public command
{
public:
    explicit failing_command(nori::resource::handle invalidated) :
        invalidated_{invalidated}
    {
    }

    command_execution execute() override
    {
        return {{.invalidated = {invalidated_}}, std::unexpected{nori::resource::error_code::name_conflict}, false};
    }

    command_execution undo() override
    {
        return {};
    }

    std::string description() const override
    {
        return "failure";
    }

private:
    nori::resource::handle invalidated_;
};

TEST_F(document_editor_test, created_selection_is_ready_before_notification_and_undo_clears_it)
{
    document_editor editor{directory_ / "test.nori", root_};
    bool saw_created{};
    const auto subscription{editor.changed().add([&](const command_effect& effect)
    {
        if (!effect.created.empty())
        {
            saw_created = true;
            EXPECT_EQ(editor.selection().selected(), effect.created.front());
        }
        if (!effect.invalidated.empty())
        {
            EXPECT_TRUE(editor.selection().selection().empty());
        }
    })};
    ASSERT_TRUE(editor.create(root_, "value", std::int32_t{42}));
    EXPECT_TRUE(saw_created);
    ASSERT_TRUE(editor.undo());
    EXPECT_TRUE(editor.selection().selection().empty());
}

TEST_F(document_editor_test, failed_command_notifies_after_removing_invalidated_selection)
{
    const auto value{create_integer("value", 1)};
    document_editor editor{directory_ / "test.nori", root_};
    document_workspace workspace{root_};
    workspace.hierarchy().open_nodes.insert(value);
    editor.selection().select(value);
    bool notified{};
    const auto subscription{editor.changed().add([&](const command_effect& effect)
    {
        notified = true;
        workspace.apply(effect);
        EXPECT_EQ(effect.invalidated, std::vector{value});
        EXPECT_TRUE(editor.selection().selection().empty());
    })};
    const auto result{editor.execute(std::make_unique<failing_command>(value))};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::resource::error_code::name_conflict);
    EXPECT_TRUE(notified);
    EXPECT_FALSE(workspace.hierarchy().open_nodes.contains(value));
}

TEST_F(document_editor_test, command_results_are_logged)
{
    const auto value{create_integer("value", 1)};
    document_editor editor{directory_ / "test.nori", root_};

    ASSERT_TRUE(editor.set_name(value, "renamed"));
    ASSERT_EQ(log::entries().size(), 1);
    EXPECT_EQ(log::entries().back().severity, log::level::info);
    EXPECT_NE(log::entries().back().text.find("[test.nori] 이름 변경"), std::string::npos);

    ASSERT_FALSE(editor.execute(std::make_unique<failing_command>(value)));
    ASSERT_EQ(log::entries().size(), 2);
    EXPECT_EQ(log::entries().back().severity, log::level::error);
    EXPECT_EQ(log::entries().back().text, "[test.nori] 작업 실패 (failure): resource name conflict");
}

TEST_F(document_editor_test, undo_preserves_selection_of_an_unaffected_node)
{
    temporary_storage temporary;
    const auto first{create_integer("first", 1)};
    const auto second{create_integer("second", 2)};
    document_editor editor{directory_ / "test.nori", root_};
    editor.selection().select(first);
    ASSERT_TRUE(editor.remove(temporary));
    editor.selection().select(second);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(editor.selection().selected(), second);
    EXPECT_TRUE(nori::resource::try_get(root_, "first"));
}

TEST_F(document_editor_test, workspace_subscription_survives_editor_relocation_and_removal)
{
    std::filesystem::current_path(directory_);
    context ctx;
    const auto first_root{(*ctx.documents().create())->root()};
    auto& first_workspace{ctx.workspace(first_root)};
    const nori::resource::handle child{nori::resource::create(first_root, "child", nori::resource::empty)};
    ASSERT_TRUE(child);
    first_workspace.hierarchy().open_nodes.insert(child);
    const auto second_root{(*ctx.documents().create())->root()};
    ctx.workspace(second_root);
    const auto first{ctx.documents().find_root(first_root)};
    first->selection().select(child);
    ASSERT_TRUE(first->remove(ctx.temporary()));
    EXPECT_FALSE(first_workspace.hierarchy().open_nodes.contains(child));
    ctx.documents().begin_close(*first);
    ctx.remove_workspace(first_root);
    ctx.documents().close(*first);
    const auto second{ctx.documents().find_root(second_root)};
    const nori::resource::handle second_child{nori::resource::create(second_root, "second", nori::resource::empty)};
    ASSERT_TRUE(second_child);
    ctx.workspace(second_root).hierarchy().open_nodes.insert(second_child);
    second->selection().select(second_child);
    ASSERT_TRUE(second->remove(ctx.temporary()));
    EXPECT_FALSE(ctx.workspace(second_root).hierarchy().open_nodes.contains(second_child));
    ctx.documents().begin_close(*second);
    ctx.remove_workspace(second_root);
    ctx.documents().close(*second);
}
} // namespace
