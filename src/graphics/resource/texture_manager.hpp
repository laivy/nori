#pragma once
#include <cstdint>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/resource/handle.hpp>

#include "descriptor_allocator.hpp"
#include "resource_states.hpp"

namespace nori::graphics
{
class command_queue;
class upload_manager;
} // namespace nori::graphics

namespace nori::graphics::resource
{
class texture
{
public:
    explicit texture(descriptor_allocator* descriptors);
    ~texture();

    texture(const texture&) = delete;
    texture(texture&& other) noexcept;
    texture& operator=(const texture&) = delete;
    texture& operator=(texture&& other) noexcept;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    descriptor_handle output_handle;
    descriptor_handle srv_handle;
    std::vector<descriptor_handle> mip_output_handles;
    std::vector<descriptor_handle> mip_srv_handles;
    texture_format format;
    texture_usage usage;
    resource_states states;
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t mip_levels;
    std::uint32_t sample_count;

private:
    void reset() noexcept;

    descriptor_allocator* descriptors_;
};

class texture_manager
{
public:
    struct params
    {
        ID3D12Device* device;
        descriptor_allocator* descriptors;
        upload_manager* uploads;
        command_queue* queue;
    };

    explicit texture_manager(params args);
    ~texture_manager();

    result<texture_handle> create(texture_desc desc);
    result<texture_handle> create(nori::resource::handle source);
    result<void> update(texture_handle handle, texture_region region, texture_data data);
    void destroy(texture_handle handle);

    result<texture_handle> create_back_buffer(Microsoft::WRL::ComPtr<ID3D12Resource> buffer);
    texture release_back_buffer(texture_handle handle);

    texture& at(texture_handle handle);
    D3D12_GPU_DESCRIPTOR_HANDLE srv(texture_handle handle) const;
    D3D12_GPU_DESCRIPTOR_HANDLE srv(texture_handle handle, std::uint16_t mip_level) const;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv(texture_handle handle, std::uint16_t mip_level) const;

private:
    const texture& at(texture_handle handle) const;

    ID3D12Device* const device_;
    descriptor_allocator* const descriptors_;
    upload_manager* const uploads_;
    command_queue* const queue_;
    core::slot_map<texture_handle, texture> textures_;
};
} // namespace nori::graphics::resource
