#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <optional>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/transfer.hpp>

namespace nori::graphics::resource
{
class buffer_manager;
class texture_manager;
} // namespace nori::graphics::resource

namespace nori::graphics
{
class command_queue;
class descriptor_allocator;
struct command_submission;
} // namespace nori::graphics

namespace nori::graphics::transfer
{
struct readback
{
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    std::shared_ptr<command_submission> submission;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    texture_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t bytes_per_pixel;
    std::optional<texture_readback> value;
};

class manager
{
public:
    struct params
    {
        ID3D12Device* device;
        descriptor_allocator* descriptors;
        command_queue* queue;
        resource::buffer_manager* buffers;
        resource::texture_manager* textures;
    };

    explicit manager(params args);

    result<void> copy_buffer(buffer_handle source_handle, std::uint64_t source_offset, buffer_handle destination_handle, std::uint64_t destination_offset, std::uint64_t size);
    result<void> copy_texture(texture_handle source_handle, texture_region source_region, texture_handle destination_handle, texture_region destination_region);
    result<void> blit_texture(texture_handle source_handle, texture_region source_region, texture_handle destination_handle, texture_region destination_region, blit_filter filter);
    result<void> generate_mips(texture_handle handle);

    result<readback_handle> read_texture(texture_handle handle, texture_region region);
    readback_result poll(readback_handle handle);
    void destroy(readback_handle handle);

private:
    struct blit_pipeline
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
    };

    result<blit_pipeline*> get_blit_pipeline(texture_format format, blit_filter filter);

    ID3D12Device* const device_;
    descriptor_allocator* const descriptors_;
    command_queue* const queue_;
    resource::buffer_manager* const buffers_;
    resource::texture_manager* const textures_;
    std::array<std::optional<blit_pipeline>, (static_cast<std::size_t>(texture_format::bc7_unorm_srgb) + 1) * 2> blit_pipelines_;
    core::slot_map<readback_handle, readback> readbacks_;
};
} // namespace nori::graphics::transfer
