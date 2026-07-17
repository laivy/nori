#pragma once
#include <memory>
#include <unordered_map>
#include <directx/d3dx12.h>
#include <nori/resource/handle.hpp>
#include <nori/resource/image.hpp>
#include "texture.hpp"

namespace nori::graphics
{
class descriptor_allocator;
class gpu_timeline;
class upload_manager;

class texture_manager
{
public:
    texture_manager(ID3D12Device* device, descriptor_allocator* descriptor_allocator, upload_manager* upload_manager, gpu_timeline* timeline);
    ~texture_manager();

    texture_manager(const texture_manager&) = delete;
    texture_manager(texture_manager&&) = delete;
    texture_manager& operator=(const texture_manager&) = delete;
    texture_manager& operator=(texture_manager&&) = delete;

    const texture* get_or_create(resource::handle handle);
    void release(resource::handle handle);
    void flush();

private:
    std::unique_ptr<texture> create(const resource::image_asset& asset);
    void defer_release(std::unique_ptr<texture> texture);

private:
    ID3D12Device* const device_;
    descriptor_allocator* const descriptor_allocator_;
    upload_manager* const upload_manager_;
    gpu_timeline* const timeline_;
    std::unordered_map<resource::handle, std::unique_ptr<texture>> textures_;
};
} // namespace nori::graphics
