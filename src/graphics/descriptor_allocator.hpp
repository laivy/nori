#pragma once
#include <array>
#include <optional>
#include <vector>
#include <directx/d3dx12.h>
#include <wrl/client.h>

namespace nori::graphics
{
struct descriptor_handle
{
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    INT index;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
};

struct descriptor_heap
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ptr;
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    std::vector<std::optional<descriptor_handle>> handles;
};

class descriptor_allocator
{
public:
    descriptor_allocator(ID3D12Device* device);
    ~descriptor_allocator() = default;

    descriptor_allocator(const descriptor_allocator&) = delete;
    descriptor_allocator(descriptor_allocator&&) = delete;
    descriptor_allocator& operator=(const descriptor_allocator&) = delete;
    descriptor_allocator& operator=(descriptor_allocator&&) = delete;

    descriptor_handle allocate(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void free(const descriptor_handle& handle) noexcept;

    ID3D12DescriptorHeap* heap(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

private:
    std::array<UINT, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> descriptor_sizes_;
    std::array<descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps_;
};
} // namespace nori::graphics
