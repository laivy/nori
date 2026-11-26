#include <ranges>
#include <utility>

#include <directx/d3dx12.h>

#include "descriptor_allocator.hpp"

namespace
{
constexpr std::uint32_t cbv_srv_uav_capacity{1024};
constexpr std::uint32_t sampler_capacity{16};
constexpr std::uint32_t rtv_capacity{32};
constexpr std::uint32_t dsv_capacity{32};
} // namespace

namespace nori::graphics
{
std::unique_ptr<descriptor_allocator> descriptor_allocator::create(ID3D12Device* device)
{
    std::array<std::uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> descriptor_sizes{};
    std::array<descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps{};
    for (auto&& [index, size] : descriptor_sizes | std::views::enumerate)
    {
        size = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(index));
    }
    const auto create_heap = [device, &heaps](D3D12_DESCRIPTOR_HEAP_TYPE type, std::uint32_t count, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    {
        descriptor_heap& heap{heaps.at(type)};
        heap.desc.Type = type;
        heap.desc.NumDescriptors = count;
        heap.desc.Flags = flags;
        heap.desc.NodeMask = 0;
        heap.free_indices.reserve(count);
        for (std::uint32_t index{count}; index > 0; --index)
        {
            heap.free_indices.push_back(index - 1);
        }
        return SUCCEEDED(device->CreateDescriptorHeap(&heap.desc, IID_PPV_ARGS(&heap.ptr)));
    };
    if (!create_heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cbv_srv_uav_capacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ||
        !create_heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sampler_capacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ||
        !create_heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv_capacity, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) ||
        !create_heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsv_capacity, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
    {
        return nullptr;
    }
    return std::unique_ptr<descriptor_allocator>{new descriptor_allocator{std::move(descriptor_sizes), std::move(heaps)}};
}

descriptor_handle descriptor_allocator::allocate(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    descriptor_heap& heap{heaps_.at(type)};
    if (heap.free_indices.empty())
    {
        return {};
    }

    const std::uint32_t index{heap.free_indices.back()};
    heap.free_indices.pop_back();

    descriptor_handle handle{};
    handle.type = type;
    handle.index = index;
    handle.cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE{heap.ptr->GetCPUDescriptorHandleForHeapStart(), static_cast<std::int32_t>(index), descriptor_sizes_[type]};
    if (heap.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE{heap.ptr->GetGPUDescriptorHandleForHeapStart(), static_cast<std::int32_t>(index), descriptor_sizes_[type]};
    }
    else
    {
        handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE{D3D12_DEFAULT};
    }
    return handle;
}

void descriptor_allocator::free(const descriptor_handle& handle) noexcept
{
    if (handle.cpu.ptr != 0)
    {
        heaps_[handle.type].free_indices.push_back(handle.index);
    }
}

void descriptor_allocator::free(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) noexcept
{
    descriptor_heap& heap{heaps_[type]};
    const std::uintptr_t heap_start{heap.ptr->GetCPUDescriptorHandleForHeapStart().ptr};
    const auto index{static_cast<std::uint32_t>((cpu_handle.ptr - heap_start) / descriptor_sizes_[type])};
    heap.free_indices.push_back(index);
}

ID3D12DescriptorHeap* descriptor_allocator::heap(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return heaps_.at(type).ptr.Get();
}

descriptor_allocator::descriptor_allocator(std::array<std::uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> descriptor_sizes, std::array<descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps) :
    descriptor_sizes_{std::move(descriptor_sizes)},
    heaps_{std::move(heaps)}
{
}
} // namespace nori::graphics
