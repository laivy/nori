#include <array>
#include <cassert>
#include "swap_chain_render_target.hpp"
#include "viewport.hpp"

namespace nori::graphics
{
swap_chain_render_target::swap_chain_render_target(ID3D12Device* device, descriptor_allocator* descriptor_allocator, Microsoft::WRL::ComPtr<ID3D12Resource> color_buffer, std::uint32_t width, std::uint32_t height) :
    descriptor_allocator_{descriptor_allocator},
    color_buffer_{std::move(color_buffer)},
    depth_stencil_{},
    rtv_handle_{},
    dsv_handle_{}
{
    assert(device);
    assert(descriptor_allocator_);
    assert(color_buffer_);
    rtv_handle_ = descriptor_allocator_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    assert(rtv_handle_.cpu.ptr != 0);
    device->CreateRenderTargetView(color_buffer_.Get(), nullptr, rtv_handle_.cpu);

    const D3D12_HEAP_PROPERTIES heap_properties{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC resource_desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = {.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };
    const D3D12_CLEAR_VALUE clear_value{
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .DepthStencil = {.Depth = 1.0f, .Stencil = 0}
    };
    if (const HRESULT result{device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&depth_stencil_))}; FAILED(result))
    {
        assert(false && "failed to create render target depth stencil");
    }
    dsv_handle_ = descriptor_allocator_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    assert(dsv_handle_.cpu.ptr != 0);
    const D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D
    };
    device->CreateDepthStencilView(depth_stencil_.Get(), &dsv_desc, dsv_handle_.cpu);
}

swap_chain_render_target::~swap_chain_render_target()
{
    release();
}

void swap_chain_render_target::begin(ID3D12GraphicsCommandList* command_list, const viewport& viewport) const
{
    assert(command_list);
    const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(color_buffer_.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)};
    command_list->ResourceBarrier(1, &barrier);
    command_list->OMSetRenderTargets(1, &rtv_handle_.cpu, FALSE, &dsv_handle_.cpu);
    viewport.apply(command_list);

    constexpr std::array clear_color{0.15625f, 0.171875f, 0.203125f, 1.0f};
    command_list->ClearRenderTargetView(rtv_handle_.cpu, clear_color.data(), 0, nullptr);
    command_list->ClearDepthStencilView(dsv_handle_.cpu, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void swap_chain_render_target::end(ID3D12GraphicsCommandList* command_list) const
{
    assert(command_list);
    const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(color_buffer_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)};
    command_list->ResourceBarrier(1, &barrier);
}

void swap_chain_render_target::release() noexcept
{
    color_buffer_.Reset();
    depth_stencil_.Reset();
    descriptor_allocator_->free(rtv_handle_);
    descriptor_allocator_->free(dsv_handle_);
    rtv_handle_ = {};
    dsv_handle_ = {};
}
} // namespace nori::graphics
