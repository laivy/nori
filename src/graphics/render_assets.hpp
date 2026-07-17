#pragma once
#include <directx/d3dx12.h>
#include <nori/resource/handle.hpp>
#include "texture_manager.hpp"

namespace nori::graphics
{
class descriptor_allocator;
class gpu_timeline;
class upload_manager;

class render_assets
{
public:
    render_assets(ID3D12Device* device, descriptor_allocator* descriptor_allocator, upload_manager* upload_manager, gpu_timeline* timeline);

    render_assets(const render_assets&) = delete;
    render_assets(render_assets&&) = delete;
    render_assets& operator=(const render_assets&) = delete;
    render_assets& operator=(render_assets&&) = delete;

    D3D12_GPU_DESCRIPTOR_HANDLE resolve_texture_srv(resource::handle handle);
    void release_texture(resource::handle handle);

private:
    texture_manager texture_manager_;
};
} // namespace nori::graphics
