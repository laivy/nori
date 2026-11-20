#include <memory>
#include <utility>

#include <nori/graphics/operations.hpp>

#include "command_manager.hpp"
#include "context.hpp"

namespace
{
std::unique_ptr<nori::graphics::context> instance;
} // namespace

namespace nori::graphics::detail
{
void texture_deleter::operator()(texture_handle handle) const noexcept
{
    destroy(handle);
}

void buffer_deleter::operator()(buffer_handle handle) const noexcept
{
    destroy(handle);
}

void sampler_deleter::operator()(sampler_handle handle) const noexcept
{
    destroy(handle);
}

void swap_chain_deleter::operator()(swap_chain_handle handle) const noexcept
{
    destroy(handle);
}

void shader_deleter::operator()(shader_handle handle) const noexcept
{
    destroy(handle);
}

void pipeline_deleter::operator()(pipeline_handle handle) const noexcept
{
    destroy(handle);
}

void readback_deleter::operator()(readback_handle handle) const noexcept
{
    destroy(handle);
}
} // namespace nori::graphics::detail

namespace nori::graphics
{
context* ctx() noexcept
{
    return instance.get();
}

bool initialize()
{
    auto created{context::create()};
    if (!created)
    {
        return false;
    }
    instance = std::move(created);
    return true;
}

void shutdown() noexcept
{
    instance.reset();
}

bool begin_frame()
{
    return ctx()->begin_frame();
}

render_pass_handle begin_render_pass(const render_pass_desc& desc)
{
    return ctx()->begin_render_pass(desc);
}

command_encoder* begin_encode(render_pass_handle handle)
{
    return ctx()->commands()->begin_encode(handle);
}

void end_encode(command_encoder* commands)
{
    ctx()->commands()->end_encode(commands);
}

bool end_render_pass(render_pass_handle handle)
{
    return ctx()->end_render_pass(handle);
}

bool end_frame()
{
    return ctx()->end_frame();
}

bool present(swap_chain_handle handle)
{
    return ctx()->present(handle);
}

std::uint64_t submit()
{
    return ctx()->submit();
}

bool poll(std::uint64_t fence_value)
{
    return ctx()->poll(fence_value);
}

void wait(std::uint64_t fence_value)
{
    ctx()->wait(fence_value);
}

result<unique_swap_chain_handle> create_swap_chain(swap_chain_desc desc)
{
    return ctx()->create_swap_chain(desc).transform([](swap_chain_handle handle)
    {
        return unique_swap_chain_handle{handle, {}};
    });
}

texture_handle back_buffer(swap_chain_handle handle)
{
    return ctx()->back_buffer(handle);
}

result<void> resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    return ctx()->resize_swap_chain(handle, width, height);
}

void destroy(swap_chain_handle handle)
{
    return ctx()->destroy(handle);
}

result<unique_texture_handle> create_texture(texture_desc desc)
{
    return ctx()->create_texture(desc).transform([](texture_handle handle)
    {
        return unique_texture_handle{handle, {}};
    });
}

result<unique_texture_handle> create_texture(nori::resource::handle source)
{
    return ctx()->create_texture(source).transform([](texture_handle handle)
    {
        return unique_texture_handle{handle, {}};
    });
}

result<void> update(texture_handle handle, texture_region region, texture_data data)
{
    return ctx()->update_texture(handle, region, data);
}

void destroy(texture_handle handle)
{
    return ctx()->destroy(handle);
}

result<unique_buffer_handle> create_buffer(buffer_desc desc, std::span<const std::byte> initial_data)
{
    return ctx()->create_buffer(desc, initial_data).transform([](buffer_handle handle)
    {
        return unique_buffer_handle{handle, {}};
    });
}

result<void> update(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data)
{
    return ctx()->update_buffer(handle, offset, data);
}

void destroy(buffer_handle handle)
{
    return ctx()->destroy(handle);
}

