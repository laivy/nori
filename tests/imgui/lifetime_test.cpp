#include "imgui_test_helpers.hpp"

namespace nori::graphics::imgui
{
TEST_F(lifetime_test, shutdown_with_recorded_ui)
{
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddText({8, 8}, IM_COL32_WHITE, "Recorded UI");
    ASSERT_TRUE(finish_ui());
    shutdown();
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    graphics::shutdown();
}

TEST_F(lifetime_test, repeated_initialization_releases_font_textures)
{
    std::optional<ImTextureID> descriptor;
    for (int index{0}; index < 2; ++index)
    {
        SCOPED_TRACE(index);
        ASSERT_TRUE(start_frame());
        ASSERT_TRUE(begin_ui());
        ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddText({8, 8}, IM_COL32_WHITE, "Descriptor recycling");
        ASSERT_TRUE(finish_ui());
        const auto current{ImGui::GetIO().Fonts->TexData->TexID};
        ASSERT_NE(current, ImTextureID_Invalid);
        if (descriptor)
        {
            EXPECT_EQ(current, *descriptor);
        }
        descriptor = current;
        ASSERT_TRUE(finish_frame());
        shutdown();
        ASSERT_TRUE(initialize(window_));
        ImGui::GetIO().IniFilename = nullptr;
    }
}

TEST_F(lifetime_test, shutdown_detaches_callbacks)
{
    shutdown();
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(finish_frame());
    ASSERT_TRUE(initialize(window_));
    ImGui::GetIO().IniFilename = nullptr;
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
}
} // namespace nori::graphics::imgui
