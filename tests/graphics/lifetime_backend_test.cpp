#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <wrl/client.h>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "command_queue.hpp"
#include "context.hpp"
#include "descriptor_allocator.hpp"
#include "lifetime_backend_test_helpers.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "resource/texture_manager.hpp"
#include "upload_manager.hpp"

namespace
{
class lifetime_backend_test : public testing::Test
{
protected:
    lifetime_backend_test() :
        color_{},
        target_{}
    {
    }

    void SetUp() override
    {
        ASSERT_TRUE(nori::graphics::initialize());
        const auto color{nori::graphics::create_texture({nori::graphics::texture_format::rgba16_float, 16, 16, 1, 1, nori::graphics::texture_usage::render_target}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(color);
        color_ = *color;
        target_ = {{{color_, {nori::graphics::begin_op::clear}, {nori::graphics::end_op::store}}}, std::nullopt};
    }

    void TearDown() override
    {
        nori::graphics::shutdown();
    }

    bool start_frame()
    {
        return nori::graphics::begin_frame();
    }

    bool finish_frame()
    {
        return nori::graphics::end_frame();
    }

    nori::graphics::result<nori::graphics::descriptor_handle> allocate()
    {
        const auto allocated{nori::graphics::ctx()->descriptors()->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        if (allocated.cpu.ptr == 0)
        {
            return std::unexpected{nori::graphics::error_code::descriptor_allocation_failed};
        }
        descriptors_.push_back(allocated);
        return allocated;
    }

    void release(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
    {
        const auto it{std::ranges::find_if(descriptors_, [cpu](const auto& owned)
        {
            return owned.cpu.ptr == cpu.ptr;
        })};
        defer_descriptor_release(*it);
        descriptors_.erase(it);
    }

    void defer_descriptor_release(nori::graphics::descriptor_handle handle)
    {
        const auto graphics_context{nori::graphics::ctx()};
        nori::graphics::resource::texture tex{graphics_context->descriptors()};
        tex.srv_handle = handle;
        graphics_context->queue()->release(std::move(tex));
    }

    std::vector<nori::graphics::descriptor_handle> descriptors_;
    nori::graphics::texture_handle color_;
    nori::graphics::render_pass_desc target_;
};
} // namespace

namespace nori::graphics
{
TEST_F(lifetime_backend_test, shutdown_releases_unsubmitted_frame_resources)
{
    ASSERT_TRUE(start_frame());
    const auto target_pass{begin_render_pass(target_)};
    ASSERT_TRUE(target_pass);
    const auto descriptor{allocate()};
    ASSERT_TRUE(descriptor);
    release(descriptor->cpu);
    const auto released{std::make_shared<std::uint32_t>(0)};
    ctx()->queue()->release(destruction_probe{released});
    ASSERT_TRUE(wait_gpu());
    EXPECT_EQ(*released, 0u);
    shutdown();
    EXPECT_EQ(*released, 1u);
}

TEST_F(lifetime_backend_test, failed_frame_end_allows_caller_shutdown_and_reinitialization)
{
    ASSERT_TRUE(start_frame());
    const auto target_pass{begin_render_pass(target_)};
    ASSERT_TRUE(end_render_pass(target_pass));
    const auto released{std::make_shared<std::uint32_t>(0)};
    const auto queue{ctx()->queue()};
    queue->release(destruction_probe{released});
    const auto batch{queue->submission()};
    ASSERT_TRUE(SUCCEEDED(queue->recording_list()->Close()));
    EXPECT_FALSE(end_frame());
    EXPECT_TRUE(batch->failed);
    EXPECT_EQ(batch->fence_value, 0u);
    EXPECT_EQ(*released, 0u);

    shutdown();
    EXPECT_EQ(*released, 1u);
    ASSERT_TRUE(initialize());
    ASSERT_TRUE(begin_frame());
    EXPECT_TRUE(end_frame());
}

TEST_F(lifetime_backend_test, shutdown_waits_for_submitted_work_before_destroying_resources)
{
    const auto graphics_context{ctx()};
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))));
    queue_gate gate{fence};
    ASSERT_TRUE(SUCCEEDED(graphics_context->queue()->native()->Wait(fence.Get(), 1)));
    auto& descriptors{*graphics_context->descriptors()};
    const auto released{std::make_shared<std::uint32_t>(0)};
    graphics_context->queue()->release(resource::sampler{&descriptors, descriptors.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)});
    graphics_context->queue()->release(destruction_probe{released});

    ASSERT_TRUE(start_frame());
    const std::vector<std::byte> data(256);
    ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::constant, data));
    resource::texture tex{&descriptors};
    tex.srv_handle = descriptors.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    graphics_context->queue()->release(std::move(tex));
    graphics_context->queue()->release(std::make_unique<destruction_probe>(released));
    const auto completed{std::make_shared<bool>(false)};
    graphics_context->queue()->on_completed([completed]()
    {
        *completed = true;
    });
    const auto batch{graphics_context->queue()->submission()};
    ASSERT_TRUE(finish_frame());
    ASSERT_NE(batch->fence_value, 0u);
    EXPECT_LT(completed_fence(), batch->fence_value);
    EXPECT_EQ(*released, 0u);
    EXPECT_FALSE(*completed);

    std::jthread release_queue{[&gate]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
        gate.open();
    }};
    shutdown();
    EXPECT_EQ(fence->GetCompletedValue(), 1u);
    EXPECT_EQ(*released, 2u);
    EXPECT_TRUE(*completed);
}

