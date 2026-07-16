#include <cassert>
#include <cstddef>
#include <cstdint>
#include <nori/resource/accessor.hpp>
#include "descriptor_allocator.hpp"
#include "gpu_timeline.hpp"
#include "texture_manager.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
texture_manager::texture_manager(ID3D12Device* device, descriptor_allocator* descriptor_allocator, upload_manager* upload_manager, gpu_timeline* timeline) :
    device_{device},
    descriptor_allocator_{descriptor_allocator},
    upload_manager_{upload_manager},
    timeline_{timeline}
{
    assert(device_);
    assert(descriptor_allocator_);
    assert(upload_manager_);
    assert(timeline_);
}

texture_manager::~texture_manager()
{
    flush();
}

const texture* texture_manager::get_or_create(resource::handle handle)
{
    if (textures_.contains(handle))
    {
        return textures_.at(handle).get();
    }
    const resource::image_asset& asset{resource::get_image_asset(handle)};
    std::unique_ptr<texture> texture{create(asset)};
    if (!texture)
    {
        return nullptr;
    }
    auto [it, _]{textures_.emplace(handle, std::move(texture))};
    return it->second.get();
}

void texture_manager::release(resource::handle handle)
{
    const auto it{textures_.find(handle)};
    if (it == textures_.end())
    {
        return;
    }
    defer_release(std::move(it->second));
    textures_.erase(it);
}

void texture_manager::flush()
{
    for (auto& [_, texture] : textures_)
    {
        defer_release(std::move(texture));
    }
    textures_.clear();
}

void texture_manager::defer_release(std::unique_ptr<texture> texture)
{
    if (!texture)
    {
        return;
    }
    descriptor_allocator* const descriptors{descriptor_allocator_};
    timeline_->defer(
        [descriptors, texture = std::move(texture)]() mutable
        {
            descriptors->free(texture->srv_handle);
            texture.reset();
        }
    );
}

std::unique_ptr<texture> texture_manager::create(const resource::image_asset& asset)
{
    switch (asset.img.format)
    {
    case resource::image_format::r8g8b8a8:
        break;
    default:
        assert(false && "Invalid image format");
        return nullptr;
    }

    constexpr std::uint32_t bytes_per_pixel{4};
    const std::uint32_t source_row_pitch{asset.img.width * bytes_per_pixel};
    if (asset.data.size() < static_cast<std::size_t>(source_row_pitch) * asset.img.height)
    {
        return nullptr;
    }

    auto result{std::make_unique<texture>()};
    result->availability = texture::state::unavailable;

    const auto default_heap_prop{CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT}};
    const auto default_heap_desc{
        CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, asset.img.width, asset.img.height, 1, 1)
    };
    if (FAILED(device_->CreateCommittedResource(
            &default_heap_prop,
            D3D12_HEAP_FLAG_NONE,
            &default_heap_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&result->resource)
        )))
    {
        return nullptr;
    }

    result->srv_handle = descriptor_allocator_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (result->srv_handle.cpu.ptr == 0 || result->srv_handle.gpu.ptr == 0)
    {
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(result->resource.Get(), &srv_desc, result->srv_handle.cpu);

    const D3D12_SUBRESOURCE_DATA subresource{
        .pData = asset.data.data(),
        .RowPitch = static_cast<LONG_PTR>(source_row_pitch),
        .SlicePitch = static_cast<LONG_PTR>(source_row_pitch) * asset.img.height
    };
    if (!upload_manager_->upload(
            result->resource.Get(),
            subresource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            [ptr = result.get()]()
            {
                ptr->availability = texture::state::available;
            }
        ))
    {
        descriptor_allocator_->free(result->srv_handle);
        return nullptr;
    }
    return result;
}
} // namespace nori::graphics
