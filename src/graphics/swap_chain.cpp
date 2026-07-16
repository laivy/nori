#include <algorithm>
#include <cassert>
#include "swap_chain.hpp"
#include "swap_chain_render_target.hpp"

namespace nori::graphics
{
swap_chain::swap_chain(HWND hwnd, IDXGIFactory4* dxgi_factory, ID3D12Device* device, ID3D12CommandQueue* command_queue, descriptor_allocator* descriptor_allocator) :
    device_{device},
    descriptor_allocator_{descriptor_allocator},
    back_buffers_{},
    frame_index_{},
    width_{},
    height_{}
{
    assert(hwnd);
    assert(dxgi_factory);
    assert(device_);
    assert(command_queue);
    assert(descriptor_allocator_);
    RECT rect{};
    if (!::GetClientRect(hwnd, &rect))
    {
        assert(false && "failed to read swap chain client rect");
    }
    width_ = static_cast<std::uint32_t>(std::max<LONG>(1, rect.right - rect.left));
    height_ = static_cast<std::uint32_t>(std::max<LONG>(1, rect.bottom - rect.top));

    const DXGI_SWAP_CHAIN_DESC1 desc{
        .Width = width_,
        .Height = height_,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {
            .Count = 1
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = frames_in_flight,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD
    };
    Microsoft::WRL::ComPtr<IDXGISwapChain1> ptr;
    if (const HRESULT result{dxgi_factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, nullptr, nullptr, &ptr)}; FAILED(result))
    {
        assert(false && "failed to create DXGI swap chain");
    }
    if (const HRESULT result{ptr.As(&ptr_)}; FAILED(result))
    {
        assert(false && "failed to query IDXGISwapChain4");
    }
    frame_index_ = ptr_->GetCurrentBackBufferIndex();
    if (const auto result{create_render_targets()}; !result)
    {
        assert(false && "failed to create swap chain render targets");
    }
}

swap_chain::~swap_chain()
{
}

result<Microsoft::WRL::ComPtr<ID3D12Resource>> swap_chain::back_buffer(std::uint32_t index) const
{
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    assert(index < frames_in_flight);
    if (FAILED(ptr_->GetBuffer(index, IID_PPV_ARGS(&buffer))))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    return buffer;
}

swap_chain_render_target& swap_chain::current_render_target() const
{
    return *back_buffers_.at(frame_index_);
}

result<void> swap_chain::present()
{
    if (FAILED(ptr_->Present(1, 0)))
    {
        return std::unexpected{error_code::presentation_failed};
    }
    frame_index_ = ptr_->GetCurrentBackBufferIndex();
    return {};
}

result<void> swap_chain::resize(std::uint32_t width, std::uint32_t height)
{
    width = std::max(1u, width);
    height = std::max(1u, height);
    if (width == width_ && height == height_)
    {
        return {};
    }

    for (auto& back_buffer : back_buffers_)
    {
        back_buffer.reset();
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(ptr_->GetDesc(&desc)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    if (FAILED(ptr_->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    width_ = width;
    height_ = height;
    frame_index_ = ptr_->GetCurrentBackBufferIndex();
    return create_render_targets();
}

result<void> swap_chain::create_render_targets()
{
    for (std::uint32_t i{0}; i < frames_in_flight; ++i)
    {
        auto buffer{back_buffer(i)};
        if (!buffer)
        {
            return std::unexpected{buffer.error()};
        }
        back_buffers_.at(i) = std::make_unique<swap_chain_render_target>(device_, descriptor_allocator_, std::move(*buffer), width_, height_);
    }
    return {};
}

std::uint32_t swap_chain::frame_index() const noexcept
{
    return frame_index_;
}

std::uint32_t swap_chain::width() const noexcept
{
    return width_;
}

std::uint32_t swap_chain::height() const noexcept
{
    return height_;
}
} // namespace nori::graphics
