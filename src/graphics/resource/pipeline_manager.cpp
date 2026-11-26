#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <directx/d3dx12.h>

#include "command_queue.hpp"
#include "format.hpp"
#include "resource/pipeline_manager.hpp"

namespace
{
struct pipeline_creation_layout
{
    std::vector<D3D12_ROOT_PARAMETER> root_parameters;
    std::vector<D3D12_DESCRIPTOR_RANGE> texture_ranges;
    std::vector<D3D12_DESCRIPTOR_RANGE> sampler_ranges;
};

constexpr D3D12_SHADER_VISIBILITY shader_visibility(nori::graphics::shader_stage stage)
{
    switch (std::to_underlying(stage))
    {
    case std::to_underlying(nori::graphics::shader_stage::vertex):
    {
        return D3D12_SHADER_VISIBILITY_VERTEX;
    }
    case std::to_underlying(nori::graphics::shader_stage::pixel):
    {
        return D3D12_SHADER_VISIBILITY_PIXEL;
    }
    case std::to_underlying(nori::graphics::shader_stage::vertex | nori::graphics::shader_stage::pixel):
    {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
    default:
        std::unreachable();
    }
}

pipeline_creation_layout create_binding_layout(std::span<const nori::graphics::shader_binding> bindings)
{
    std::size_t texture_count{};
    std::size_t sampler_count{};
    for (const nori::graphics::shader_binding& binding : bindings)
    {
        switch (binding.type)
        {
        case nori::graphics::shader_binding_type::constant_buffer:
        {
            break;
        }
        case nori::graphics::shader_binding_type::texture_srv:
        {
            ++texture_count;
            break;
        }
        case nori::graphics::shader_binding_type::sampler:
        {
            ++sampler_count;
            break;
        }
        default:
            std::unreachable();
        }
    }

    pipeline_creation_layout layout;
    layout.root_parameters.reserve(bindings.size());
    layout.texture_ranges.reserve(texture_count);
    layout.sampler_ranges.reserve(sampler_count);

    for (const nori::graphics::shader_binding& binding : bindings)
    {
        const D3D12_SHADER_VISIBILITY visibility{shader_visibility(binding.visibility)};
        switch (binding.type)
        {
        case nori::graphics::shader_binding_type::constant_buffer:
        {
            CD3DX12_ROOT_PARAMETER parameter;
            parameter.InitAsConstantBufferView(binding.shader_register, 0, visibility);
            layout.root_parameters.push_back(parameter);
            break;
        }
        case nori::graphics::shader_binding_type::texture_srv:
        {
            layout.texture_ranges.emplace_back(CD3DX12_DESCRIPTOR_RANGE{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, binding.shader_register});
            CD3DX12_ROOT_PARAMETER parameter;
            parameter.InitAsDescriptorTable(1, &layout.texture_ranges.back(), visibility);
            layout.root_parameters.push_back(parameter);
            break;
        }
        case nori::graphics::shader_binding_type::sampler:
        {
            layout.sampler_ranges.emplace_back(CD3DX12_DESCRIPTOR_RANGE{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, binding.shader_register});
            CD3DX12_ROOT_PARAMETER parameter;
            parameter.InitAsDescriptorTable(1, &layout.sampler_ranges.back(), visibility);
            layout.root_parameters.push_back(parameter);
            break;
        }
        default:
            std::unreachable();
        }
    }
    return layout;
}

constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type(nori::graphics::primitive_topology topology)
{
    switch (topology)
    {
    case nori::graphics::primitive_topology::point_list:
    {
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    }
    case nori::graphics::primitive_topology::line_list:
    case nori::graphics::primitive_topology::line_strip:
    {
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    }
    case nori::graphics::primitive_topology::triangle_list:
    case nori::graphics::primitive_topology::triangle_strip:
    {
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
    default:
        std::unreachable();
    }
}

constexpr D3D12_COMPARISON_FUNC to_comparison(nori::graphics::comparison_function value)
{
    return static_cast<D3D12_COMPARISON_FUNC>(static_cast<std::uint32_t>(value) + 1);
}

constexpr D3D12_BLEND to_blend(nori::graphics::blend_factor value)
{
    switch (value)
    {
    case nori::graphics::blend_factor::zero:
    {
        return D3D12_BLEND_ZERO;
    }
    case nori::graphics::blend_factor::one:
    {
        return D3D12_BLEND_ONE;
    }
    case nori::graphics::blend_factor::source_alpha:
    {
        return D3D12_BLEND_SRC_ALPHA;
    }
    case nori::graphics::blend_factor::inverse_source_alpha:
    {
        return D3D12_BLEND_INV_SRC_ALPHA;
    }
    case nori::graphics::blend_factor::destination_alpha:
    {
        return D3D12_BLEND_DEST_ALPHA;
    }
    case nori::graphics::blend_factor::inverse_destination_alpha:
    {
        return D3D12_BLEND_INV_DEST_ALPHA;
    }
    default:
        std::unreachable();
    }
}

constexpr D3D12_BLEND_OP to_blend_op(nori::graphics::blend_op value)
{
    return static_cast<D3D12_BLEND_OP>(static_cast<std::uint32_t>(value) + 1);
}

constexpr D3D12_CULL_MODE to_cull_mode(nori::graphics::cull_mode value)
{
    switch (value)
    {
    case nori::graphics::cull_mode::none:
    {
        return D3D12_CULL_MODE_NONE;
    }
    case nori::graphics::cull_mode::front:
    {
        return D3D12_CULL_MODE_FRONT;
    }
    case nori::graphics::cull_mode::back:
    {
        return D3D12_CULL_MODE_BACK;
    }
    default:
        std::unreachable();
    }
}

constexpr D3D12_FILL_MODE to_fill_mode(nori::graphics::fill_mode value)
{
    switch (value)
    {
    case nori::graphics::fill_mode::solid:
    {
        return D3D12_FILL_MODE_SOLID;
    }
    case nori::graphics::fill_mode::wireframe:
    {
        return D3D12_FILL_MODE_WIREFRAME;
    }
    default:
        std::unreachable();
    }
}

constexpr BOOL is_counter_clockwise(nori::graphics::front_face value)
{
    switch (value)
    {
    case nori::graphics::front_face::clockwise:
    {
        return FALSE;
    }
    case nori::graphics::front_face::counter_clockwise:
    {
        return TRUE;
    }
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::graphics::resource
{
pipeline_manager::pipeline_manager(params args) :
    device_{args.device},
    queue_{args.queue}
{
}

shader_handle pipeline_manager::create_shader(std::span<const std::byte> bytecode)
{
    return shaders_.emplace(shader{.bytecode = std::vector{std::from_range, bytecode}});
}

void pipeline_manager::destroy(shader_handle handle)
{
    shaders_.erase(handle);
}

pipeline_handle pipeline_manager::create_pipeline(const graphics_pipeline_desc& desc)
{
    auto created{create_pipeline_resource(desc)};
    if (!created)
    {
        return {};
    }
    return pipelines_.emplace(std::move(*created));
}

void pipeline_manager::bind(pipeline_handle handle, ID3D12GraphicsCommandList* command_list)
{
    const auto& entry{pipelines_.at(handle)};
    command_list->SetGraphicsRootSignature(entry.root_signature.Get());
    command_list->SetPipelineState(entry.pipeline_state.Get());
}

void pipeline_manager::destroy(pipeline_handle handle)
{
    queue_->release(std::move(pipelines_.at(handle)));
    pipelines_.erase(handle);
}

const pipeline& pipeline_manager::at(pipeline_handle handle) const
{
    return pipelines_.at(handle);
}

std::optional<pipeline> pipeline_manager::create_pipeline_resource(const graphics_pipeline_desc& desc) const
{
    std::vector<std::string> semantics;
    semantics.reserve(desc.layout.elements.size());
    std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
    input_elements.reserve(desc.layout.elements.size());
    for (const input_element& element : desc.layout.elements)
    {
        const std::string& semantic{semantics.emplace_back(element.semantic_name)};
        input_elements.push_back({semantic.c_str(), element.semantic_index, to_dxgi_format(element.format), 0, element.offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
    }

    const pipeline_creation_layout layout{create_binding_layout(desc.bindings)};
    const CD3DX12_ROOT_SIGNATURE_DESC root_desc{
        static_cast<UINT>(layout.root_parameters.size()),
        layout.root_parameters.empty() ? nullptr : layout.root_parameters.data(),
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
    };
    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    if (FAILED(::D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors)))
    {
        return std::nullopt;
    }

    pipeline created{
        .bindings = {desc.bindings.begin(), desc.bindings.end()},
        .vertex_stride = desc.layout.stride,
        .topology = desc.topology,
    };
    if (FAILED(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&created.root_signature))))
    {
        return std::nullopt;
    }

    CD3DX12_DEPTH_STENCIL_DESC depth_stencil{D3D12_DEFAULT};
    depth_stencil.DepthEnable = desc.depth.has_value();
    depth_stencil.DepthWriteMask = desc.depth && desc.depth->write_enabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    depth_stencil.DepthFunc = to_comparison(desc.depth ? desc.depth->comparison : comparison_function::always);
    depth_stencil.StencilEnable = FALSE;

    CD3DX12_BLEND_DESC blend{D3D12_DEFAULT};
    for (std::size_t index{0}; index < desc.render_target_formats.size(); ++index)
    {
        const auto& source{desc.render_target_blends[index]};
        auto& target{blend.RenderTarget[index]};
        target.BlendEnable = source.enabled;
        target.SrcBlend = to_blend(source.source_color);
        target.DestBlend = to_blend(source.destination_color);
        target.BlendOp = to_blend_op(source.color_operation);
        target.SrcBlendAlpha = to_blend(source.source_alpha);
        target.DestBlendAlpha = to_blend(source.destination_alpha);
        target.BlendOpAlpha = to_blend_op(source.alpha_operation);
        target.RenderTargetWriteMask = static_cast<UINT8>(source.write_mask);
    }

    CD3DX12_RASTERIZER_DESC rasterizer{D3D12_DEFAULT};
    rasterizer.FillMode = to_fill_mode(desc.rasterizer.fill);
    rasterizer.CullMode = to_cull_mode(desc.rasterizer.cull);
    rasterizer.FrontCounterClockwise = is_counter_clockwise(desc.rasterizer.front);

    const shader& vs{shaders_.at(desc.vertex_shader)};
    const shader& ps{shaders_.at(desc.pixel_shader)};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{
        .pRootSignature = created.root_signature.Get(),
        .VS = {.pShaderBytecode = vs.bytecode.data(), .BytecodeLength = vs.bytecode.size()},
        .PS = {.pShaderBytecode = ps.bytecode.data(), .BytecodeLength = ps.bytecode.size()},
        .BlendState = blend,
        .SampleMask = UINT_MAX,
        .RasterizerState = rasterizer,
        .DepthStencilState = depth_stencil,
        .InputLayout = {input_elements.empty() ? nullptr : input_elements.data(), static_cast<UINT>(input_elements.size())},
        .PrimitiveTopologyType = topology_type(desc.topology),
        .NumRenderTargets = static_cast<UINT>(desc.render_target_formats.size()),
        .DSVFormat = desc.depth_stencil_format ? to_dxgi_format(*desc.depth_stencil_format) : DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = desc.sample_count}
    };
    for (std::size_t index{0}; index < desc.render_target_formats.size(); ++index)
    {
        pipeline_state_desc.RTVFormats[index] = to_dxgi_format(desc.render_target_formats[index]);
    }
    if (FAILED(device_->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&created.pipeline_state))))
    {
        return std::nullopt;
    }
    return created;
}
} // namespace nori::graphics::resource
