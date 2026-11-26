#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <nori/graphics/buffer.hpp>
#include <nori/graphics/command_encoder.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/render_pass.hpp>
#include <nori/graphics/sampler.hpp>
#include <nori/graphics/swap_chain.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/graphics/transfer.hpp>
#include <nori/resource/handle.hpp>

namespace nori::graphics
{
bool initialize();
void shutdown() noexcept;

bool begin_frame();
render_pass_handle begin_render_pass(const render_pass_desc& desc);
command_encoder* begin_encode(render_pass_handle handle);
void end_encode(command_encoder* commands);
bool end_render_pass(render_pass_handle handle);
bool end_frame();
bool present(swap_chain_handle handle);

std::uint64_t submit();
bool poll(std::uint64_t fence_value);
void wait(std::uint64_t fence_value);

result<unique_swap_chain_handle> create_swap_chain(swap_chain_desc desc);
texture_handle back_buffer(swap_chain_handle handle);
result<void> resize(swap_chain_handle handle, std::uint32_t width, std::uint32_t height);
void destroy(swap_chain_handle handle);

result<unique_texture_handle> create_texture(texture_desc desc);
result<unique_texture_handle> create_texture(nori::resource::handle source);
result<void> update(texture_handle handle, texture_region region, texture_data data);
void destroy(texture_handle handle);

result<unique_buffer_handle> create_buffer(buffer_desc desc, std::span<const std::byte> initial_data = {});
result<void> update(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data);
void destroy(buffer_handle handle);

result<frame_upload_buffer_view> create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data);

unique_shader_handle create_shader(std::span<const std::byte> bytecode);
void destroy(shader_handle handle);
unique_pipeline_handle create_graphics_pipeline(const graphics_pipeline_desc& desc);
void destroy(pipeline_handle handle);

unique_sampler_handle create_sampler(sampler_desc desc);
void destroy(sampler_handle handle);

result<void> copy(buffer_handle source, std::uint64_t source_offset, buffer_handle destination, std::uint64_t destination_offset, std::uint64_t size);
result<void> copy(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region);
result<void> blit(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region, blit_filter filter = blit_filter::linear);
result<void> generate_mips(texture_handle texture);

result<unique_readback_handle> readback(texture_handle texture, texture_region region);
readback_result poll(readback_handle handle);
void destroy(readback_handle handle);
} // namespace nori::graphics

namespace nori::graphics::text
{
result<unique_font_handle> create_font(font_desc desc);
result<unique_font_handle> create_font(nori::resource::handle source, std::uint32_t face_index = 0);
void destroy(font_handle handle);

result<unique_layout_handle> create_layout(layout_desc desc, std::string_view utf8);
result<unique_layout_handle> create_layout(layout_desc desc, std::span<const item> items);
result<void> update(layout_handle layout, std::string_view utf8);
result<void> update(layout_handle layout, layout_desc desc, std::string_view utf8);
result<void> update(layout_handle layout, std::span<const item> items);
result<void> update(layout_handle layout, layout_desc desc, std::span<const item> items);
layout_metrics metrics(layout_handle layout);
void destroy(layout_handle handle);
} // namespace nori::graphics::text
