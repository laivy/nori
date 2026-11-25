#include <algorithm>
#include <cstring>
#include <utility>

#include <directx/d3dx12.h>

#include "command_queue.hpp"
#include "resource/buffer_manager.hpp"
#include "upload_manager.hpp"

namespace
{
constexpr D3D12_HEAP_TYPE to_heap_type(nori::graphics::buffer_access access)
{
    switch (access)
    {
    case nori::graphics::buffer_access::gpu_only:
    {
        return D3D12_HEAP_TYPE_DEFAULT;
    }
    case nori::graphics::buffer_access::cpu_write:
    {
        return D3D12_HEAP_TYPE_UPLOAD;
    }
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::graphics::resource
{
buffer_manager::buffer_manager(params args) :
    device_{args.device},
    uploads_{args.uploads},
    queue_{args.queue},
    frame_upload_buffer_pages_{}
{
}

buffer_manager::~buffer_manager() noexcept
{
    for (const auto& pages : frame_upload_buffer_pages_)
    {
        for (const auto& page : pages)
        {
            destroy(page.handle);
        }
    }
}

void buffer_manager::begin_frame()
{
    for (auto& page : frame_upload_buffer_pages_.at(queue_->frame_index()))
    {
        page.offset = 0;
    }
}

result<buffer_handle> buffer_manager::create(buffer_desc desc, std::span<const std::byte> data)
{
    const CD3DX12_HEAP_PROPERTIES heap_prop{CD3DX12_HEAP_PROPERTIES{to_heap_type(desc.access)}};
    const CD3DX12_RESOURCE_DESC resource_desc{CD3DX12_RESOURCE_DESC::Buffer(desc.size)};
    const D3D12_RESOURCE_STATES init_state{desc.access == buffer_access::cpu_write ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON};
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if (FAILED(device_->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc, init_state, nullptr, IID_PPV_ARGS(&resource))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    resource_states states{init_state};
    std::byte* mapped{};
    if (desc.access == buffer_access::cpu_write)
    {
        if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped))))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }
        if (!data.empty())
        {
            std::memcpy(mapped, data.data(), data.size());
        }
    }
    else if (!data.empty())
    {
        if (!uploads_->upload_buffer(resource.Get(), 0, data, states, D3D12_RESOURCE_STATE_GENERIC_READ))
        {
            return std::unexpected{error_code::command_failed};
        }
    }
    return buffers_.emplace(buffer{std::move(resource), desc.size, desc.usage, desc.access, mapped, std::move(states)});
}

result<frame_upload_buffer_view> buffer_manager::create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data)
{
    const bool is_constant{(usage & buffer_usage::constant) != buffer_usage::none};
    constexpr std::uint64_t constant_buffer_alignment{D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT};
    const std::uint64_t alignment{is_constant ? constant_buffer_alignment : 4};
    const std::uint64_t allocation_size{is_constant ? (data.size() + alignment - 1) / alignment * alignment : data.size()};
    const auto view{allocate_frame_upload_buffer(allocation_size, alignment)};
    if (!view)
    {
        return std::unexpected{view.error()};
    }

    std::byte* const mapped{at(view->buffer).mapped + view->offset};
    std::memcpy(mapped, data.data(), data.size());
    if (is_constant)
    {
        std::memset(mapped + data.size(), 0, static_cast<std::size_t>(allocation_size - data.size()));
    }
    return frame_upload_buffer_view{view->buffer, view->offset, data.size()};
}

result<void> buffer_manager::update(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data)
{
    buffer& buf{buffers_.at(handle)};
    if (buf.access == buffer_access::cpu_write)
    {
        std::memcpy(buf.mapped + offset, data.data(), data.size());
        return {};
    }
    if (!uploads_->upload_buffer(buf.resource.Get(), offset, data, buf.states, D3D12_RESOURCE_STATE_GENERIC_READ))
    {
        return std::unexpected{error_code::command_failed};
    }
    return {};
}

void buffer_manager::destroy(buffer_handle handle)
{
    queue_->release(std::move(buffers_.at(handle).resource));
    buffers_.erase(handle);
}

buffer& buffer_manager::at(buffer_handle handle)
{
    return buffers_.at(handle);
}

result<frame_upload_buffer_view> buffer_manager::allocate_frame_upload_buffer(std::uint64_t size, std::uint64_t alignment)
{
    std::vector<frame_upload_buffer_page>& pages{frame_upload_buffer_pages_.at(queue_->frame_index())};
    for (auto& page : pages)
    {
        const std::uint64_t offset{(page.offset + alignment - 1) / alignment * alignment};
        if (offset <= page.size && size <= page.size - offset)
        {
            page.offset = offset + size;
            return frame_upload_buffer_view{page.handle, offset, size};
        }
    }

    constexpr std::uint64_t page_size{1024 * 1024};
    constexpr std::uint64_t page_alignment{64 * 1024};
    const std::uint64_t capacity{std::max(page_size, (size + page_alignment - 1) / page_alignment * page_alignment)};
    const auto buf{create(buffer_desc{capacity, buffer_usage::vertex | buffer_usage::index | buffer_usage::constant, buffer_access::cpu_write}, {})};
    if (!buf)
    {
        return std::unexpected{buf.error()};
    }

    pages.push_back(frame_upload_buffer_page{*buf, capacity, size});
    return frame_upload_buffer_view{*buf, 0, size};
}
} // namespace nori::graphics::resource
