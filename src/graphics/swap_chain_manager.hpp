#pragma once
#include <memory>
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/render_target.hpp>
#include "swap_chain.hpp"

namespace nori::graphics
{
class descriptor_allocator;
class gpu_timeline;

class swap_chain_manager
{
public:
    swap_chain_manager(IDXGIFactory4* dxgi_factory, ID3D12Device* device, ID3D12CommandQueue* command_queue, descriptor_allocator* descriptor_allocator, gpu_timeline* timeline);
    ~swap_chain_manager();

    result<swap_chain_handle> create(swap_chain_specification spec);
    result<void> destroy(swap_chain_handle handle);
    result<void> resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height);
    result<void> present(swap_chain_handle handle);
    result<void> begin(swap_chain_handle handle, ID3D12GraphicsCommandList* command_list);
    result<void> end(swap_chain_handle handle, ID3D12GraphicsCommandList* command_list);

private:
    [[nodiscard]]
    swap_chain* get(swap_chain_handle handle);

    IDXGIFactory4* const dxgi_factory_;
    ID3D12Device* const device_;
    ID3D12CommandQueue* const command_queue_;
    descriptor_allocator* const descriptor_allocator_;
    gpu_timeline* const timeline_;
    core::slot_map<swap_chain_handle, std::unique_ptr<swap_chain>> swap_chains_;
};
} // namespace nori::graphics