TEST_F(lifetime_backend_test, frame_release_survives_early_queue_checkpoint)
{
    const auto original{allocate()};
    ASSERT_TRUE(original);
    ASSERT_TRUE(start_frame());
    release(original->cpu);
    const auto destroyed{std::make_shared<std::uint32_t>(0)};
    const auto graphics_context{ctx()};
    graphics_context->queue()->release(destruction_probe{destroyed});
    graphics_context->queue()->release(std::make_unique<destruction_probe>(destroyed));
    graphics_context->queue()->release(destruction_probe{destroyed});
    ASSERT_TRUE(wait_gpu());
    EXPECT_EQ(*destroyed, 0u);
    const auto before_submission{allocate()};
    ASSERT_TRUE(before_submission);
    EXPECT_NE(before_submission->cpu.ptr, original->cpu.ptr);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))));
    queue_gate gate{fence};
    ASSERT_TRUE(SUCCEEDED(graphics_context->queue()->native()->Wait(fence.Get(), 1)));
    const auto batch{graphics_context->queue()->submission()};
    ASSERT_TRUE(finish_frame());
    ASSERT_NE(batch->fence_value, 0u);
    EXPECT_LT(completed_fence(), batch->fence_value);
    EXPECT_EQ(*destroyed, 0u);
    const auto in_flight{allocate()};
    ASSERT_TRUE(in_flight);
    EXPECT_NE(in_flight->cpu.ptr, original->cpu.ptr);
    gate.open();
    ASSERT_TRUE(wait_gpu(batch->fence_value));
    ASSERT_TRUE(wait_gpu());
    EXPECT_EQ(*destroyed, 3u);
    graphics_context->queue()->collect();
    EXPECT_EQ(*destroyed, 3u);
    const auto collected{allocate()};
    ASSERT_TRUE(collected);
    EXPECT_EQ(collected->cpu.ptr, original->cpu.ptr);
}

TEST_F(lifetime_backend_test, frame_release_survives_upload_and_transfer_completion)
{
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && destination);
    const auto graphics_context{ctx()};
    ASSERT_TRUE(wait_gpu());
    const auto destroyed{std::make_shared<std::uint32_t>(0)};
    ASSERT_TRUE(start_frame());
    const auto batch{graphics_context->queue()->submission()};
    graphics_context->queue()->release(destruction_probe{destroyed});
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&pixel, 1}), 4}));
    EXPECT_EQ(batch->fence_value, 0u);
    ASSERT_TRUE(copy(*source, {0, 0, 1, 1}, *destination, {0, 0, 1, 1}));
    const auto request{readback(*destination, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    EXPECT_EQ(batch->fence_value, 0u);
    ASSERT_TRUE(wait_gpu());
    EXPECT_EQ(batch->fence_value, 0u);
    EXPECT_EQ(poll(*request).status, readback_status::pending);
    EXPECT_EQ(*destroyed, 0u);
    ASSERT_TRUE(finish_frame());
    ASSERT_NE(batch->fence_value, 0u);
    EXPECT_FALSE(batch->failed);
    ASSERT_TRUE(wait_gpu(batch->fence_value));
    graphics_context->queue()->collect();
    EXPECT_EQ(*destroyed, 1u);
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    std::uint32_t actual{};
    std::memcpy(&actual, status.value.data.data(), sizeof(actual));
    EXPECT_EQ(actual, pixel);

    destroy(*request);
    destroy(*destination);
    destroy(*source);
}

TEST_F(lifetime_backend_test, frame_release_waits_for_external_queue_work)
{
    const auto graphics_context{ctx()};
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))));
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list))));
    ASSERT_TRUE(SUCCEEDED(command_list->Close()));
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))));
    queue_gate gate{fence};
    const auto destroyed{std::make_shared<std::uint32_t>(0)};
    ASSERT_TRUE(graphics_context->queue()->begin_frame());
    graphics_context->queue()->release(destruction_probe{destroyed});
    const auto batch{graphics_context->queue()->submission()};
    ASSERT_TRUE(graphics_context->queue()->submit());
    ASSERT_NE(batch->fence_value, 0u);
    const auto main_submission{batch->fence_value};
    ASSERT_TRUE(graphics_context->queue()->wait(main_submission));
    ASSERT_TRUE(SUCCEEDED(graphics_context->queue()->native()->Wait(fence.Get(), 1)));
    ID3D12CommandList* const native_list{command_list.Get()};
    graphics_context->queue()->native()->ExecuteCommandLists(1, &native_list);
    const auto final_submission{graphics_context->queue()->signal()};
    ASSERT_GT(final_submission, main_submission);
    graphics_context->queue()->end_frame();
    graphics_context->queue()->collect();
    EXPECT_LT(graphics_context->queue()->completed_fence_value(), final_submission);
    EXPECT_EQ(*destroyed, 0u);
    gate.open();
    ASSERT_TRUE(graphics_context->queue()->wait(final_submission));
    graphics_context->queue()->collect();
    EXPECT_EQ(*destroyed, 1u);
}
} // namespace nori::graphics
