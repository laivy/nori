#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_set>
#include "gpu_timeline.hpp"
#include "render_target_manager.hpp"

namespace nori::graphics
{
namespace
{
bool is_depth(render_texture_format format)
{
    return format == render_texture_format::d32_float;
}

DXGI_FORMAT resource_format(render_texture_format format)
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
        return DXGI_FORMAT_R32_TYPELESS;
    }
    return DXGI_FORMAT_UNKNOWN;
}
} // namespace

render_target_manager::render_target_manager(ID3D12Device* device, descriptor_allocator* descriptor_allocator, gpu_timeline* timeline) :
    device_{device},
    descriptor_allocator_{descriptor_allocator},
    timeline_{timeline},
    targets_{},
    textures_{}
{
    assert(device_);
    assert(descriptor_allocator_);
    assert(timeline_);
}

render_target_manager::~render_target_manager()
{
    for (auto& [_, texture] : textures_)
    {
        release_texture_immediate(texture);
    }
}

result<render_target_handle> render_target_manager::create_target(const render_target_specification& spec)
{
    if (spec.color_attachments.size() > 8 || (spec.color_attachments.empty() && !spec.depth_attachment))
    {
        return std::unexpected{error_code::invalid_argument};
    }
    std::unordered_set<render_texture_handle> unique;
    std::uint32_t width{0};
    std::uint32_t height{0};
    auto validate = [this, &unique, &width, &height](render_texture_handle handle, bool depth) -> result<void>
    {
        if (!textures_.contains(handle))
        {
            return std::unexpected{error_code::invalid_handle};
        }
        const auto& value{resolve(handle)};
        if (is_depth(value.format) != depth)
        {
            return std::unexpected{error_code::invalid_argument};
        }
        if (!unique.insert(handle).second)
        {
            return std::unexpected{error_code::invalid_argument};
        }
        if (width == 0)
        {
            width = value.width;
            height = value.height;
        }
        else
        {
            if (width != value.width || height != value.height)
            {
                return std::unexpected{error_code::invalid_argument};
            }
        }
        return {};
    };
    for (auto handle : spec.color_attachments)
    {
        if (auto result{validate(handle, false)}; !result)
        {
            return std::unexpected{result.error()};
        }
    }
    if (spec.depth_attachment)
    {
        if (auto result{validate(*spec.depth_attachment, true)}; !result)
        {
            return std::unexpected{result.error()};
        }
    }
    return targets_.emplace(offscreen_render_target{spec.color_attachments, spec.depth_attachment, width, height});
}

result<void> render_target_manager::destroy(render_target_handle handle)
{
    if (!targets_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    targets_.erase(handle);
    return {};
}

result<void> render_target_manager::begin(render_target_handle handle, ID3D12GraphicsCommandList* command_list)
{
    if (!targets_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    auto& value{resolve(handle)};
    assert(command_list);
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    for (auto texture_handle : value.color_attachments)
    {
        auto& item{resolve(texture_handle)};
        const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(item.resource.Get(), item.state, D3D12_RESOURCE_STATE_RENDER_TARGET)};
        command_list->ResourceBarrier(1, &barrier);
        item.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        rtvs.push_back(item.output_handle.cpu);
        constexpr std::array clear{0.0f, 0.0f, 0.0f, 0.0f};
        command_list->ClearRenderTargetView(item.output_handle.cpu, clear.data(), 0, nullptr);
    }
    D3D12_CPU_DESCRIPTOR_HANDLE* dsv{};
    if (value.depth_attachment)
    {
        auto& item{resolve(*value.depth_attachment)};
        const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(item.resource.Get(), item.state, D3D12_RESOURCE_STATE_DEPTH_WRITE)};
        command_list->ResourceBarrier(1, &barrier);
        item.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        dsv = &item.output_handle.cpu;
        command_list->ClearDepthStencilView(*dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
    command_list->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, dsv);
    const CD3DX12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(value.width), static_cast<float>(value.height)};
    const CD3DX12_RECT scissor{0, 0, static_cast<LONG>(value.width), static_cast<LONG>(value.height)};
    command_list->RSSetViewports(1, &viewport);
    command_list->RSSetScissorRects(1, &scissor);
    return {};
}

