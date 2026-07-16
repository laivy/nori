#include <algorithm>
#include <cassert>
#include <ranges>
#include "descriptor_allocator.hpp"

namespace nori::graphics
{
descriptor_allocator::descriptor_allocator(ID3D12Device* device) :
    descriptor_sizes_{},
    heaps_{}
{
    assert(device);
    for (auto [i, size] : descriptor_sizes_ | std::views::enumerate)
    {
        size = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
    }
    auto create_heap =
        [this](ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    {
        descriptor_heap& heap{heaps_.at(type)};
        heap.desc.Type = type;
        heap.desc.NumDescriptors = count;
        heap.desc.Flags = flags;
        heap.desc.NodeMask = 0;
        heap.handles.resize(count);
        return SUCCEEDED(device->CreateDescriptorHeap(&heap.desc, IID_PPV_ARGS(&heap.ptr)));
    };
    if (!create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ||
        !create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ||
        !create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) ||
        !create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
    {
        assert(false && "failed to create descriptor heaps");
    }
}

descriptor_handle descriptor_allocator::allocate(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    descriptor_heap& heap{heaps_.at(type)};
    assert(heap.ptr);
    const auto it{std::ranges::find_if(
        heap.handles,
        [](const auto& handle)
        {
            return !handle.has_value();
        }
    )};
    if (it == heap.handles.end())
    {
        return {};
    }

    descriptor_handle& handle{it->emplace()};
    handle.type = type;
    handle.index = static_cast<INT>(std::distance(heap.handles.begin(), it));
    handle.cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE{
        heap.ptr->GetCPUDescriptorHandleForHeapStart(), handle.index, descriptor_sizes_[type]
    };
    if (heap.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            heap.ptr->GetGPUDescriptorHandleForHeapStart(), handle.index, descriptor_sizes_[type]
        };
    }
    else
    {
        handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE{D3D12_DEFAULT};
    }
    return handle;
}

void descriptor_allocator::free(const descriptor_handle& handle) noexcept
{
    if (handle.cpu.ptr == 0)
    {
        return;
    }
    if (handle.type < 0 || handle.type >= D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)
    {
        return;
    }
    descriptor_heap& heap{heaps_[handle.type]};
    if (!heap.ptr)
    {
        return;
    }
    if (handle.index < 0 || static_cast<std::size_t>(handle.index) >= heap.handles.size())
    {
        return;
    }
    heap.handles[handle.index].reset();
}

ID3D12DescriptorHeap* descriptor_allocator::heap(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return heaps_.at(type).ptr.Get();
}
} // namespace nori::graphics
