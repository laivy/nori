#pragma once
#include <cstdint>
#include <optional>
#include <nori/core/singleton.hpp>
#include <nori/graphics/draw_list.hpp>
#include <nori/graphics/render_target.hpp>
#include "renderer.hpp"

namespace nori::graphics
{
class context : public core::singleton<context>
{
public:
    context();
    ~context() noexcept;

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
    result<draw_list> begin_pass(swap_chain_handle target);
    result<draw_list> begin_pass(render_target_handle target);
    result<void> end_pass(const draw_list& list);
    result<void> end_frame();
    result<void> present(swap_chain_handle handle);

#ifndef IMGUI_DISABLE
    result<void> initialize_imgui(HWND hwnd);
    result<void> render_imgui();
#endif

private:
    renderer renderer_;
    std::optional<swap_chain_handle> active_swap_chain_;
    std::optional<render_target_handle> active_render_target_;
    bool frame_active_;
    bool pass_active_;
    bool shutdown_;
    bool shutdown_succeeded_;
#ifndef IMGUI_DISABLE
    bool imgui_initialized_;
    bool imgui_rendered_;
#endif
};
} // namespace nori::graphics
