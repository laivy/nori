#include "command_queue.hpp"
#include "config.hpp"
#include "context.hpp"
#include "frame_upload_test_helpers.hpp"

namespace
{
class frame_queue_gate
{
public:
    explicit frame_queue_gate(Microsoft::WRL::ComPtr<ID3D12Fence> fence) :
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

    ~frame_queue_gate()
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

class frame_data_backend_test : public frame_upload_buffer_test
{
};

TEST_F(frame_data_backend_test, pending_frame_data_survives_following_frame_and_completed_slot_reuse)
{
    constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
    constexpr std::array<float, 8> red_constants{4, 4, 0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 8> green_constants{4, 4, 0, 0, 0, 1, 0, 1};
    ASSERT_TRUE(begin_frame());
    constexpr auto left{quad(0, 2)};
    const auto first_vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{left}))};
    const auto first_indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{indices}))};
    const auto first_constant_buffer{create_frame_upload_buffer(buffer_usage::constant, std::as_bytes(std::span{red_constants}))};
    ASSERT_TRUE(first_vertices && first_indices && first_constant_buffer);
    const auto first_pass{begin_render_pass(pass())};
    const auto first_encoder{begin_encode(first_pass)};
    draw(*first_encoder, *first_vertices, *first_indices, *first_constant_buffer);
    end_encode(first_encoder);
    ASSERT_TRUE(end_render_pass(first_pass));
    const auto first_readback{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first_readback);
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    const auto graphics_context{ctx()};
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))));
    frame_queue_gate gate{fence};
    ASSERT_TRUE(SUCCEEDED(graphics_context->queue()->native()->Wait(fence.Get(), 1)));
    const auto batch{graphics_context->queue()->submission()};
    ASSERT_TRUE(end_frame());
    ASSERT_NE(batch->fence_value, 0u);
    const auto first_submission{batch->fence_value};
    ASSERT_LT(graphics_context->queue()->completed_fence_value(), first_submission);

    constexpr auto full{quad(0, 4)};
    std::array<readback_handle, frames_in_flight - 1> following_readbacks{};
    for (auto& following_readback : following_readbacks)
    {
        ASSERT_TRUE(begin_frame());
        const auto vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{full}))};
        const auto index_buffer{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{indices}))};
        const auto constant_buffer{create_frame_upload_buffer(buffer_usage::constant, std::as_bytes(std::span{green_constants}))};
        ASSERT_TRUE(vertices && index_buffer && constant_buffer);
        const auto following_pass{begin_render_pass(pass())};
        const auto following_encoder{begin_encode(following_pass)};
        draw(*following_encoder, *vertices, *index_buffer, *constant_buffer);
        end_encode(following_encoder);
        ASSERT_TRUE(end_render_pass(following_pass));
        const auto request{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(request);
        following_readback = *request;
        ASSERT_TRUE(end_frame());
        EXPECT_EQ(fence->GetCompletedValue(), 0u);
        EXPECT_LT(graphics_context->queue()->completed_fence_value(), first_submission);
    }
    gate.open();

    ASSERT_TRUE(begin_frame());
    EXPECT_GE(graphics_context->queue()->completed_fence_value(), first_submission);
    const auto reused_vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{full}))};
    const auto reused_indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{indices}))};
    const auto reused_constant_buffer{create_frame_upload_buffer(buffer_usage::constant, std::as_bytes(std::span{red_constants}))};
    ASSERT_TRUE(reused_vertices && reused_indices && reused_constant_buffer);
    const auto reused_pass{begin_render_pass(pass())};
    const auto reused_encoder{begin_encode(reused_pass)};
    draw(*reused_encoder, *reused_vertices, *reused_indices, *reused_constant_buffer);
    end_encode(reused_encoder);
    ASSERT_TRUE(end_render_pass(reused_pass));
    const auto reused_readback{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(reused_readback);
    ASSERT_TRUE(end_frame());
    expect_readback(*first_readback, 0xFF0000FFu, 0u);
    for (const auto following_readback : following_readbacks)
    {
        expect_readback(following_readback, 0xFF00FF00u, 0xFF00FF00u);
    }
    expect_readback(*reused_readback, 0xFF0000FFu, 0xFF0000FFu);
}
} // namespace
