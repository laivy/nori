#pragma once
#include <cstdint>
#include <vector>
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/render_target.hpp>
#include "descriptor_allocator.hpp"

namespace nori::graphics
{
class gpu_timeline;

struct offscreen_render_target
{
    std::vector<render_texture_handle> color_attachments;
    std::optional<render_texture_handle> depth_attachment;
    std::uint32_t width;
    std::uint32_t height;
};

struct render_texture_resource
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    descriptor_handle output_handle;
    descriptor_handle srv_handle;
    render_texture_format format;
    D3D12_RESOURCE_STATES state;
    std::uint32_t width;
    std::uint32_t height;
};

class render_target_manager
{
public:
    render_target_manager(ID3D12Device* device, descriptor_allocator* descriptor_allocator, gpu_timeline* timeline);
    ~render_target_manager();

    result<render_target_handle> create_target(const render_target_specification& spec);
    result<void> destroy(render_target_handle handle);
    result<void> begin(render_target_handle handle, ID3D12GraphicsCommandList* command_list);
    result<void> end(render_target_handle handle, ID3D12GraphicsCommandList* command_list);
    result<render_texture_handle> create_texture(render_texture_specification spec);
    result<void> destroy(render_texture_handle handle);
    result<D3D12_GPU_DESCRIPTOR_HANDLE> shader_resource(render_texture_handle handle) const;
    bool is_attachment(render_target_handle target, render_texture_handle texture) const;
    result<std::vector<render_texture_format>> color_formats(render_target_handle target) const;

private:
    offscreen_render_target& resolve(render_target_handle handle);
    const offscreen_render_target& resolve(render_target_handle handle) const;
    render_texture_resource& resolve(render_texture_handle handle);
    const render_texture_resource& resolve(render_texture_handle handle) const;
    void release_texture(render_texture_resource& texture);
    void release_texture_immediate(render_texture_resource& texture) noexcept;

    ID3D12Device* const device_;
    descriptor_allocator* const descriptor_allocator_;
    gpu_timeline* const timeline_;
    core::slot_map<render_target_handle, offscreen_render_target> targets_;
    core::slot_map<render_texture_handle, render_texture_resource> textures_;
};
} // namespace nori::graphics
