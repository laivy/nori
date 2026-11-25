#pragma once
#include <vector>

#include <nori/graphics/command_encoder.hpp>

#include "command_buffer.hpp"

namespace nori::graphics
{
class command_encoder::impl
{
public:
    impl();

    void begin_encode(command_buffer* buffer);
    command_buffer* end_encode();

    void set_pipeline(pipeline_handle pipeline);
    void set_vertex_buffer(buffer_handle buffer, std::uint32_t stride, std::uint64_t offset, std::optional<std::uint64_t> size = std::nullopt);
    void set_index_buffer(buffer_handle buffer, index_format format, std::uint64_t offset, std::optional<std::uint64_t> size = std::nullopt);
    void set_constant_buffer(std::uint32_t binding_index, buffer_handle buffer, std::uint64_t offset);
    void set_constant_buffer(std::uint32_t binding_index, std::span<const std::byte> data);
    void set_texture(std::uint32_t binding_index, texture_handle texture);
    void set_sampler(std::uint32_t binding_index, sampler_handle sampler);
    void set_viewport(viewport view);
    void set_scissor_rect(scissor_rect scissor);
    void draw(draw_operation operation);
    void draw_text(text::draw_desc desc);

private:
    void set_binding(std::uint32_t binding_index, draw_binding binding);

    command_buffer* buffer_;
    draw_state state_;
    std::vector<draw_binding> bindings_;
    std::size_t state_index_;
    bool dirty_;
};
} // namespace nori::graphics
