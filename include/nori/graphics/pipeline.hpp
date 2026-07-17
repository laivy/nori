#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <vector>
#include <nori/core/handle.hpp>
#include <nori/graphics/render_target.hpp>

namespace nori::graphics
{
struct shader_tag;
using shader_handle = core::handle<shader_tag>;

struct pipeline_tag;
using pipeline_handle = core::handle<pipeline_tag>;

enum class shader_stage
{
    vertex,
    pixel
};

struct shader_specification
{
    shader_stage stage;
    std::span<const std::byte> bytecode;
};

enum class shader_binding_type
{
    texture_srv,
    static_sampler
};

struct shader_binding
{
    shader_binding_type type;
    std::uint32_t shader_register;
    shader_stage visibility;
};

struct graphics_pipeline_specification
{
    shader_handle vertex_shader;
    shader_handle pixel_shader;
    std::vector<shader_binding> bindings;
    std::vector<render_texture_format> color_formats;
    std::optional<render_texture_format> depth_format;
};
} // namespace nori::graphics
