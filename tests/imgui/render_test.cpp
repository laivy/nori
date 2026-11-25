#include <chrono>
#include <optional>
#include <thread>
#include <utility>

#include "imgui_test_helpers.hpp"

namespace nori::graphics::imgui
{
TEST_F(render_test, main_draw_callback)
{
    const auto& io{ImGui::GetIO()};
    EXPECT_STREQ(io.BackendRendererName, "imgui_impl_dx12");
    EXPECT_STREQ(io.BackendPlatformName, "imgui_impl_win32");
    EXPECT_NE(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable, 0);
    EXPECT_NE(io.BackendFlags & ImGuiBackendFlags_RendererHasViewports, 0);
    EXPECT_NE(io.BackendFlags & ImGuiBackendFlags_RendererHasTextures, 0);

    struct frame_probe
    {
        bool called;
    };

    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    frame_probe probe{false};
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddRectFilled({8, 8}, {32, 32}, IM_COL32(255, 0, 0, 255));
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddCallback([](const ImDrawList*, const ImDrawCmd* command)
    {
        auto& data{*static_cast<frame_probe*>(command->UserCallbackData)};
        data.called = true;
    }, &probe);
    EXPECT_FALSE(probe.called);
    ASSERT_TRUE(finish_ui());
    EXPECT_TRUE(probe.called);
    ASSERT_TRUE(finish_frame());
}

TEST_F(render_test, render_settings_selected_at_pass_begin)
{
    ASSERT_TRUE(start_frame());
    const auto alternate{graphics::create_texture({texture_format::rgba8_unorm, 512, 512, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(alternate);
    const render_pass_desc desc{.render_targets = {{*alternate, {begin_op::clear, {.color = {0, 1, 0, 1}}}, {end_op::store}}}};
    ASSERT_TRUE(imgui::begin_render_pass(desc));
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddRectFilled({8, 8}, {32, 32}, IM_COL32(255, 0, 0, 255));
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    const auto inside_x{static_cast<std::uint32_t>(20.0f * 512 / ImGui::GetDrawData()->DisplaySize.x)};
    const auto inside_y{static_cast<std::uint32_t>(20.0f * 512 / ImGui::GetDrawData()->DisplaySize.y)};
    const auto ticket{graphics::readback(*alternate, {0, 0, 512, 512, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(ticket);
    const auto pixels{await_readback(*ticket)};
    ASSERT_TRUE(pixels);
    const auto channel = [&pixels](std::size_t x, std::size_t y, std::size_t component)
    {
        return std::to_integer<unsigned>(pixels->data[y * pixels->row_pitch + x * 4 + component]);
    };
    EXPECT_EQ(channel(inside_x, inside_y, 0), 255u);
    EXPECT_EQ(channel(inside_x, inside_y, 1), 0u);
    EXPECT_EQ(channel(500, 500, 0), 0u);
    EXPECT_EQ(channel(500, 500, 1), 255u);
    graphics::destroy(*ticket);
    graphics::destroy(*alternate);
}

TEST_F(render_test, back_buffer_uses_resized_texture_dimensions)
{
    const auto chain{graphics::create_swap_chain({window_, false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    const std::array<std::array<std::uint32_t, 2>, 3> sizes{{{256, 256}, {128, 96}, {0, 0}}};
    for (const auto& size : sizes)
    {
        ASSERT_TRUE(graphics::resize(*chain, size[0], size[1]));
        ASSERT_TRUE(start_frame());
        const auto screen{graphics::back_buffer(*chain)};
        ASSERT_TRUE(imgui::begin_render_pass({.render_targets = {{screen, {begin_op::clear, {.color = {0, 1, 0, 1}}}, {end_op::store}}}}));
        const std::uint32_t width{size[0] == 0 ? 256u : size[0]};
        const std::uint32_t height{size[1] == 0 ? 256u : size[1]};
        ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddRectFilled({8, 8}, {32, 32}, IM_COL32(255, 0, 0, 255));
        ASSERT_TRUE(finish_ui());
        const auto ticket{graphics::readback(screen, {0, 0, width, height}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(ticket);
        ASSERT_TRUE(finish_frame());
        ASSERT_TRUE(graphics::present(*chain));
        const auto pixels{await_readback(*ticket)};
        ASSERT_TRUE(pixels);
        EXPECT_EQ(pixels->width, width);
        EXPECT_EQ(pixels->height, height);
        const auto channel = [&pixels](std::size_t x, std::size_t y, std::size_t component)
        {
            return std::to_integer<unsigned>(pixels->data[y * pixels->row_pitch + x * 4 + component]);
        };
        EXPECT_EQ(channel(20u * width / 256u, 20u * height / 256u, 0), 255u);
        EXPECT_EQ(channel(20u * width / 256u, 20u * height / 256u, 1), 0u);
        EXPECT_EQ(channel(width - 1, height - 1, 0), 0u);
        EXPECT_EQ(channel(width - 1, height - 1, 1), 255u);
        graphics::destroy(*ticket);
    }
    graphics::destroy(*chain);
}

TEST_F(render_test, scene_pass_before_ui)
{
    ASSERT_TRUE(start_frame());
    const auto first_scene_pass{graphics::begin_render_pass(ui_pass())};
    ASSERT_TRUE(first_scene_pass);
    ASSERT_TRUE(graphics::end_render_pass(first_scene_pass));
    const auto second_scene_pass{graphics::begin_render_pass(ui_pass())};
    ASSERT_TRUE(second_scene_pass);
    auto commands{graphics::begin_encode(second_scene_pass)};
    graphics::end_encode(commands);
    ASSERT_TRUE(graphics::end_render_pass(second_scene_pass));
    ASSERT_TRUE(begin_ui());
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
}

TEST_F(render_test, ui_copy_then_scene_preserves_order_until_frame_submission)
{
    const auto copied{graphics::create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(copied);
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_render_pass({.render_targets = {{color_, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}, .depth_stencil = {{depth_, {begin_op::clear, {.depth = 1.0f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}}));
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(graphics::copy(color_, {0, 0, 1, 1}, *copied, {0, 0, 1, 1}));
    const auto snapshot{graphics::readback(*copied, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(graphics::poll(*snapshot).status, readback_status::pending);
    const auto scene{graphics::begin_render_pass({.render_targets = {{color_, {begin_op::clear, {.color = {0, 1, 0, 1}}}, {end_op::store}}}, .depth_stencil = {{depth_, {begin_op::clear, {.depth = 1.0f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}})};
    ASSERT_TRUE(graphics::end_render_pass(scene));
    EXPECT_EQ(graphics::poll(*snapshot).status, readback_status::pending);
    ASSERT_TRUE(finish_frame());
    const auto red{poll_until_complete(*snapshot)};
    ASSERT_EQ(red.status, readback_status::ready);
    EXPECT_EQ(std::to_integer<unsigned>(red.value.data[0]), 255u);
    EXPECT_EQ(std::to_integer<unsigned>(red.value.data[1]), 0u);
    const auto current{graphics::readback(color_, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(current);
    const auto green{await_readback(*current)};
    ASSERT_TRUE(green);
    EXPECT_EQ(std::to_integer<unsigned>(green->data[0]), 0u);
    EXPECT_EQ(std::to_integer<unsigned>(green->data[1]), 255u);
    graphics::destroy(*current);
    graphics::destroy(*snapshot);
    graphics::destroy(*copied);
}

TEST_F(render_test, font_growth_with_minimized_main_viewport)
{
    ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport*)
    {
    };
    for (int index{0}; index < 6; ++index)
    {
        SCOPED_TRACE(index);
        ASSERT_TRUE(start_frame());
        ASSERT_TRUE(begin_ui());
        if (index == 1)
        {
            const auto atlas{ImGui::GetIO().Fonts->TexData};
            ASSERT_NE(ImGui::GetIO().Fonts->AddCustomRect(atlas->Width, atlas->Height), ImFontAtlasRectId_Invalid);
        }
        const auto main_viewport{ImGui::GetMainViewport()};
        ImGui::SetNextWindowPos(main_viewport->Pos + ImVec2{main_viewport->Size.x + 100.0f, 0});
        ImGui::SetNextWindowSize({128, 128});
        ImGui::Begin("Font growth viewport", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextUnformatted("Font growth");
        ImGui::End();
        main_viewport->Flags |= ImGuiViewportFlags_IsMinimized;
        ASSERT_TRUE(finish_ui());
        EXPECT_EQ(ImGui::GetDrawData()->DisplaySize.x, 0.0f);
        ASSERT_TRUE(finish_frame());
        EXPECT_GT(ImGui::GetPlatformIO().Viewports.Size, 1);
        EXPECT_EQ(ImGui::GetIO().Fonts->TexData->Status, ImTextureStatus_OK);
    }
}

TEST_F(render_test, graphics_only_frames_skip_viewports)
{
    ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport*)
    {
    };
    int platform_draws{0};
    for (int index{0}; index < 8; ++index)
    {
        SCOPED_TRACE(index);
        ASSERT_TRUE(start_frame());
        const int previous_draws{platform_draws};
        if (index % 4 == 3)
        {
            // graphics만 사용하는 frame에서 이전 viewport draw data 재생 방지 검증
            ASSERT_TRUE(finish_frame());
            EXPECT_EQ(platform_draws, previous_draws);
            continue;
        }
        ASSERT_TRUE(begin_ui());
        const auto main_viewport{ImGui::GetMainViewport()};
        ImGui::SetNextWindowPos(main_viewport->Pos + ImVec2{main_viewport->Size.x + 100.0f, 0});
        ImGui::SetNextWindowSize({128, 128});
        ImGui::Begin("Skipped viewport", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextUnformatted("Frame lifecycle");
        ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList*, const ImDrawCmd* command)
        {
            ++*static_cast<int*>(command->UserCallbackData);
        }, &platform_draws);
        ImGui::End();
        ASSERT_TRUE(finish_ui());
        ASSERT_TRUE(finish_frame());
        if (index > 0)
        {
            EXPECT_GT(platform_draws, previous_draws);
        }
    }
}

TEST_F(render_test, offscreen_preview_pixels)
{
    const auto preview_texture{graphics::create_texture({texture_format::rgba8_unorm, 32, 32, 1, 1, texture_usage::render_target | texture_usage::shader_resource}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(preview_texture);
    ASSERT_TRUE(start_frame());
    const auto preview_pass{graphics::begin_render_pass({.render_targets = {{*preview_texture, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(preview_pass);
    ASSERT_TRUE(graphics::end_render_pass(preview_pass));
    ASSERT_TRUE(begin_ui());
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({128, 128});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto preview_size{ImGui::GetContentRegionAvail()};
    EXPECT_GT(preview_size.x, 0);
    EXPECT_GT(preview_size.y, 0);
    ImGui::Image(texture_id(*preview_texture), {96, 96});
    ImGui::End();
    ImGui::PopStyleVar();
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    const auto request{graphics::readback(color_, {32, 32, 1, 1, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    const auto pixels{await_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_GE(pixels->data.size(), 4u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels->data[0]), 255u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels->data[1]), 0u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels->data[2]), 0u);
    graphics::destroy(*request);
    graphics::destroy(*preview_texture);
}

TEST_F(render_test, depthless_target_pixels_and_clipping)
{
    target_.depth_stencil.reset();
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    const auto list{ImGui::GetForegroundDrawList(ImGui::GetMainViewport())};
    list->PushClipRect({8, 8}, {24, 24});
    list->AddRectFilled({0, 0}, {64, 64}, IM_COL32(255, 0, 0, 128));
    list->PopClipRect();
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    const auto ticket{graphics::readback(color_, {0, 0, 64, 64, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(ticket);
    const auto pixels{await_readback(*ticket)};
    ASSERT_TRUE(pixels);
    const auto channel = [&pixels](std::size_t x, std::size_t y, std::size_t component)
    {
        return std::to_integer<unsigned>(pixels->data[y * pixels->row_pitch + x * 4 + component]);
    };
    EXPECT_NEAR(channel(16, 16, 0), 128u, 1);
    EXPECT_NEAR(channel(16, 16, 3), 128u, 1);
    EXPECT_EQ(channel(0, 0, 0), 0u);
    EXPECT_EQ(channel(40, 40, 0), 0u);
    graphics::destroy(*ticket);
}

TEST_F(render_test, graphics_only_frame_does_not_replay_main_ui)
{
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    int calls{0};
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd)
    {
        ++*static_cast<int*>(cmd->UserCallbackData);
    }, &calls);
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    EXPECT_EQ(calls, 1);
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(finish_frame());
    EXPECT_EQ(calls, 1);
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    EXPECT_EQ(calls, 1);
}

TEST_F(render_test, large_geometry_and_vertex_offsets)
{
    ASSERT_TRUE(start_frame());
    ASSERT_TRUE(begin_ui());
    const auto list{ImGui::GetForegroundDrawList(ImGui::GetMainViewport())};
    for (int index{0}; index < 70000; ++index)
    {
        list->AddRectFilled({8, 8}, {16, 16}, IM_COL32(0, 255, 0, 255));
    }
    list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
    list->AddRectFilled({32, 32}, {48, 48}, IM_COL32(255, 0, 0, 255));
    ASSERT_TRUE(finish_ui());
    ASSERT_TRUE(finish_frame());
    const auto ticket{graphics::readback(color_, {40, 40, 1, 1, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(ticket);
    const auto pixels{await_readback(*ticket)};
    ASSERT_TRUE(pixels);
    EXPECT_EQ(std::to_integer<unsigned>(pixels->data[0]), 255u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels->data[1]), 0u);
    graphics::destroy(*ticket);
}
} // namespace nori::graphics::imgui
