#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <nori/core/handle.hpp>
#include <nori/core/vector.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::detail
{
struct render_pass_tag;
} // namespace nori::graphics::detail

namespace nori::graphics
{
using render_pass_handle = core::basic_handle<detail::render_pass_tag>;

enum class begin_op
{
    load,
    clear,
    discard
};

enum class end_op
{
    store,
    discard
};

struct clear_value
{
    core::vec4 color;
    float depth;
    std::uint8_t stencil;
};

struct begin_op_desc
{
    begin_op type;
    clear_value clear;
};

struct end_op_desc
{
    end_op type;
};

struct render_target_desc
{
    texture_handle target;
    begin_op_desc begin;
    end_op_desc end;
};

struct depth_stencil_desc
{
    texture_handle target;
    begin_op_desc depth_begin;
    begin_op_desc stencil_begin;
    end_op_desc depth_end;
    end_op_desc stencil_end;
};

struct render_pass_desc
{
    std::vector<render_target_desc> render_targets;
    std::optional<depth_stencil_desc> depth_stencil;
};
} // namespace nori::graphics
