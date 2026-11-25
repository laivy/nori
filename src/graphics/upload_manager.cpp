#include <algorithm>
#include <cstdint>
#include <cstring>

#include <directx/d3dx12.h>

#include "command_queue.hpp"
#include "resource_states.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
upload_manager::upload_manager(params args) :
    device_{args.device},
    queue_{args.queue}
{
}

bool upload_manager::upload_buffer(ID3D12Resource* destination, std::uint64_t destination_offset, std::span<const std::byte> data, resource_states& states, D3D12_RESOURCE_STATES after_state)
{
    const CD3DX12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_UPLOAD};
    const CD3DX12_RESOURCE_DESC desc{CD3DX12_RESOURCE_DESC::Buffer(data.size())};
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer))))
    {
        return false;
    }
    void* mapped{};
    if (FAILED(buffer->Map(0, nullptr, &mapped)))
    {
        return false;
    }
    std::memcpy(mapped, data.data(), data.size());
    buffer->Unmap(0, nullptr);
    const auto list{queue_->record()};
    if (!list)
    {
        return false;
    }
    queue_->retain(buffer.Get());
    queue_->retain(destination);
    states.transition(list, destination, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(destination, destination_offset, buffer.Get(), 0, data.size());
    states.transition(list, destination, after_state);
    return true;
}

bool upload_manager::upload_texture(ID3D12Resource* destination, std::uint32_t subresource, std::uint32_t destination_x, std::uint32_t destination_y, std::uint32_t width, std::uint32_t height, std::uint32_t copy_row_size, std::uint32_t row_count, std::span<const std::byte> data, std::uint32_t source_row_pitch, resource_states& states, D3D12_RESOURCE_STATES after_state, std::move_only_function<void()> on_completed)
{
    const D3D12_RESOURCE_DESC destination_desc{destination->GetDesc()};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 upload_size{};
    device_->GetCopyableFootprints(
        &destination_desc,
        subresource,
        1,
        0,
        &footprint,
        nullptr,
        nullptr,
        &upload_size
    );

    const CD3DX12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_UPLOAD};
    const CD3DX12_RESOURCE_DESC desc{CD3DX12_RESOURCE_DESC::Buffer(upload_size)};
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer))))
    {
        return false;
    }

    const bool full_subresource{
        destination_x == 0 &&
        destination_y == 0 &&
        width == std::max<std::uint64_t>(1, destination_desc.Width >> subresource) &&
        height == std::max<std::uint32_t>(1, destination_desc.Height >> subresource)
    };
    if (!full_subresource)
    {
        std::byte* mapped{};
        if (FAILED(buffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped))))
        {
            return false;
        }
        for (std::uint32_t row{0}; row < row_count; ++row)
        {
            std::memcpy(
                mapped + footprint.Offset + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                data.data() + static_cast<std::size_t>(row) * source_row_pitch,
                copy_row_size
            );
        }
        buffer->Unmap(0, nullptr);
    }

    ID3D12GraphicsCommandList* const list{queue_->record()};
    if (!list)
    {
        return false;
    }
    queue_->retain(buffer.Get());
    queue_->retain(destination);

    states.transition(list, destination, D3D12_RESOURCE_STATE_COPY_DEST, subresource);
    if (full_subresource)
    {
        const D3D12_SUBRESOURCE_DATA source{
            .pData = data.data(),
            .RowPitch = static_cast<LONG_PTR>(source_row_pitch),
            .SlicePitch = static_cast<LONG_PTR>(source_row_pitch) * row_count
        };
        if (::UpdateSubresources(list, destination, buffer.Get(), 0, subresource, 1, &source) == 0)
        {
            return false;
        }
    }
    else
    {
        const CD3DX12_TEXTURE_COPY_LOCATION destination_location{destination, subresource};
        const CD3DX12_TEXTURE_COPY_LOCATION source_location{buffer.Get(), footprint};
        const D3D12_BOX source_box{0, 0, 0, width, height, 1};
        list->CopyTextureRegion(
            &destination_location,
            destination_x,
            destination_y,
            0,
            &source_location,
            &source_box
        );
    }
    states.transition(list, destination, after_state, subresource);

    queue_->on_completed(std::move(on_completed));
    return true;
}
} // namespace nori::graphics