result<frame_upload_buffer_view> create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data)
{
    return ctx()->create_frame_upload_buffer(usage, data);
}

unique_shader_handle create_shader(std::span<const std::byte> bytecode)
{
    return {ctx()->create_shader(bytecode), {}};
}

void destroy(shader_handle handle)
{
    return ctx()->destroy(handle);
}

unique_pipeline_handle create_graphics_pipeline(const graphics_pipeline_desc& desc)
{
    return {ctx()->create_graphics_pipeline(desc), {}};
}

void destroy(pipeline_handle handle)
{
    return ctx()->destroy(handle);
}

unique_sampler_handle create_sampler(sampler_desc desc)
{
    return {ctx()->create_sampler(desc), {}};
}

void destroy(sampler_handle handle)
{
    return ctx()->destroy(handle);
}

result<void> copy(buffer_handle source, std::uint64_t source_offset, buffer_handle destination, std::uint64_t destination_offset, std::uint64_t size)
{
    return ctx()->copy_buffer(source, source_offset, destination, destination_offset, size);
}

result<void> copy(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region)
{
    return ctx()->copy_texture(source, source_region, destination, destination_region);
}

result<void> blit(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region, blit_filter filter)
{
    return ctx()->blit_texture(source, source_region, destination, destination_region, filter);
}

result<void> generate_mips(texture_handle texture)
{
    return ctx()->generate_mips(texture);
}

result<unique_readback_handle> readback(texture_handle texture, texture_region region)
{
    return ctx()->read_texture(texture, region).transform([](readback_handle handle)
    {
        return unique_readback_handle{handle, {}};
    });
}

readback_result poll(readback_handle handle)
{
    return ctx()->poll(handle);
}

void destroy(readback_handle handle)
{
    return ctx()->destroy(handle);
}
} // namespace nori::graphics

namespace nori::graphics::text::detail
{
void font_deleter::operator()(font_handle handle) const noexcept
{
    destroy(handle);
}

void layout_deleter::operator()(layout_handle handle) const noexcept
{
    destroy(handle);
}
} // namespace nori::graphics::text::detail

namespace nori::graphics::text
{
result<unique_font_handle> create_font(font_desc desc)
{
    return ctx()->create_font(desc).transform([](font_handle handle)
    {
        return unique_font_handle{handle, {}};
    });
}

result<unique_font_handle> create_font(nori::resource::handle source, std::uint32_t face_index)
{
    return ctx()->create_font(source, face_index).transform([](font_handle handle)
    {
        return unique_font_handle{handle, {}};
    });
}

void destroy(font_handle handle)
{
    return ctx()->destroy(handle);
}

result<unique_layout_handle> create_layout(layout_desc desc, std::string_view utf8)
{
    return ctx()->create_text_layout(desc, utf8).transform([](layout_handle handle)
    {
        return unique_layout_handle{handle, {}};
    });
}

result<unique_layout_handle> create_layout(layout_desc desc, std::span<const item> items)
{
    return ctx()->create_text_layout(desc, items).transform([](layout_handle handle)
    {
        return unique_layout_handle{handle, {}};
    });
}

result<void> update(layout_handle layout, std::string_view utf8)
{
    return ctx()->update_text_layout(layout, utf8);
}

result<void> update(layout_handle layout, layout_desc desc, std::string_view utf8)
{
    return ctx()->update_text_layout(layout, desc, utf8);
}

result<void> update(layout_handle layout, std::span<const item> items)
{
    return ctx()->update_text_layout(layout, items);
}

result<void> update(layout_handle layout, layout_desc desc, std::span<const item> items)
{
    return ctx()->update_text_layout(layout, desc, items);
}

layout_metrics metrics(layout_handle layout)
{
    return ctx()->text_layout_metrics(layout);
}

void destroy(layout_handle handle)
{
    return ctx()->destroy(handle);
}
} // namespace nori::graphics::text
