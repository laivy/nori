#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <nori/core/handle.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::detail
{
struct shader_tag;
struct pipeline_tag;

class shader_deleter
{
public:
    void operator()(core::basic_handle<shader_tag> handle) const noexcept;
};

class pipeline_deleter
{
public:
    void operator()(core::basic_handle<pipeline_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using shader_handle = core::basic_handle<detail::shader_tag>;
using pipeline_handle = core::basic_handle<detail::pipeline_tag>;
using unique_shader_handle = core::unique_handle<shader_handle, detail::shader_deleter>;
using unique_pipeline_handle = core::unique_handle<pipeline_handle, detail::pipeline_deleter>;

enum class shader_stage
{
    vertex = 1 << 0,
    pixel = 1 << 1
};

constexpr shader_stage operator|(shader_stage lhs, shader_stage rhs) noexcept
{
    return shader_stage{std::to_underlying(lhs) | std::to_underlying(rhs)};
}

constexpr shader_stage operator&(shader_stage lhs, shader_stage rhs) noexcept
{
    return shader_stage{std::to_underlying(lhs) & std::to_underlying(rhs)};
}

enum class shader_binding_type
{
    constant_buffer,
    texture_srv,
    sampler
};

enum class input_element_format
{
    float1,
    float2,
    float3,
    float4,
    uint1,
    uint2,
    uint3,
    uint4,
    int1,
    int2,
    int3,
    int4
};

struct input_element
{
    std::string_view semantic_name;
    std::uint32_t semantic_index;
    input_element_format format;
    std::uint32_t offset;
};

struct input_layout
{
    std::uint32_t stride;
    std::span<const input_element> elements;
};

enum class primitive_topology
{
    point_list,
    line_list,
    line_strip,
    triangle_list,
    triangle_strip
};

enum class fill_mode
{
    solid,
    wireframe
};

enum class cull_mode
{
    none,
    front,
    back
};

enum class front_face
{
    clockwise,
    counter_clockwise
};

enum class comparison_function
{
    never,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always
};

enum class stencil_op
{
    keep,
    zero,
    replace,
    increment_saturate,
    decrement_saturate,
    invert,
    increment,
    decrement
};

enum class blend_factor
{
    zero,
    one,
    source_alpha,
    inverse_source_alpha,
    destination_alpha,
    inverse_destination_alpha
};

enum class blend_op
{
    add,
    subtract,
    reverse_subtract,
    minimum,
    maximum
};

enum class color_write_mask : std::uint8_t
{
    none = 0,
    red = 1 << 0,
    green = 1 << 1,
    blue = 1 << 2,
    alpha = 1 << 3,
    all = 0x0F
};

struct render_target_blend_desc
{
    bool enabled;
    blend_factor source_color;
    blend_factor destination_color;
    blend_op color_operation;
    blend_factor source_alpha;
    blend_factor destination_alpha;
    blend_op alpha_operation;
    color_write_mask write_mask;
};

struct rasterizer_desc
{
    fill_mode fill;
    cull_mode cull;
    front_face front;
};

struct depth_desc
{
    bool write_enabled;
    comparison_function comparison;
};

struct stencil_op_desc
{
    stencil_op fail_op;
    stencil_op depth_fail_op;
    stencil_op pass_op;
    comparison_function comparison;
};

struct stencil_desc
{
    std::uint8_t read_mask;
    std::uint8_t write_mask;
    stencil_op_desc front;
    stencil_op_desc back;
};

struct shader_binding
{
    shader_binding_type type;
    std::uint32_t shader_register;
    shader_stage visibility;
};

struct graphics_pipeline_desc
{
    shader_handle vertex_shader;
    shader_handle pixel_shader;
    std::span<const shader_binding> bindings;
    std::span<const texture_format> render_target_formats;
    std::optional<texture_format> depth_stencil_format;
    input_layout layout;
    primitive_topology topology;
    rasterizer_desc rasterizer;
    std::optional<depth_desc> depth;
    std::optional<stencil_desc> stencil;
    std::span<const render_target_blend_desc> render_target_blends;
    std::uint32_t sample_count;
};
} // namespace nori::graphics
