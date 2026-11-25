#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/buffer.hpp>
#include <nori/graphics/error_code.hpp>

#include "config.hpp"
#include "resource_states.hpp"

namespace nori::graphics
{
class command_queue;
class upload_manager;
} // namespace nori::graphics

namespace nori::graphics::resource
{
struct buffer
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    std::uint64_t size;
    buffer_usage usage;
    buffer_access access;
    std::byte* mapped;
    resource_states states;
};

class buffer_manager
{
public:
    struct params
    {
        ID3D12Device* device;
        upload_manager* uploads;
        command_queue* queue;
    };

    explicit buffer_manager(params args);
    ~buffer_manager() noexcept;

    void begin_frame();

    result<buffer_handle> create(buffer_desc desc, std::span<const std::byte> data);
    result<frame_upload_buffer_view> create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data);
    result<void> update(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data);
    void destroy(buffer_handle handle);
    buffer& at(buffer_handle handle);

private:
    struct frame_upload_buffer_page
    {
        buffer_handle handle;
        std::uint64_t size;
        std::uint64_t offset;
    };

    result<frame_upload_buffer_view> allocate_frame_upload_buffer(std::uint64_t size, std::uint64_t alignment);

    ID3D12Device* const device_;
    upload_manager* const uploads_;
    command_queue* const queue_;
    core::slot_map<buffer_handle, buffer> buffers_;
    std::array<std::vector<frame_upload_buffer_page>, frames_in_flight> frame_upload_buffer_pages_;
};
} // namespace nori::graphics::resource
