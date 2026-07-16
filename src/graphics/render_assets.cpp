#include "render_assets.hpp"

namespace nori::graphics
{
render_assets::render_assets(ID3D12Device* device, descriptor_allocator* descriptor_allocator, upload_manager* upload_manager, gpu_timeline* timeline) :
    texture_manager_{device, descriptor_allocator, upload_manager, timeline}
{
}

D3D12_GPU_DESCRIPTOR_HANDLE render_assets::resolve_texture_srv(resource::handle handle)
{
    const texture* const texture{texture_manager_.get_or_create(handle)};
    if (!texture)
    {
        return {};
    }
    if (texture->availability != texture::state::available)
    {
        return {};
    }
    return texture->srv_handle.gpu;
}

void render_assets::release_texture(resource::handle handle)
{
    texture_manager_.release(handle);
}
} // namespace nori::graphics
