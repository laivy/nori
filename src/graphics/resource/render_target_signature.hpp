#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <nori/graphics/texture.hpp>

namespace nori::graphics::resource
{
struct render_target_signature
{
    bool operator==(const render_target_signature&) const = default;

    std::vector<texture_format> render_target_formats;
    std::optional<texture_format> depth_stencil_format;
    std::uint32_t sample_count;
};
} // namespace nori::graphics::resource
