#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <Windows.h>
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <nori/graphics/error_code.hpp>
#include "constants.hpp"

namespace nori::graphics
{
class descriptor_allocator;
class swap_chain_render_target;

class swap_chain
{
public:
    swap_chain(HWND hwnd, IDXGIFactory4* dxgi_factory, ID3D12Device* device, ID3D12CommandQueue* command_queue, descriptor_allocator* descriptor_allocator);
    ~swap_chain();

    swap_chain(const swap_chain&) = delete;
    swap_chain(swap_chain&&) = delete;
    swap_chain& operator=(const swap_chain&) = delete;
    swap_chain& operator=(swap_chain&&) = delete;

    [[nodiscard]]
    swap_chain_render_target& current_render_target() const;
    result<void> present();
    result<void> resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]]
    std::uint32_t frame_index() const noexcept;
    [[nodiscard]]
    std::uint32_t width() const noexcept;
    [[nodiscard]]
    std::uint32_t height() const noexcept;

private:
    [[nodiscard]]
    result<Microsoft::WRL::ComPtr<ID3D12Resource>> back_buffer(std::uint32_t index) const;
    result<void> create_render_targets();

private:
    Microsoft::WRL::ComPtr<IDXGISwapChain4> ptr_;
    ID3D12Device* const device_;
    descriptor_allocator* const descriptor_allocator_;
    std::array<std::unique_ptr<swap_chain_render_target>, frames_in_flight> back_buffers_;
    std::uint32_t frame_index_;
    std::uint32_t width_;
    std::uint32_t height_;
};
} // namespace nori::graphics
