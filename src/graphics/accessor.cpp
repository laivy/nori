#include "accessor.hpp"
#include "context.hpp"

namespace nori::graphics
{
result<swap_chain_handle> create_swap_chain(swap_chain_specification spec)
{
    if (context* const value{context::instance()})
    {
        return value->create_swap_chain(spec);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    if (context* const value{context::instance()})
    {
        return value->resize_swap_chain(handle, width, height);
    }
    return std::unexpected{error_code::not_initialized};
}

result<render_target_handle> create_render_target(const render_target_specification& spec)
{
    if (context* const value{context::instance()})
    {
        return value->create_render_target(spec);
    }
    return std::unexpected{error_code::not_initialized};
}

result<render_texture_handle> create_render_texture(render_texture_specification spec)
{
    if (context* const value{context::instance()})
    {
        return value->create_render_texture(spec);
    }
    return std::unexpected{error_code::not_initialized};
}

result<pipeline_handle> create_graphics_pipeline(const graphics_pipeline_specification& spec)
{
    if (context* const value{context::instance()})
    {
        return value->create_graphics_pipeline(spec);
    }
    return std::unexpected{error_code::not_initialized};
}

result<shader_handle> create_shader(shader_specification spec)
{
    if (context* const value{context::instance()})
    {
        return value->create_shader(spec);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> destroy(swap_chain_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->destroy(handle);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> destroy(render_target_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->destroy(handle);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> destroy(render_texture_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->destroy(handle);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> destroy(pipeline_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->destroy(handle);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> destroy(shader_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->destroy(handle);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> begin_frame()
{
    if (context* const value{context::instance()})
    {
        return value->begin_frame();
    }
    return std::unexpected{error_code::not_initialized};
}

result<draw_list> begin_pass(swap_chain_handle target)
{
    if (context* const value{context::instance()})
    {
        return value->begin_pass(target);
    }
    return std::unexpected{error_code::not_initialized};
}

result<draw_list> begin_pass(render_target_handle target)
{
    if (context* const value{context::instance()})
    {
        return value->begin_pass(target);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> end_pass(const draw_list& list)
{
    if (context* const value{context::instance()})
    {
        return value->end_pass(list);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> end_frame()
{
    if (context* const value{context::instance()})
    {
        return value->end_frame();
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> present(swap_chain_handle handle)
{
    if (context* const value{context::instance()})
    {
        return value->present(handle);
    }
    return std::unexpected{error_code::not_initialized};
}
} // namespace nori::graphics
