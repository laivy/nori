#include <cassert>
#include "gpu_timeline.hpp"
#include "swap_chain.hpp"
#include "swap_chain_manager.hpp"
#include "swap_chain_render_target.hpp"
#include "viewport.hpp"

namespace nori::graphics
{
swap_chain_manager::swap_chain_manager(IDXGIFactory4* dxgi_factory, ID3D12Device* device, ID3D12CommandQueue* command_queue, descriptor_allocator* descriptor_allocator, gpu_timeline* timeline) :
    dxgi_factory_{dxgi_factory},
    device_{device},
    command_queue_{command_queue},
    descriptor_allocator_{descriptor_allocator},
    timeline_{timeline}
{
    assert(dxgi_factory_);
    assert(device_);
    assert(command_queue_);
    assert(descriptor_allocator_);
    assert(timeline_);
}

swap_chain_manager::~swap_chain_manager()
{
}

result<swap_chain_handle> swap_chain_manager::create(swap_chain_specification spec)
{
    if (!spec.hwnd)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    return swap_chains_.emplace(std::make_unique<swap_chain>(spec.hwnd, dxgi_factory_, device_, command_queue_, descriptor_allocator_));
}

result<void> swap_chain_manager::destroy(swap_chain_handle handle)
{
    if (!swap_chains_.contains(handle))
    {
        return std::unexpected{error_code::invalid_handle};
    }
    auto chain{std::move(swap_chains_.get(handle))};
    swap_chains_.erase(handle);
    timeline_->defer([chain = std::move(chain)]() mutable
        {
            chain.reset();
        });
    return {};
}

result<void> swap_chain_manager::resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    auto chain{get(handle)};
    if (!chain)
    {
        return std::unexpected{error_code::invalid_handle};
    }
    return chain->resize(width, height);
}

result<void> swap_chain_manager::present(swap_chain_handle handle)
{
    auto chain{get(handle)};
    if (!chain)
    {
        return std::unexpected{error_code::invalid_handle};
    }
    return chain->present();
}

result<void> swap_chain_manager::begin(swap_chain_handle handle, ID3D12GraphicsCommandList* command_list)
{
    auto chain{get(handle)};
    if (!chain)
    {
        return std::unexpected{error_code::invalid_handle};
    }
    assert(command_list);
    chain->current_render_target().begin(command_list, viewport{chain->width(), chain->height()});
    return {};
}

result<void> swap_chain_manager::end(swap_chain_handle handle, ID3D12GraphicsCommandList* command_list)
{
    auto chain{get(handle)};
    if (!chain)
    {
        return std::unexpected{error_code::invalid_handle};
    }
    assert(command_list);
    chain->current_render_target().end(command_list);
    return {};
}

swap_chain* swap_chain_manager::get(swap_chain_handle handle)
{
    if (!swap_chains_.contains(handle))
    {
        return nullptr;
    }
    return swap_chains_.get(handle).get();
}
} // namespace nori::graphics
