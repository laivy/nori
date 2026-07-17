#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/render_target.hpp>
#include "constants.hpp"
#include "descriptor_allocator.hpp"
#include "draw_list.hpp"
#include "render_assets.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
class gpu_timeline;
class swap_chain_manager;
class swap_chain_render_target;
class render_target_manager;
class viewport;
class pipeline_manager;

class renderer
{
private:
    struct frame_context
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
        std::uint64_t fence_value;
    };

public:
    renderer();
    ~renderer() noexcept;

    renderer(const renderer&) = delete;
    renderer(renderer&&) = delete;
    renderer& operator=(const renderer&) = delete;
    renderer& operator=(renderer&&) = delete;

    bool shutdown();

    result<swap_chain_handle> create_swap_chain(swap_chain_specification spec);
    result<void> resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height);
    result<render_target_handle> create_render_target(const render_target_specification& spec);
    result<render_texture_handle> create_render_texture(render_texture_specification spec);
    result<pipeline_handle> create_graphics_pipeline(const graphics_pipeline_specification& spec);
    result<shader_handle> create_shader(shader_specification spec);

    result<void> destroy(swap_chain_handle handle);
    result<void> destroy(render_target_handle handle);
    result<void> destroy(render_texture_handle handle);
    result<void> destroy(pipeline_handle handle);
    result<void> destroy(shader_handle handle);

    result<void> begin_frame();
    result<void> begin_pass(swap_chain_handle target);
    result<void> begin_pass(render_target_handle target);
    result<void> record(const draw_list& list);
    result<void> end_pass(swap_chain_handle target);
    result<void> end_pass(render_target_handle target);
    result<void> end_frame();
    result<void> present(swap_chain_handle handle);
    bool wait_idle();

    D3D12_GPU_DESCRIPTOR_HANDLE resolve_texture_srv(resource::handle handle);
    void release_texture(resource::handle handle);

#ifndef IMGUI_DISABLE
    result<void> initialize_imgui();
    result<void> begin_imgui();
    result<void> render_imgui();
    void shutdown_imgui() noexcept;
#endif

private:
    void initialize();
    bool create_dxgi_factory();
    bool create_d3d12_device();
    bool create_command_objects();
    void create_subsystems();
    void collect_completed_work();

    void begin_pass(swap_chain_render_target& target, const viewport& viewport);
    void end_pass(swap_chain_render_target& target);
    void render_model(const model& model);
    void render_quad(const quad& quad);
    result<void> render_fullscreen(const fullscreen_draw& draw);

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
    std::array<frame_context, frames_in_flight> frame_contexts_;
    std::uint32_t frame_index_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;
    std::unique_ptr<gpu_timeline> timeline_;
    std::unique_ptr<upload_manager> upload_manager_;
    std::unique_ptr<descriptor_allocator> descriptor_allocator_;
    std::unique_ptr<swap_chain_manager> swap_chain_manager_;
    std::unique_ptr<render_target_manager> render_target_manager_;
    std::unique_ptr<render_assets> assets_;
    std::unique_ptr<pipeline_manager> pipeline_manager_;
    std::optional<render_target_handle> active_render_target_;
    std::vector<render_texture_format> active_color_formats_;

    bool shutdown_;
    bool shutdown_succeeded_;
#ifndef IMGUI_DISABLE
    bool imgui_initialized_;
#endif
};
} // namespace nori::graphics
