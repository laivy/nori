#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <utility>

#include <directx/d3dx12.h>

#include <nori/core/vector.hpp>

#include "command_queue.hpp"
#include "descriptor_allocator.hpp"
#include "format.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/texture_manager.hpp"
#include "resource_states.hpp"
#include "shader/blit_ps.hpp"
#include "shader/blit_vs.hpp"
#include "transfer/manager.hpp"

namespace
{
struct blit_constants
{
    nori::core::vec2 source_offset;
    nori::core::vec2 source_scale;
};

static_assert(sizeof(blit_constants) == 4 * sizeof(float));

constexpr D3D12_FILTER to_blit_filter(nori::graphics::blit_filter filter)
{
    switch (filter)
    {
    case nori::graphics::blit_filter::point:
    {
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
    case nori::graphics::blit_filter::linear:
    {
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }
    default:
        std::unreachable();
    }
}

constexpr std::uint32_t mip_extent(std::uint32_t extent, std::uint16_t mip)
{
    return std::max(1u, extent >> mip);
}

constexpr std::size_t pipeline_index(nori::graphics::texture_format format, nori::graphics::blit_filter filter)
{
    return static_cast<std::size_t>(format) * 2 + static_cast<std::size_t>(filter);
}

constexpr D3D12_RESOURCE_STATES texture_idle_state(nori::graphics::texture_usage usage)
{
    return (usage & nori::graphics::texture_usage::shader_resource) != nori::graphics::texture_usage::none ?
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE :
        D3D12_RESOURCE_STATE_COMMON;
}
} // namespace

namespace nori::graphics::transfer
{
manager::manager(params args) :
    device_{args.device},
    descriptors_{args.descriptors},
    queue_{args.queue},
    buffers_{args.buffers},
    textures_{args.textures},
    blit_pipelines_{},
    readbacks_{}
{
}

result<void> manager::copy_buffer(buffer_handle source_handle, std::uint64_t source_offset, buffer_handle destination_handle, std::uint64_t destination_offset, std::uint64_t size)
{
    const auto list{queue_->record()};
    if (!list)
    {
        return std::unexpected{error_code::command_failed};
    }

    resource::buffer& source{buffers_->at(source_handle)};
    resource::buffer& destination{buffers_->at(destination_handle)};
    queue_->retain(source.resource.Get());
    queue_->retain(destination.resource.Get());
    if (source.access == buffer_access::gpu_only)
    {
        source.states.transition(list, source.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    destination.states.transition(list, destination.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(destination.resource.Get(), destination_offset, source.resource.Get(), source_offset, size);
    if (source.access == buffer_access::gpu_only)
    {
        source.states.transition(list, source.resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    }
    destination.states.transition(list, destination.resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    return {};
}

result<void> manager::copy_texture(texture_handle source_handle, texture_region source_region, texture_handle destination_handle, texture_region destination_region)
{
    resource::texture& source{textures_->at(source_handle)};
    resource::texture& destination{textures_->at(destination_handle)};

    const auto list{queue_->record()};
    if (!list)
    {
        return std::unexpected{error_code::command_failed};
    }

    queue_->retain(source.resource.Get());
    queue_->retain(destination.resource.Get());
    const UINT source_subresource{source_region.mip_level};
    const UINT destination_subresource{destination_region.mip_level};

    source.states.transition(list, source.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, source_subresource);
    destination.states.transition(list, destination.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, destination_subresource);
    const CD3DX12_TEXTURE_COPY_LOCATION source_location{source.resource.Get(), source_subresource};
    const CD3DX12_TEXTURE_COPY_LOCATION destination_location{destination.resource.Get(), destination_subresource};
    const D3D12_BOX source_box{source_region.x, source_region.y, 0, source_region.x + source_region.width, source_region.y + source_region.height, 1};
    list->CopyTextureRegion(&destination_location, destination_region.x, destination_region.y, 0, &source_location, &source_box);

    source.states.transition(list, source.resource.Get(), texture_idle_state(source.usage), source_region.mip_level);
    destination.states.transition(list, destination.resource.Get(), texture_idle_state(destination.usage), destination_region.mip_level);
    return {};
}

result<void> manager::blit_texture(texture_handle source_handle, texture_region source_region, texture_handle destination_handle, texture_region destination_region, blit_filter filter)
{
    resource::texture& source{textures_->at(source_handle)};
    resource::texture& destination{textures_->at(destination_handle)};

    const auto pipeline{get_blit_pipeline(destination.format, filter)};
    if (!pipeline)
    {
        return std::unexpected{pipeline.error()};
    }
    const auto source_srv{textures_->srv(source_handle, source_region.mip_level)};
    const auto destination_rtv{textures_->rtv(destination_handle, destination_region.mip_level)};
    const auto list{queue_->record()};
    if (!list)
    {
        return std::unexpected{error_code::command_failed};
    }

    queue_->retain(source.resource.Get());
    queue_->retain(destination.resource.Get());
    source.states.transition(list, source.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, source_region.mip_level);
    destination.states.transition(list, destination.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, destination_region.mip_level);
    const CD3DX12_VIEWPORT native_viewport{
        static_cast<float>(destination_region.x),
        static_cast<float>(destination_region.y),
        static_cast<float>(destination_region.width),
        static_cast<float>(destination_region.height)
    };
    const D3D12_RECT scissor{
        static_cast<LONG>(destination_region.x),
        static_cast<LONG>(destination_region.y),
        static_cast<LONG>(destination_region.x + destination_region.width),
        static_cast<LONG>(destination_region.y + destination_region.height)
    };
    list->OMSetRenderTargets(1, &destination_rtv, FALSE, nullptr);
    list->RSSetViewports(1, &native_viewport);
    list->RSSetScissorRects(1, &scissor);

    ID3D12DescriptorHeap* const heap{descriptors_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
    list->SetDescriptorHeaps(1, &heap);
    list->SetGraphicsRootSignature((*pipeline)->root_signature.Get());
    list->SetPipelineState((*pipeline)->pipeline_state.Get());

    const float source_width{static_cast<float>(mip_extent(source.width, source_region.mip_level))};
    const float source_height{static_cast<float>(mip_extent(source.height, source_region.mip_level))};
    const blit_constants constants{
        {static_cast<float>(source_region.x) / source_width, static_cast<float>(source_region.y) / source_height},
        {static_cast<float>(source_region.width) / source_width, static_cast<float>(source_region.height) / source_height},
    };
    list->SetGraphicsRoot32BitConstants(0, 4, &constants, 0);
    list->SetGraphicsRootDescriptorTable(1, source_srv);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);

    source.states.transition(list, source.resource.Get(), texture_idle_state(source.usage), source_region.mip_level);
    destination.states.transition(list, destination.resource.Get(), texture_idle_state(destination.usage), destination_region.mip_level);
    return {};
}

result<void> manager::generate_mips(texture_handle handle)
{
    resource::texture& tex{textures_->at(handle)};

    for (std::uint16_t mip{1}; mip < tex.mip_levels; ++mip)
    {
        const texture_region source{0, 0, mip_extent(tex.width, mip - 1), mip_extent(tex.height, mip - 1), static_cast<std::uint16_t>(mip - 1)};
        const texture_region destination{0, 0, mip_extent(tex.width, mip), mip_extent(tex.height, mip), mip};
        if (const auto res{blit_texture(handle, source, handle, destination, blit_filter::linear)}; !res)
        {
            return res;
        }
    }
    return {};
}

result<readback_handle> manager::read_texture(texture_handle handle, texture_region region)
{
    resource::texture& tex{textures_->at(handle)};
    const D3D12_RESOURCE_DESC desc{tex.resource->GetDesc()};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 size{};
    device_->GetCopyableFootprints(&desc, region.mip_level, 1, 0, &footprint, nullptr, nullptr, &size);

    const CD3DX12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_READBACK};
    const CD3DX12_RESOURCE_DESC readback_desc{CD3DX12_RESOURCE_DESC::Buffer(size)};
    readback request{};
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &readback_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&request.buffer))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    ID3D12GraphicsCommandList* const list{queue_->record()};
    if (!list)
    {
        return std::unexpected{error_code::command_failed};
    }

    queue_->retain(tex.resource.Get());
    queue_->retain(request.buffer.Get());

    tex.states.transition(list, tex.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, region.mip_level);
    const CD3DX12_TEXTURE_COPY_LOCATION source{tex.resource.Get(), region.mip_level};
    const CD3DX12_TEXTURE_COPY_LOCATION destination{request.buffer.Get(), footprint};
    const D3D12_BOX box{region.x, region.y, 0, region.x + region.width, region.y + region.height, 1};
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
    tex.states.transition(list, tex.resource.Get(), texture_idle_state(tex.usage), region.mip_level);

    request.submission = queue_->submission();
    request.footprint = footprint;
    request.format = tex.format;
    request.width = region.width;
    request.height = region.height;
    request.bytes_per_pixel = bytes_per_pixel(tex.format);
    return readbacks_.emplace(std::move(request));
}

readback_result manager::poll(readback_handle handle)
{
    queue_->collect();
    readback& entry{readbacks_.at(handle)};
    const std::uint64_t completed{queue_->completed_fence_value()};
    if (entry.submission->failed || completed == std::numeric_limits<std::uint64_t>::max())
    {
        return {readback_status::failed};
    }
    if (!entry.submission->fence_value || entry.submission->fence_value > completed)
    {
        return {readback_status::pending};
    }
    if (!entry.value)
    {
        const bool compressed{entry.format == texture_format::bc5_unorm || entry.format == texture_format::bc7_unorm || entry.format == texture_format::bc7_unorm_srgb};
        texture_readback value{
            entry.format,
            compressed ? std::max(1u, (entry.width + 3u) / 4u) : entry.width,
            compressed ? std::max(1u, (entry.height + 3u) / 4u) : entry.height,
            compressed ? std::max(1u, (entry.width + 3u) / 4u) * entry.bytes_per_pixel : entry.width * entry.bytes_per_pixel
        };
        value.data.resize(static_cast<std::size_t>(value.row_pitch) * value.height);
        std::byte* mapped{};
        if (FAILED(entry.buffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped))))
        {
            return {readback_status::failed};
        }
        for (std::uint32_t row{0}; row < value.height; ++row)
        {
            std::memcpy(
                value.data.data() + static_cast<std::size_t>(row) * value.row_pitch,
                mapped + entry.footprint.Offset + static_cast<std::size_t>(row) * entry.footprint.Footprint.RowPitch,
                value.row_pitch
            );
        }
        entry.buffer->Unmap(0, nullptr);
        entry.value = std::move(value);
    }
    return {readback_status::ready, *entry.value};
}

void manager::destroy(readback_handle handle)
{
    readbacks_.erase(handle);
}

result<manager::blit_pipeline*> manager::get_blit_pipeline(texture_format format, blit_filter filter)
{
    auto& cached{blit_pipelines_.at(pipeline_index(format, filter))};
    if (cached)
    {
        return &*cached;
    }

    const D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0};
    std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = {.Num32BitValues = 4};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_STATIC_SAMPLER_DESC sampler{0, to_blit_filter(filter), D3D12_TEXTURE_ADDRESS_MODE_CLAMP};
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const CD3DX12_ROOT_SIGNATURE_DESC root_desc{static_cast<UINT>(parameters.size()), parameters.data(), 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    if (FAILED(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, nullptr)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    blit_pipeline pipeline;
    if (FAILED(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&pipeline.root_signature))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    CD3DX12_DEPTH_STENCIL_DESC depth_stencil_state{D3D12_DEFAULT};
    depth_stencil_state.DepthEnable = FALSE;
    depth_stencil_state.StencilEnable = FALSE;
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
        .pRootSignature = pipeline.root_signature.Get(),
        .VS = {blit_vs, sizeof(blit_vs)},
        .PS = {blit_ps, sizeof(blit_ps)},
        .BlendState = CD3DX12_BLEND_DESC{D3D12_DEFAULT},
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC{D3D12_DEFAULT},
        .DepthStencilState = depth_stencil_state,
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = {to_dxgi_format(format)},
        .SampleDesc = {.Count = 1},
    };
    if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipeline_state))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    cached = std::move(pipeline);
    return &*cached;
}
} // namespace nori::graphics::transfer
