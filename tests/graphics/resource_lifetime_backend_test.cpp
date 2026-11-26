#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "command_queue.hpp"
#include "context.hpp"
#include "descriptor_test_helpers.hpp"
#include "lifetime_backend_test_helpers.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "resource/texture_manager.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
TEST(resource_lifetime_backend_test, failed_texture_creation_returns_partial_descriptors)
{
    const auto graphics_context{context::create()};
    ASSERT_TRUE(graphics_context);
    auto& descriptors{*graphics_context->descriptors()};
    upload_manager uploads{{graphics_context->device(), graphics_context->queue()}};
    resource::texture_manager textures{{graphics_context->device(), &descriptors, &uploads, graphics_context->queue()}};
    for (const auto type : {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV})
    {
        std::vector<descriptor_handle> reserved;
        while (true)
        {
            const auto handle{descriptors.allocate(type)};
            if (handle.cpu.ptr == 0)
            {
                break;
            }
            reserved.push_back(handle);
        }
        for (std::uint32_t index{0}; index < 2; ++index)
        {
            descriptors.free(reserved.back());
            reserved.pop_back();
        }
        const auto srv_count{available_descriptor_count(descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        const auto rtv_count{available_descriptor_count(descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
        const auto tex{textures.create({texture_format::rgba8_unorm, 16, 16, 3, 1, texture_usage::shader_resource | texture_usage::render_target})};
        EXPECT_FALSE(tex);
        if (tex)
        {
            textures.destroy(*tex);
            EXPECT_TRUE(graphics_context->queue()->wait_idle());
            graphics_context->queue()->collect();
        }
        else
        {
            EXPECT_EQ(tex.error(), error_code::descriptor_allocation_failed);
        }
        EXPECT_EQ(available_descriptor_count(descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), srv_count);
        EXPECT_EQ(available_descriptor_count(descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_RTV), rtv_count);
        for (const auto& handle : reserved)
        {
            descriptors.free(handle);
        }
    }
}
} // namespace nori::graphics
