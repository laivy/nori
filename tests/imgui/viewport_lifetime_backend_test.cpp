#include "command_queue.hpp"
#include "context.hpp"
#include "imgui_test_helpers.hpp"

namespace
{
class queue_gate
{
public:
    explicit queue_gate(Microsoft::WRL::ComPtr<ID3D12Fence> fence) :
        release_{false},
        worker_{[this, fence = std::move(fence)]()
    {
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        while (!release_.load() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        EXPECT_TRUE(SUCCEEDED(fence->Signal(1)));
    }}
    {
    }

    ~queue_gate()
    {
        open();
    }

    void open()
    {
        release_.store(true);
    }

private:
    std::atomic<bool> release_;
    std::jthread worker_;
};

class viewport_lifetime_backend_test : public render_test
{
};
} // namespace

namespace nori::graphics::imgui
{
TEST_F(viewport_lifetime_backend_test, detached_preview_submission_and_resize)
{
    ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport*)
    {
    };
    const auto preview{graphics::create_texture({texture_format::rgba8_unorm, 32, 32, 1, 1, texture_usage::render_target | texture_usage::shader_resource | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(preview);
    for (int index{0}; index < 4; ++index)
    {
        SCOPED_TRACE(index);
        ASSERT_TRUE(start_frame());
        const auto preview_pass{graphics::begin_render_pass({.render_targets = {{*preview, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
        ASSERT_TRUE(preview_pass);
        ASSERT_TRUE(graphics::end_render_pass(preview_pass));
        ASSERT_TRUE(begin_ui());
        const auto queue{graphics::ctx()->queue()};
        const auto batch{queue->submission()};
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        ASSERT_TRUE(SUCCEEDED(graphics::ctx()->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))));
        queue_gate gate{fence};
        const auto released{std::make_shared<bool>(false)};
        queue->release(std::shared_ptr<void>{nullptr, [released](void*)
        {
            *released = true;
        }});

        struct viewport_probe
        {
            bool called;
            ID3D12Fence* fence;
        };

        viewport_probe probe{false, fence.Get()};
        const auto main{ImGui::GetMainViewport()};
        ImGui::SetNextWindowPos(main->Pos + ImVec2{main->Size.x + 100, 0});
        ImGui::SetNextWindowSize({128.0f + 16.0f * index, 128.0f + 16.0f * index});
        ImGui::Begin("Detached preview", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::Image(texture_id(*preview), {64, 64});
        ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd)
        {
            auto& state{*static_cast<viewport_probe*>(cmd->UserCallbackData)};
            state.called = true;
            EXPECT_TRUE(SUCCEEDED(graphics::ctx()->queue()->native()->Wait(state.fence, 1)));
        }, &probe);
        ImGui::End();
        ASSERT_TRUE(finish_ui());
        EXPECT_FALSE(probe.called);
        ASSERT_TRUE(finish_frame());
        ASSERT_NE(batch->fence_value, 0u);
        ASSERT_TRUE(queue->wait(batch->fence_value));
        if (index > 0)
        {
            ASSERT_TRUE(probe.called);
            ASSERT_EQ(fence->GetCompletedValue(), 0u);
            queue->collect();
            EXPECT_FALSE(*released);
        }
        gate.open();
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        while (!*released && std::chrono::steady_clock::now() < deadline)
        {
            queue->collect();
            std::this_thread::yield();
        }
        EXPECT_TRUE(*released);
        EXPECT_GT(ImGui::GetPlatformIO().Viewports.Size, 1);
        const auto ticket{graphics::readback(*preview, {0, 0, 1, 1, 0}).transform([](auto&& owner)
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
    graphics::destroy(*preview);
}
} // namespace nori::graphics::imgui
