#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include <nori/core/handle.hpp>
#include <nori/core/vector.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::text::detail
{
struct font_tag;

class font_deleter
{
public:
    void operator()(core::basic_handle<font_tag> handle) const noexcept;
};

struct layout_tag;

class layout_deleter
{
public:
    void operator()(core::basic_handle<layout_tag> handle) const noexcept;
};
} // namespace nori::graphics::text::detail

namespace nori::graphics::text
{
using font_handle = core::basic_handle<detail::font_tag>;
using unique_font_handle = core::unique_handle<font_handle, detail::font_deleter>;

struct font_desc
{
    std::span<const std::byte> data;
    std::uint32_t face_index;
};

struct outline_desc
{
    float thickness;
    core::vec4 color;
};

struct style
{
    std::optional<font_handle> font;
    std::optional<float> font_size;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
    std::optional<bool> strikethrough;
    std::optional<core::vec4> color;
    std::optional<outline_desc> outline;
};

struct span
{
    std::string_view text;
    style style_override;
};

struct inline_image
{
    texture_handle texture;
    core::vec2 size;
};

using item = std::variant<span, inline_image>;

using layout_handle = core::basic_handle<detail::layout_tag>;
using unique_layout_handle = core::unique_handle<layout_handle, detail::layout_deleter>;

enum class alignment
{
    left,
    center,
    right
};

enum class wrapping
{
    none,
    wrap
};

struct layout_desc
{
    font_handle font;
    float font_size;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    core::vec4 color;
    std::optional<outline_desc> outline;

    std::optional<float> max_width;
    alignment alignment_mode;
    wrapping wrapping_mode;
};

struct layout_metrics
{
    float width;
    float width_including_trailing_whitespace;
    float height;
    float first_baseline;
    std::uint32_t line_count;
};

struct effect
{
    shader_handle vertex_shader;
    shader_handle pixel_shader;
    std::span<const std::byte> constants;
};

struct draw_desc
{
    layout_handle layout;
    core::vec2 position;
    core::vec4 color;
    std::optional<effect> glyph_effect;
};
} // namespace nori::graphics::text
