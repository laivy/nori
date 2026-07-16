#pragma once
#include <cstdint>
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include "descriptor_allocator.hpp"

namespace nori::graphics
{
class viewport;

class swap_chain_render_target
{
public:
    swap_chain_render_target(ID3D12Device* device, descriptor_allocator* descriptor_allocator, Microsoft::WRL::ComPtr<ID3D12Resource> color_buffer, std::uint32_t width, std::uint32_t height);
    ~swap_chain_render_target();

    swap_chain_render_target(const swap_chain_render_target&) = delete;
    swap_chain_render_target(swap_chain_render_target&&) = delete;
    swap_chain_render_target& operator=(const swap_chain_render_target&) = delete;
    swap_chain_render_target& operator=(swap_chain_render_target&&) = delete;

    void begin(ID3D12GraphicsCommandList* command_list, const viewport& viewport) const;
    void end(ID3D12GraphicsCommandList* command_list) const;

private:
    void release() noexcept;

    descriptor_allocator* const descriptor_allocator_;
    Microsoft::WRL::ComPtr<ID3D12Resource> color_buffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_stencil_;
    descriptor_handle rtv_handle_;
    descriptor_handle dsv_handle_;
};
} // namespace nori::graphics