result<void> render_target_manager::end(render_target_handle handle, ID3D12GraphicsCommandList* command_list)
{
    if (!targets_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    auto& value{resolve(handle)};
    assert(command_list);
    auto transition = [&](render_texture_handle handle)
    {
        auto& item{resolve(handle)};
        const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(item.resource.Get(), item.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
        command_list->ResourceBarrier(1, &barrier);
        item.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    };
    for (auto item : value.color_attachments)
    {
        transition(item);
    }
    if (value.depth_attachment)
    {
        transition(*value.depth_attachment);
    }
    return {};
}

result<render_texture_handle> render_target_manager::create_texture(render_texture_specification spec)
{
    if (spec.width == 0 || spec.height == 0)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    auto result{std::make_unique<render_texture_resource>()};
    result->format = spec.format;
    result->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    result->width = spec.width;
    result->height = spec.height;
    const bool depth{is_depth(spec.format)};
    const DXGI_FORMAT format{resource_format(spec.format)};
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    const auto heap{CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT}};
    const auto desc{CD3DX12_RESOURCE_DESC::Tex2D(format, spec.width, spec.height, 1, 1, 1, 0, depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)};
    const D3D12_CLEAR_VALUE clear{depth ? D3D12_CLEAR_VALUE{.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f}} : D3D12_CLEAR_VALUE{.Format = format, .Color = {0, 0, 0, 0}}};
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, result->state, &clear, IID_PPV_ARGS(&result->resource))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    result->output_handle = descriptor_allocator_->allocate(depth ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    result->srv_handle = descriptor_allocator_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (result->output_handle.cpu.ptr == 0 || result->srv_handle.cpu.ptr == 0)
    {
        release_texture_immediate(*result);
        return std::unexpected{error_code::descriptor_allocation_failed};
    }
    if (depth)
    {
        const D3D12_DEPTH_STENCIL_VIEW_DESC view{.Format = DXGI_FORMAT_D32_FLOAT, .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D};
        device_->CreateDepthStencilView(result->resource.Get(), &view, result->output_handle.cpu);
    }
    else
    {
        device_->CreateRenderTargetView(result->resource.Get(), nullptr, result->output_handle.cpu);
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = depth ? DXGI_FORMAT_R32_FLOAT : format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(result->resource.Get(), &srv, result->srv_handle.cpu);
    return textures_.emplace(std::move(*result));
}

result<void> render_target_manager::destroy(render_texture_handle handle)
{
    for (const auto& [_, target] : targets_)
    {
        if (target.depth_attachment == handle || std::ranges::contains(target.color_attachments, handle))
        {
            return std::unexpected{error_code::resource_in_use};
        }
    }
    if (!textures_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    release_texture(textures_.get(handle));
    textures_.erase(handle);
    return {};
}

result<D3D12_GPU_DESCRIPTOR_HANDLE> render_target_manager::shader_resource(render_texture_handle handle) const
{
    if (!textures_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    const auto& item{resolve(handle)};
    if (item.state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return item.srv_handle.gpu;
}

bool render_target_manager::is_attachment(render_target_handle handle, render_texture_handle texture_handle) const
{
    if (!targets_.contains(handle))
    {
        return false;
    }
    const auto& value{resolve(handle)};
    return value.depth_attachment == texture_handle || std::ranges::contains(value.color_attachments, texture_handle);
}

result<std::vector<render_texture_format>> render_target_manager::color_formats(render_target_handle handle) const
{
    if (!targets_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    std::vector<render_texture_format> result;
    for (render_texture_handle texture_handle : resolve(handle).color_attachments)
    {
        result.push_back(resolve(texture_handle).format);
    }
    return result;
}

offscreen_render_target& render_target_manager::resolve(render_target_handle handle)
{
    assert(targets_.contains(handle));
    return targets_.get(handle);
}

const offscreen_render_target& render_target_manager::resolve(render_target_handle handle) const
{
    assert(targets_.contains(handle));
    return targets_.get(handle);
}

render_texture_resource& render_target_manager::resolve(render_texture_handle handle)
{
    assert(textures_.contains(handle));
    return textures_.get(handle);
}

const render_texture_resource& render_target_manager::resolve(render_texture_handle handle) const
{
    assert(textures_.contains(handle));
    return textures_.get(handle);
}

void render_target_manager::release_texture(render_texture_resource& texture)
{
    descriptor_allocator* const descriptors{descriptor_allocator_};
    auto resource{std::move(texture.resource)};
    const descriptor_handle output{texture.output_handle};
    const descriptor_handle srv{texture.srv_handle};
    texture.output_handle = {};
    texture.srv_handle = {};
    timeline_->defer(
        [descriptors, resource = std::move(resource), output, srv]() mutable
        {
            descriptors->free(output);
            descriptors->free(srv);
            resource.Reset();
        }
    );
}

void render_target_manager::release_texture_immediate(render_texture_resource& texture) noexcept
{
    descriptor_allocator_->free(texture.output_handle);
    descriptor_allocator_->free(texture.srv_handle);
    texture.output_handle = {};
    texture.srv_handle = {};
    texture.resource.Reset();
}
} // namespace nori::graphics
