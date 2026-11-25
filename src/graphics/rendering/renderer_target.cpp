#include <array>
#include <utility>
#include <vector>

#include "rendering/renderer.hpp"
#include "resource/texture_manager.hpp"

namespace
{
D3D12_CPU_DESCRIPTOR_HANDLE begin_render_target(nori::graphics::resource::texture& tex, const nori::graphics::begin_op_desc& op, ID3D12GraphicsCommandList* command_list)
{
    tex.states.transition(command_list, tex.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
    switch (op.type)
    {
    case nori::graphics::begin_op::load:
    {
        break;
    }
    case nori::graphics::begin_op::clear:
    {
        const std::array clear{op.clear.color.x, op.clear.color.y, op.clear.color.z, op.clear.color.w};
        command_list->ClearRenderTargetView(tex.output_handle.cpu, clear.data(), 0, nullptr);
        break;
    }
    case nori::graphics::begin_op::discard:
    {
        command_list->DiscardResource(tex.resource.Get(), nullptr);
        break;
    }
    default:
        std::unreachable();
    }
    return tex.output_handle.cpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE begin_depth_stencil(nori::graphics::resource::texture& tex, const nori::graphics::begin_op_desc& op, ID3D12GraphicsCommandList* command_list)
{
    tex.states.transition(command_list, tex.resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, 0);
    switch (op.type)
    {
    case nori::graphics::begin_op::load:
    {
        break;
    }
    case nori::graphics::begin_op::clear:
    {
        command_list->ClearDepthStencilView(tex.output_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, op.clear.depth, 0, 0, nullptr);
        break;
    }
    case nori::graphics::begin_op::discard:
    {
        command_list->DiscardResource(tex.resource.Get(), nullptr);
        break;
    }
    default:
        std::unreachable();
    }
    return tex.output_handle.cpu;
}

void end_target(nori::graphics::resource::texture& tex, nori::graphics::end_op_desc op, ID3D12GraphicsCommandList* command_list)
{
    switch (op.type)
    {
    case nori::graphics::end_op::store:
    {
        break;
    }
    case nori::graphics::end_op::discard:
    {
        command_list->DiscardResource(tex.resource.Get(), nullptr);
        break;
    }
    default:
        std::unreachable();
    }
    const D3D12_RESOURCE_STATES final_state{(tex.usage & nori::graphics::texture_usage::shader_resource) != nori::graphics::texture_usage::none ? D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON};
    tex.states.transition(command_list, tex.resource.Get(), final_state, 0);
}
} // namespace

namespace nori::graphics::rendering
{
resource::render_target_info renderer::make_render_target_info(const render_pass_desc& desc) const
{
    resource::render_target_info info{};
    info.signature.render_target_formats.reserve(desc.render_targets.size());
    for (const auto& render_target : desc.render_targets)
    {
        const resource::texture& tex{textures_->at(render_target.target)};
        if (info.signature.render_target_formats.empty())
        {
            info.size = core::vec2{static_cast<float>(tex.width), static_cast<float>(tex.height)};
            info.signature.sample_count = tex.sample_count;
        }
        info.signature.render_target_formats.push_back(tex.format);
    }
    if (desc.depth_stencil)
    {
        const resource::texture& tex{textures_->at(desc.depth_stencil->target)};
        info.signature.depth_stencil_format = tex.format;
        if (desc.render_targets.empty())
        {
            info.size = core::vec2{static_cast<float>(tex.width), static_cast<float>(tex.height)};
            info.signature.sample_count = tex.sample_count;
        }
    }
    return info;
}

void renderer::begin_render_targets(const render_pass_desc& desc)
{
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    rtvs.reserve(desc.render_targets.size());
    for (const auto& render_target : desc.render_targets)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv{begin_render_target(textures_->at(render_target.target), render_target.begin, command_list())};
        rtvs.push_back(rtv);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
    if (desc.depth_stencil)
    {
        dsv = begin_depth_stencil(textures_->at(desc.depth_stencil->target), desc.depth_stencil->depth_begin, command_list());
    }

    command_list()->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, desc.depth_stencil ? &dsv : nullptr);
}

void renderer::end_render_targets(const render_pass_desc& desc)
{
    for (const auto& render_target : desc.render_targets)
    {
        end_target(textures_->at(render_target.target), render_target.end, command_list());
    }
    if (desc.depth_stencil)
    {
        end_target(textures_->at(desc.depth_stencil->target), desc.depth_stencil->depth_end, command_list());
    }
}
} // namespace nori::graphics::rendering
