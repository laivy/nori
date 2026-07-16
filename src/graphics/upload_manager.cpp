#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include "gpu_timeline.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
upload_manager::upload_manager(ID3D12Device* device, ID3D12CommandQueue* command_queue, gpu_timeline* timeline) :
    device_{device},
    command_queue_{command_queue},
    timeline_{timeline}
{
    assert(device_);
    assert(command_queue_);
    assert(timeline_);
}

upload_manager::~upload_manager()
{
    flush();
}

bool upload_manager::upload(ID3D12Resource* destination, const D3D12_SUBRESOURCE_DATA& subresource, D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state, const std::function<void()>& on_completed)
{
    collect();
    assert(destination);

    const UINT64 upload_size{::GetRequiredIntermediateSize(destination, 0, 1)};
    const auto upload_heap_prop{CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD}};
    const auto upload_heap_desc{CD3DX12_RESOURCE_DESC::Buffer(upload_size)};
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    if (FAILED(device_->CreateCommittedResource(
            &upload_heap_prop,
            D3D12_HEAP_FLAG_NONE,
            &upload_heap_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&buffer)
        )))
    {
        return false;
    }

    command_context command_context{acquire_command_context()};
    if (!command_context.command_allocator || !command_context.command_list)
    {
        return false;
    }

    ::UpdateSubresources(command_context.command_list.Get(), destination, buffer.Get(), 0, 0, 1, &subresource);
    if (before_state != after_state)
    {
        const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(destination, before_state, after_state)};
        command_context.command_list->ResourceBarrier(1, &barrier);
    }
    if (FAILED(command_context.command_list->Close()))
    {
        return false;
    }

    const auto command_lists{std::to_array<ID3D12CommandList*>({command_context.command_list.Get()})};
    command_queue_->ExecuteCommandLists(static_cast<UINT>(command_lists.size()), command_lists.data());

    std::uint64_t fence_value{0};
    if (!timeline_->signal(fence_value))
    {
        return false;
    }

    pending_uploads_.emplace_back(
        upload_info{
            .buffer = buffer,
            .command_context = std::move(command_context),
            .fence_value = fence_value,
            .on_completed = std::move(on_completed)
        }
    );
    return true;
}

void upload_manager::collect()
{
    const std::uint64_t completed_value{timeline_->completed_value()};
    std::erase_if(
        pending_uploads_,
        [this, completed_value](upload_info& pending)
        {
            if (pending.fence_value > completed_value)
            {
                return false;
            }
            command_contexts_.push(std::move(pending.command_context));
            if (pending.on_completed)
            {
                pending.on_completed();
            }
            return true;
        }
    );
}

bool upload_manager::flush()
{
    if (pending_uploads_.empty())
    {
        return true;
    }
    const auto it{std::ranges::max_element(pending_uploads_, {}, &upload_info::fence_value)};
    const std::uint64_t fence_value{it->fence_value};
    if (fence_value > timeline_->completed_value())
    {
        if (!timeline_->wait(fence_value))
        {
            return false;
        }
    }
    collect();
    return true;
}

upload_manager::command_context upload_manager::acquire_command_context()
{
    command_context command_context;
    if (command_contexts_.empty())
    {
        if (FAILED(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_context.command_allocator)
            )))
        {
            return {};
        }
        if (FAILED(device_->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                command_context.command_allocator.Get(),
                nullptr,
                IID_PPV_ARGS(&command_context.command_list)
            )))
        {
            return {};
        }
    }
    else
    {
        command_context = std::move(command_contexts_.front());
        command_contexts_.pop();
        if (FAILED(command_context.command_allocator->Reset()))
        {
            return {};
        }
        if (FAILED(command_context.command_list->Reset(command_context.command_allocator.Get(), nullptr)))
        {
            return {};
        }
    }
    return command_context;
}
} // namespace nori::graphics
