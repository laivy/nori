#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <nori/core/handle.hpp>

namespace nori::graphics::detail
{
struct texture_tag;

class texture_deleter
{
public:
    void operator()(core::basic_handle<texture_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using texture_handle = core::basic_handle<detail::texture_tag>;
using unique_texture_handle = core::unique_handle<texture_handle, detail::texture_deleter>;

enum class texture_format
{
    r8_unorm,
    rgba8_unorm,
    rgba16_float,
    r32_float,
    d32_float,
    bc5_unorm,
    bc7_unorm,
    bc7_unorm_srgb
};

enum class texture_usage : std::uint32_t
{
    none = 0,
    shader_resource = 1 << 0,
    render_target = 1 << 1,
    depth_stencil = 1 << 2,
    copy_source = 1 << 3,
    copy_destination = 1 << 4
};

constexpr texture_usage operator|(texture_usage lhs, texture_usage rhs) noexcept
{
    return texture_usage{std::to_underlying(lhs) | std::to_underlying(rhs)};
}

constexpr texture_usage operator&(texture_usage lhs, texture_usage rhs) noexcept
{
    return texture_usage{std::to_underlying(lhs) & std::to_underlying(rhs)};
}

struct texture_desc
{
    texture_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t mip_levels;
    std::uint32_t sample_count;
    texture_usage usage;
};

struct texture_region
{
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t mip_level;
};

struct texture_data
{
    std::span<const std::byte> data;
    std::uint32_t row_pitch;
};
} // namespace nori::graphics
