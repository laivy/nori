#include <directx/d3d12.h>
#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "command_queue.hpp"
#include "context.hpp"

using namespace nori::graphics;

namespace
{
TEST(queue_backend_test, begin_failure_does_not_advance_frame)
{
    auto graphics_context{context::create()};
    ASSERT_TRUE(graphics_context);
    Microsoft::WRL::ComPtr<ID3D12Device5> device;
    ASSERT_TRUE(SUCCEEDED(graphics_context->device()->QueryInterface(IID_PPV_ARGS(&device))));
    const auto queue{command_queue::create(device.Get())};
    ASSERT_TRUE(queue);
    graphics_context.reset();
    device->RemoveDevice();
    const auto started{queue->begin_frame()};
    EXPECT_FALSE(started);
    EXPECT_EQ(queue->frame_index(), 0u);
}
} // namespace
