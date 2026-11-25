#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

namespace nori::graphics
{
struct descriptor_handle
{
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    std::uint32_t index;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
};

class descriptor_allocator
{
public:
    descriptor_allocator(const descriptor_allocator&) = delete;
    descriptor_allocator(descriptor_allocator&&) = delete;
    descriptor_allocator& operator=(const descriptor_allocator&) = delete;
    descriptor_allocator& operator=(descriptor_allocator&&) = delete;

    static std::unique_ptr<descriptor_allocator> create(ID3D12Device* device);

    descriptor_handle allocate(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void free(const descriptor_handle& handle) noexcept;
    void free(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) noexcept;

    ID3D12DescriptorHeap* heap(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

private:
    struct descriptor_heap
    {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ptr;
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        std::vector<std::uint32_t> free_indices;
    };

    descriptor_allocator(std::array<std::uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> descriptor_sizes, std::array<descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps);

    std::array<std::uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> descriptor_sizes_;
    std::array<descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps_;
};
} // namespace nori::graphics
