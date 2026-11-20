#pragma once
#include <memory>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/swap_chain.hpp>
#include <nori/graphics/texture.hpp>

struct IDXGIFactory4;

namespace nori::graphics
{
class command_queue;
} // namespace nori::graphics

namespace nori::graphics::resource
{
class swap_chain;
class texture_manager;

class swap_chain_manager
{
public:
    struct params
    {
        IDXGIFactory4* dxgi_factory;
        command_queue* queue;
        texture_manager* textures;
    };

    explicit swap_chain_manager(params args);
    ~swap_chain_manager();

    bool wait_for_present_slots();
    result<swap_chain_handle> create(swap_chain_desc desc);
    texture_handle back_buffer(swap_chain_handle handle) const;
    result<void> resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height);
    void destroy(swap_chain_handle handle);

    bool present(swap_chain_handle handle);

private:
    IDXGIFactory4* const dxgi_factory_;
    command_queue* const queue_;
    texture_manager* const textures_;
    core::slot_map<swap_chain_handle, std::unique_ptr<swap_chain>> swap_chains_;
};
} // namespace nori::graphics::resource
