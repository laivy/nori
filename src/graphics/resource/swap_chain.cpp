#include <cstddef>
#include <utility>

#include "resource/swap_chain.hpp"
#include "texture_manager.hpp"

namespace
{
constexpr std::uint32_t maximum_frame_latency{1};

bool supports_tearing(IDXGIFactory4* dxgi_factory)
{
    Microsoft::WRL::ComPtr<IDXGIFactory5> factory;
    if (FAILED(dxgi_factory->QueryInterface(IID_PPV_ARGS(factory.GetAddressOf()))))
    {
        return false;
    }

    BOOL supported{FALSE};
    return SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))) && supported;
}

void unregister_back_buffers(nori::graphics::resource::texture_manager* textures, std::array<nori::graphics::texture_handle, nori::graphics::frames_in_flight>& buffers)
{
    for (auto& handle : buffers)
    {
        if (handle)
        {
            textures->release_back_buffer(handle);
            handle = {};
        }
    }
}

nori::graphics::result<void> register_back_buffers(IDXGISwapChain4* native, nori::graphics::resource::texture_manager* textures, std::array<nori::graphics::texture_handle, nori::graphics::frames_in_flight>& buffers)
{
    for (std::size_t i{0}; i < buffers.size(); ++i)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        if (FAILED(native->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&buffer))))
        {
            unregister_back_buffers(textures, buffers);
            return std::unexpected{nori::graphics::error_code::resource_creation_failed};
        }
        const auto handle{textures->create_back_buffer(std::move(buffer))};
        if (!handle)
        {
            unregister_back_buffers(textures, buffers);
            return std::unexpected{handle.error()};
        }
        buffers[i] = *handle;
    }
    return {};
}
} // namespace

