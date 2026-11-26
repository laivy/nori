#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include <directx/d3d12.h>

namespace nori::graphics
{
class command_queue;
class resource_states;

class upload_manager
{
public:
    struct params
    {
        ID3D12Device* device;
        command_queue* queue;
    };

    explicit upload_manager(params args);

    upload_manager(const upload_manager&) = delete;
    upload_manager(upload_manager&&) = delete;
    upload_manager& operator=(const upload_manager&) = delete;
    upload_manager& operator=(upload_manager&&) = delete;

    bool upload_buffer(ID3D12Resource* destination, std::uint64_t destination_offset, std::span<const std::byte> data, resource_states& states, D3D12_RESOURCE_STATES after_state);
    bool upload_texture(ID3D12Resource* destination, std::uint32_t subresource, std::uint32_t destination_x, std::uint32_t destination_y, std::uint32_t width, std::uint32_t height, std::uint32_t copy_row_size, std::uint32_t row_count, std::span<const std::byte> data, std::uint32_t source_row_pitch, resource_states& states, D3D12_RESOURCE_STATES after_state, std::move_only_function<void()> on_completed);

private:
    ID3D12Device* const device_;
    command_queue* const queue_;
};
} // namespace nori::graphics
