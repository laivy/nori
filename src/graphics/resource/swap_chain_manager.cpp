#include "command_queue.hpp"
#include "resource/swap_chain_manager.hpp"
#include "swap_chain.hpp"

namespace nori::graphics::resource
{
swap_chain_manager::swap_chain_manager(params args) :
    dxgi_factory_{args.dxgi_factory},
    queue_{args.queue},
    textures_{args.textures}
{
}

swap_chain_manager::~swap_chain_manager() = default;

bool swap_chain_manager::wait_for_present_slots()
{
    for (auto& [_, chain] : swap_chains_)
    {
        if (!chain->is_usable())
        {
            continue;
        }
        if (const auto res{chain->wait_for_present_slot()}; !res)
        {
            return false;
        }
    }
    return true;
}

result<swap_chain_handle> swap_chain_manager::create(swap_chain_desc desc)
{
    auto chain{swap_chain::create(desc, dxgi_factory_, queue_->native(), textures_)};
    if (!chain)
    {
        return std::unexpected{chain.error()};
    }
    return swap_chains_.emplace(std::move(*chain));
}

texture_handle swap_chain_manager::back_buffer(swap_chain_handle handle) const
{
    return swap_chains_.at(handle)->back_buffer();
}

result<void> swap_chain_manager::resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    const auto& chain{swap_chains_.at(handle)};
    if (!chain->is_usable())
    {
        return std::unexpected{error_code::swap_chain_invalidated};
    }
    if (width == chain->width() && height == chain->height())
    {
        return {};
    }
    if (const auto res{queue_->submit()}; !res)
    {
        return std::unexpected{res.error()};
    }
    if (!queue_->wait_idle())
    {
        return std::unexpected{error_code::synchronization_failed};
    }
    queue_->collect();
    return chain->resize(width, height);
}

void swap_chain_manager::destroy(swap_chain_handle handle)
{
    auto& chain{swap_chains_.at(handle)};
    if (chain->is_usable())
    {
        chain->retire_back_buffers();
        queue_->release(std::move(chain));
    }
    // 무효 Chain은 백버퍼 해제 전에 GPU 완료를 기다린 상태
    swap_chains_.erase(handle);
}

bool swap_chain_manager::present(swap_chain_handle handle)
{
    return swap_chains_.at(handle)->present();
}
} // namespace nori::graphics::resource
