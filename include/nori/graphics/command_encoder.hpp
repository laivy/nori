#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <nori/core/vector.hpp>
#include <nori/graphics/buffer.hpp>
#include <nori/graphics/line.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/rect.hpp>
#include <nori/graphics/sampler.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics
{
enum class index_format
{
    uint16,
    uint32
};

struct viewport
{
    core::vec2 position;
    core::vec2 size;
    core::vec2 depth_range;
};

struct scissor_rect
{
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t width;
    std::uint32_t height;
};

class command_encoder
{
    friend class command_manager;

public:
    ~command_encoder() noexcept;

    command_encoder(const command_encoder&) = delete;
    command_encoder(command_encoder&&) = delete;
    command_encoder& operator=(const command_encoder&) = delete;
    command_encoder& operator=(command_encoder&&) = delete;

    void set_pipeline(pipeline_handle pipeline);
    void set_vertex_buffer(buffer_handle buffer, std::uint32_t stride, std::uint64_t offset = 0);
    void set_vertex_buffer(frame_upload_buffer_view buffer, std::uint32_t stride);
    void set_index_buffer(buffer_handle buffer, index_format format, std::uint64_t offset = 0);
    void set_index_buffer(frame_upload_buffer_view buffer, index_format format);
    void set_constant_buffer(std::uint32_t binding_index, buffer_handle buffer, std::uint64_t offset = 0);
    void set_constant_buffer(std::uint32_t binding_index, frame_upload_buffer_view buffer);
    void set_constant_buffer(std::uint32_t binding_index, std::span<const std::byte> data);
    void set_texture(std::uint32_t binding_index, texture_handle texture);
    void set_sampler(std::uint32_t binding_index, sampler_handle sampler);

    void set_viewport(viewport view);
    void set_scissor_rect(scissor_rect scissor);

    void draw(std::uint32_t vertex_count, std::uint32_t first_vertex = 0);
    void draw_instanced(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0);
    void draw_indexed(std::uint32_t index_count, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0);
    void draw_indexed_instanced(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0, std::uint32_t first_instance = 0);
    void draw_line(line_draw_desc desc);
    void draw_rect(rect_draw_desc desc);
    void draw_text(text::draw_desc desc);

private:
    class impl;

    command_encoder();

    std::unique_ptr<impl> impl_;
};
} // namespace nori::graphics
