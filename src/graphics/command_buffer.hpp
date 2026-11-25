#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <nori/graphics/buffer.hpp>
#include <nori/graphics/command_encoder.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/render_pass.hpp>
#include <nori/graphics/sampler.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics
{
struct vertex_buffer_command
{
    buffer_handle buffer;
    std::uint32_t stride;
    std::uint64_t offset;
    std::optional<std::uint64_t> size;
};

struct index_buffer_command
{
    buffer_handle buffer;
    index_format format;
    std::uint64_t offset;
    std::optional<std::uint64_t> size;
};

struct draw_state
{
    pipeline_handle pipeline;
    vertex_buffer_command vertex_buffer;
    index_buffer_command index_buffer;
    std::optional<viewport> view;
    std::optional<scissor_rect> scissor;
    std::size_t binding_offset;
    std::size_t binding_count;
};

struct constant_buffer_command
{
    buffer_handle buffer;
    std::uint64_t offset;
};

struct constants_command
{
    std::size_t offset;
    std::size_t size;
};

using draw_binding = std::variant<std::monostate, constant_buffer_command, texture_handle, sampler_handle, constants_command>;

struct draw_args
{
    std::uint32_t vertex_count;
    std::uint32_t instance_count;
    std::uint32_t first_vertex;
    std::uint32_t first_instance;
};

struct draw_indexed_args
{
    std::uint32_t index_count;
    std::uint32_t instance_count;
    std::uint32_t first_index;
    std::int32_t vertex_offset;
    std::uint32_t first_instance;
};

struct text_draw_command
{
    text::layout_handle layout;
    core::vec2 position;
    core::vec4 color;
    shader_handle vertex_shader;
    shader_handle pixel_shader;
    constants_command constants;
};

using draw_operation = std::variant<draw_args, draw_indexed_args, line_draw_desc, rect_draw_desc, text_draw_command>;

struct draw_command
{
    std::size_t state_index;
    draw_operation operation;
};

struct command_buffer
{
    render_pass_handle pass;
    std::vector<draw_state> states;
    std::vector<draw_binding> bindings;
    std::vector<std::byte> constants;
    std::vector<draw_command> draws;
};
} // namespace nori::graphics
