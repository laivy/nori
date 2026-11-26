#pragma once
#include <array>
#include <cstdint>
#include <memory>

#include <Windows.h>
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <nori/graphics/error_code.hpp>
#include <nori/graphics/swap_chain.hpp>
#include <nori/graphics/texture.hpp>

#include "config.hpp"

namespace nori::graphics::resource
{
class texture_manager;

class swap_chain
{
public:
    ~swap_chain();

    swap_chain(const swap_chain&) = delete;
    swap_chain(swap_chain&&) = delete;
    swap_chain& operator=(const swap_chain&) = delete;
    swap_chain& operator=(swap_chain&&) = delete;

    static result<std::unique_ptr<swap_chain>> create(swap_chain_desc desc, IDXGIFactory4* dxgi_factory, ID3D12CommandQueue* command_queue, texture_manager* textures);

    [[nodiscard]] texture_handle back_buffer() const noexcept;
    void retire_back_buffers();
    bool wait_for_present_slot();
    bool present();
    result<void> resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] bool is_usable() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

private:
    struct params;

    explicit swap_chain(params args);
    void release_back_buffers();

    static constexpr std::uint32_t swap_chain_buffer_count{frames_in_flight};

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_chain_;
    texture_manager* const textures_;

    std::array<texture_handle, frames_in_flight> back_buffers_;
    std::uint32_t back_buffer_index_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool is_usable_;

    HANDLE frame_latency_wait_handle_;
    bool needs_frame_latency_wait_;

    bool vsync_;
    bool allow_tearing_;
};
} // namespace nori::graphics::resource
