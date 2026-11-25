#include "recording_backend_test_helpers.hpp"

namespace
{
TEST_F(recording_backend_test, readbacks_remain_pending_until_gpu_completion)
{
    const auto color{create_color(1)};
    ASSERT_TRUE(color);
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(textures_->update(*color, {0, 0, 1, 1}, {std::as_bytes(std::span{&pixel, 1}), 4}));
    ASSERT_TRUE(graphics_context_->queue()->wait_idle());
    Microsoft::WRL::ComPtr<ID3D12Fence> gate_fence;
    ASSERT_TRUE(SUCCEEDED(graphics_context_->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate_fence))));
    {
        const submission_queue_gate gate{gate_fence};
        ASSERT_TRUE(SUCCEEDED(graphics_context_->queue()->native()->Wait(gate_fence.Get(), 1)));
        const auto first{transfers_->read_texture(*color, {0, 0, 1, 1})};
        const auto second{transfers_->read_texture(*color, {0, 0, 1, 1})};
        ASSERT_TRUE(first && second);
        ASSERT_TRUE(graphics_context_->queue()->submit());
        const auto first_status{transfers_->poll(*first)};
        const auto second_status{transfers_->poll(*second)};
        EXPECT_EQ(first_status.status, readback_status::pending);
        EXPECT_EQ(second_status.status, readback_status::pending);
        EXPECT_TRUE(first_status.value.data.empty());
        EXPECT_TRUE(second_status.value.data.empty());
    }
    ASSERT_TRUE(graphics_context_->queue()->wait_idle());
    const auto third{transfers_->read_texture(*color, {0, 0, 1, 1})};
    ASSERT_TRUE(third);
    expect_pixels(*third, pixel);
}

TEST_F(recording_backend_test, discarded_readback_reports_failed)
{
    const auto color{create_color(1)};
    ASSERT_TRUE(color);
    ASSERT_TRUE(graphics_context_->queue()->begin_frame());
    const auto request{transfers_->read_texture(*color, {0, 0, 1, 1})};
    ASSERT_TRUE(request);
    EXPECT_EQ(transfers_->poll(*request).status, readback_status::pending);
    graphics_context_->queue()->discard();
    EXPECT_EQ(transfers_->poll(*request).status, readback_status::failed);
    graphics_context_->queue()->end_frame();
}

TEST_F(recording_backend_test, mixed_mip_states_and_targeted_upload)
{
    const auto color{create_color(4, 3)};
    ASSERT_TRUE(color);
    auto& texture{textures_->at(*color)};
    const auto command_list{graphics_context_->queue()->record()};
    ASSERT_TRUE(command_list);
    texture.states.transition(command_list, texture.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
    texture.states.transition(command_list, texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, 1);
    texture.states.transition(command_list, texture.resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    texture.states.transition(command_list, texture.resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE); // 중복 barrier 생략 검증
    ASSERT_TRUE(graphics_context_->queue()->submit());
    ASSERT_TRUE(graphics_context_->queue()->wait_idle());
    const std::array<std::uint32_t, 4> pixels{0xFFABCDEF, 0xFFABCDEF, 0xFFABCDEF, 0xFFABCDEF};
    ASSERT_TRUE(textures_->update(*color, {0, 0, 2, 2, 1}, {std::as_bytes(std::span{pixels}), 8}));
    const auto request{transfers_->read_texture(*color, {0, 0, 2, 2, 1})};
    ASSERT_TRUE(request);
    expect_pixels(*request, pixels.front());
}
} // namespace
