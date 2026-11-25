#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <nori/graphics/error_code.hpp>
#include <nori/graphics/operations.hpp>
#include <nori/resource.hpp>

#include "command.hpp"
#include "document_editor.hpp"
#include "document_workspace.hpp"
#include "exit_workflow.hpp"
#include "log.hpp"
#include "noritor_test_helpers.hpp"
#include "preview.hpp"

using namespace nori::editor;

namespace
{
class preview_test : public noritor_test_fixture
{
protected:
    void SetUp() override
    {
        noritor_test_fixture::SetUp();
        if (HasFatalFailure())
        {
            return;
        }
        if (!nori::graphics::initialize())
        {
            GTEST_SKIP() << "graphics device unavailable, skipping GPU tests";
        }
    }

    void TearDown() override
    {
        nori::graphics::shutdown();
        noritor_test_fixture::TearDown();
    }
};

TEST_F(preview_test, preview_retry)
{
    nori::resource::image image{
        .width = 2,
        .height = 1,
        .mip_levels = 1,
        .data = std::vector<std::byte>(4)
    };
    const nori::resource::handle source{create_value("image", image)};
    document_editor target{directory_ / "document.nori", root_};
    target.selection().select(source);
    document_workspace workspace{root_};
    preview view;

    const auto first{view.update(target, workspace)};
    ASSERT_FALSE(first);
    EXPECT_EQ(first.error(), nori::graphics::error_code::invalid_argument);
    EXPECT_EQ(workspace.preview().source, source);
    EXPECT_FALSE(workspace.preview().texture);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 2.0f);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.y, 1.0f);
    EXPECT_TRUE(workspace.preview().refresh);

    workspace.preview().zoom = 2.0f;
    workspace.preview().reset_scroll = false;
    ASSERT_FALSE(view.update(target, workspace));
    EXPECT_FLOAT_EQ(workspace.preview().zoom, 2.0f);
    EXPECT_FALSE(workspace.preview().reset_scroll);
    EXPECT_TRUE(workspace.preview().refresh);

    image.data.resize(8);
    nori::resource::set(source, image);
    ASSERT_TRUE(view.update(target, workspace));
    EXPECT_TRUE(workspace.preview().texture);
    EXPECT_FALSE(workspace.preview().refresh);
    EXPECT_FLOAT_EQ(workspace.preview().zoom, 2.0f);

    view.release(workspace);
    EXPECT_FALSE(workspace.preview().source);
    EXPECT_FALSE(workspace.preview().texture);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 0.0f);
    EXPECT_FLOAT_EQ(workspace.preview().zoom, 1.0f);
    EXPECT_TRUE(workspace.preview().reset_scroll);
    EXPECT_FALSE(workspace.preview().refresh);
}

TEST_F(preview_test, preview_maintenance)
{
    nori::resource::image first_image{
        .width = 2,
        .height = 1,
        .mip_levels = 1,
        .data = std::vector<std::byte>(8)
    };
    nori::resource::image second_image{
        .width = 4,
        .height = 2,
        .mip_levels = 1,
        .data = std::vector<std::byte>(32)
    };
    const nori::resource::handle first{create_value("first", first_image)};
    const nori::resource::handle second{create_value("second", second_image)};
    document_editor target{directory_ / "document.nori", root_};
    document_workspace workspace{root_};
    preview view;

    target.selection().select(first);
    ASSERT_TRUE(view.update(target, workspace));
    EXPECT_EQ(workspace.preview().source, first);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 2.0f);

    first_image.width = 3;
    first_image.data.resize(12);
    const auto effect{target.execute(std::make_unique<set_value_command<nori::resource::image>>(root_, std::vector{property("first")}, first_image))};
    ASSERT_TRUE(effect);
    workspace.apply(*effect);
    EXPECT_TRUE(workspace.preview().refresh);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 2.0f);
    ASSERT_TRUE(view.update(target, workspace));
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 3.0f);

    target.selection().select(second);
    EXPECT_EQ(workspace.preview().source, first);
    ASSERT_TRUE(view.update(target, workspace));
    EXPECT_EQ(workspace.preview().source, second);
    EXPECT_FLOAT_EQ(workspace.preview().image_size.x, 4.0f);

    document_manager manager;
    document_editor* const managed{*manager.create()};
    document_workspace managed_workspace{managed->root()};
    managed_workspace.preview().source = second;
    manager.begin_close(*managed);
    EXPECT_EQ(managed_workspace.preview().source, second);
    view.release(managed_workspace);
    EXPECT_FALSE(managed_workspace.preview().source);
    manager.close(*managed);
    view.release(workspace);
}
} // namespace