namespace nori::graphics::resource
{
struct swap_chain::params
{
    Microsoft::WRL::ComPtr<IDXGISwapChain4> native;
    texture_manager* textures;
    std::array<texture_handle, frames_in_flight> back_buffers;
    std::uint32_t back_buffer_index;
    std::uint32_t width;
    std::uint32_t height;
    HANDLE frame_latency_wait_handle;
    bool vsync;
    bool allow_tearing;
};

swap_chain::~swap_chain()
{
    release_back_buffers();
    if (frame_latency_wait_handle_)
    {
        ::CloseHandle(frame_latency_wait_handle_);
    }
}

result<std::unique_ptr<swap_chain>> swap_chain::create(swap_chain_desc desc, IDXGIFactory4* dxgi_factory, ID3D12CommandQueue* command_queue, texture_manager* textures)
{
    RECT rect{};
    if (!::GetClientRect(desc.hwnd, &rect))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    const bool allow_tearing{!desc.vsync && supports_tearing(dxgi_factory)};
    const UINT flags{DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u)};
    const DXGI_SWAP_CHAIN_DESC1 native_desc{
        .Width = static_cast<std::uint32_t>(rect.right - rect.left),
        .Height = static_cast<std::uint32_t>(rect.bottom - rect.top),
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {
            .Count = 1
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = swap_chain_buffer_count,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = flags
    };
    Microsoft::WRL::ComPtr<IDXGISwapChain1> ptr;
    if (const HRESULT hr{dxgi_factory->CreateSwapChainForHwnd(command_queue, desc.hwnd, &native_desc, nullptr, nullptr, &ptr)}; FAILED(hr))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    Microsoft::WRL::ComPtr<IDXGISwapChain4> native;
    if (const HRESULT hr{ptr.As(&native)}; FAILED(hr))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    if (const HRESULT hr{native->SetMaximumFrameLatency(maximum_frame_latency)}; FAILED(hr))
    {
        return std::unexpected{error_code::synchronization_failed};
    }

    std::array<texture_handle, frames_in_flight> back_buffers{};
    const auto cleanup = [textures, &back_buffers](void* handle)
    {
        unregister_back_buffers(textures, back_buffers);
        ::CloseHandle(handle);
    };
    std::unique_ptr<void, decltype(cleanup)> frame_latency_wait_handle{native->GetFrameLatencyWaitableObject(), cleanup};
    if (!frame_latency_wait_handle || ::WaitForSingleObject(frame_latency_wait_handle.get(), INFINITE) != WAIT_OBJECT_0)
    {
        return std::unexpected{error_code::synchronization_failed};
    }
    if (const auto res{register_back_buffers(native.Get(), textures, back_buffers)}; !res)
    {
        return std::unexpected{res.error()};
    }

    const texture& tex{textures->at(back_buffers.front())};
    const auto back_buffer_index{native->GetCurrentBackBufferIndex()};
    auto chain{std::unique_ptr<swap_chain>{new swap_chain{params{
        .native = std::move(native),
        .textures = textures,
        .back_buffers = back_buffers,
        .back_buffer_index = back_buffer_index,
        .width = tex.width,
        .height = tex.height,
        .frame_latency_wait_handle = frame_latency_wait_handle.get(),
        .vsync = desc.vsync,
        .allow_tearing = allow_tearing,
    }}}};
    frame_latency_wait_handle.release();
    return chain;
}

texture_handle swap_chain::back_buffer() const noexcept
{
    return back_buffers_[back_buffer_index_];
}

void swap_chain::retire_back_buffers()
{
    for (auto& handle : back_buffers_)
    {
        if (handle)
        {
            textures_->destroy(handle);
            handle = {};
        }
    }
}

bool swap_chain::wait_for_present_slot()
{
    if (!needs_frame_latency_wait_)
    {
        return true;
    }
    if (!frame_latency_wait_handle_)
    {
        return false;
    }
    if (::WaitForSingleObject(frame_latency_wait_handle_, INFINITE) != WAIT_OBJECT_0)
    {
        return false;
    }
    needs_frame_latency_wait_ = false;
    return true;
}

bool swap_chain::present()
{
    if (!is_usable_)
    {
        return false;
    }
    if (!wait_for_present_slot())
    {
        return false;
    }

    const UINT sync_interval{vsync_ ? 1u : 0u};
    const UINT flags{!vsync_ && allow_tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0u};
    if (FAILED(swap_chain_->Present(sync_interval, flags)))
    {
        return false;
    }

    back_buffer_index_ = swap_chain_->GetCurrentBackBufferIndex();
    needs_frame_latency_wait_ = true;
    return true;
}

result<void> swap_chain::resize(std::uint32_t width, std::uint32_t height)
{
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap_chain_->GetDesc(&desc)))
    {
        return std::unexpected{error_code::swap_chain_resize_failed};
    }

    is_usable_ = false;
    release_back_buffers();

    if (FAILED(swap_chain_->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags)))
    {
        return std::unexpected{error_code::swap_chain_invalidated};
    }

    if (!register_back_buffers(swap_chain_.Get(), textures_, back_buffers_))
    {
        return std::unexpected{error_code::swap_chain_invalidated};
    }

    const texture& tex{textures_->at(back_buffers_.front())};
    width_ = tex.width;
    height_ = tex.height;
    back_buffer_index_ = swap_chain_->GetCurrentBackBufferIndex();
    is_usable_ = true;
    return {};
}

bool swap_chain::is_usable() const noexcept
{
    return is_usable_;
}

std::uint32_t swap_chain::width() const noexcept
{
    return width_;
}

std::uint32_t swap_chain::height() const noexcept
{
    return height_;
}

swap_chain::swap_chain(params args) :
    swap_chain_{std::move(args.native)},
    textures_{args.textures},
    back_buffers_{args.back_buffers},
    back_buffer_index_{args.back_buffer_index},
    width_{args.width},
    height_{args.height},
    is_usable_{true},
    frame_latency_wait_handle_{args.frame_latency_wait_handle},
    needs_frame_latency_wait_{false},
    vsync_{args.vsync},
    allow_tearing_{args.allow_tearing}
{
}

void swap_chain::release_back_buffers()
{
    unregister_back_buffers(textures_, back_buffers_);
}
} // namespace nori::graphics::resource
