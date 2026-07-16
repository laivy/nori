#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_set>
#include "gpu_timeline.hpp"
#include "pipeline_manager.hpp"

namespace nori::graphics
{
namespace
{
DXGI_FORMAT dxgi_format(render_texture_format format)
{
    switch (format)
    {
    case render_texture_format::rgba8_unorm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case render_texture_format::rgba16_float:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case render_texture_format::r32_float:
        return DXGI_FORMAT_R32_FLOAT;
    case render_texture_format::d32_float:
        return DXGI_FORMAT_D32_FLOAT;
    }
    return DXGI_FORMAT_UNKNOWN;
}
} // namespace

pipeline_manager::pipeline_manager(ID3D12Device* device, gpu_timeline* timeline) :
    device_{device},
    timeline_{timeline}
{
    assert(device_);
    assert(timeline_);
}

result<pipeline_handle> pipeline_manager::create_pipeline(const graphics_pipeline_specification& spec)
{
    if (!shaders_.contains(spec.vertex_shader) || !shaders_.contains(spec.pixel_shader) || spec.color_formats.empty() || spec.color_formats.size() > 8)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    const auto& vertex{shaders_.get(spec.vertex_shader)};
    const auto& pixel{shaders_.get(spec.pixel_shader)};
    if (vertex.stage != shader_stage::vertex || pixel.stage != shader_stage::pixel)
    {
        return std::unexpected{error_code::invalid_argument};
    }

    bool has_srv{};
    bool has_sampler{};
    for (const shader_binding& binding : spec.bindings)
    {
        if (binding.visibility != shader_stage::pixel || binding.shader_register != 0)
        {
            return std::unexpected{error_code::unsupported};
        }
        if (binding.type == shader_binding_type::texture_srv)
        {
            if (has_srv)
            {
                return std::unexpected{error_code::invalid_argument};
            }
            has_srv = true;
        }
        else
        {
            if (has_sampler)
            {
                return std::unexpected{error_code::invalid_argument};
            }
            has_sampler = true;
        }
    }
    if (!has_srv || !has_sampler)
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const CD3DX12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0};
    CD3DX12_ROOT_PARAMETER parameter;
    parameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
    const CD3DX12_STATIC_SAMPLER_DESC sampler{0, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
    const CD3DX12_ROOT_SIGNATURE_DESC root_desc{
        1, &parameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
    };
    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    if (FAILED(::D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    pipeline_resource result{spec.vertex_shader, spec.pixel_shader};
    result.color_formats = spec.color_formats;
    if (FAILED(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&result.root_signature))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = result.root_signature.Get();
    desc.VS = {vertex.bytecode.data(), vertex.bytecode.size()};
    desc.PS = {pixel.bytecode.data(), pixel.bytecode.size()};
    desc.BlendState = CD3DX12_BLEND_DESC{D3D12_DEFAULT};
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC{D3D12_DEFAULT};
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC{D3D12_DEFAULT};
    desc.DepthStencilState.DepthEnable = spec.depth_format.has_value();
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = static_cast<UINT>(spec.color_formats.size());
    for (std::size_t i{}; i < spec.color_formats.size(); ++i)
    {
        desc.RTVFormats[i] = dxgi_format(spec.color_formats[i]);
        if (desc.RTVFormats[i] == DXGI_FORMAT_UNKNOWN || desc.RTVFormats[i] == DXGI_FORMAT_D32_FLOAT)
        {
            return std::unexpected{error_code::invalid_argument};
        }
    }
    if (spec.depth_format && *spec.depth_format != render_texture_format::d32_float)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    desc.DSVFormat = spec.depth_format ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result.pipeline_state))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    return pipelines_.emplace(std::move(result));
}

result<void> pipeline_manager::destroy(pipeline_handle handle)
{
    if (!pipelines_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    auto resource{std::move(pipelines_.get(handle))};
    pipelines_.erase(handle);
    timeline_->defer([resource = std::move(resource)]() mutable
        {
            resource.pipeline_state.Reset();
            resource.root_signature.Reset();
        });
    return {};
}

result<shader_handle> pipeline_manager::create_shader(shader_specification spec)
{
    if (spec.bytecode.empty())
    {
        return std::unexpected{error_code::invalid_argument};
    }
    return shaders_.emplace(shader_resource{spec.stage, {spec.bytecode.begin(), spec.bytecode.end()}});
}

result<void> pipeline_manager::destroy(shader_handle handle)
{
    if (!shaders_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    if (std::ranges::any_of(pipelines_, [handle](const auto& item)
            {
                return item.second.vertex_shader == handle || item.second.pixel_shader == handle;
            }))
    {
        return std::unexpected{error_code::resource_in_use};
    }
    shaders_.erase(handle);
    return {};
}

result<void> pipeline_manager::bind_fullscreen(pipeline_handle handle, std::span<const render_texture_format> output_formats, D3D12_GPU_DESCRIPTOR_HANDLE source, ID3D12GraphicsCommandList* command_list)
{
    if (!pipelines_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    assert(command_list);
    assert(source.ptr != 0);
    const auto& pipeline{pipelines_.get(handle)};
    if (!std::ranges::equal(pipeline.color_formats, output_formats))
    {
        return std::unexpected{error_code::invalid_state};
    }
    command_list->SetGraphicsRootSignature(pipeline.root_signature.Get());
    command_list->SetPipelineState(pipeline.pipeline_state.Get());
    command_list->SetGraphicsRootDescriptorTable(0, source);
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->DrawInstanced(3, 1, 0, 0);
    return {};
}
} // namespace nori::graphics
