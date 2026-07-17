#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <Windows.h>
#include <nori/core/handle.hpp>

namespace nori::graphics
{
struct swap_chain_tag;
using swap_chain_handle = core::handle<swap_chain_tag>;

struct render_target_tag;
using render_target_handle = core::handle<render_target_tag>;

struct render_texture_tag;
using render_texture_handle = core::handle<render_texture_tag>;

struct swap_chain_specification
{
    HWND hwnd;
};

enum class render_texture_format
{
    rgba8_unorm,
    rgba16_float,
    r32_float,
    d32_float
};

struct render_texture_specification
{
    std::uint32_t width;
    std::uint32_t height;
    render_texture_format format;
};

struct render_target_specification
{
    std::vector<render_texture_handle> color_attachments;
    std::optional<render_texture_handle> depth_attachment;
};
} // namespace nori::graphics
