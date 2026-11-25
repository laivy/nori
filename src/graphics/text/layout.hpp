#pragma once
#include <cstdint>
#include <variant>
#include <vector>

#include <nori/graphics/buffer.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/rect.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>

#include "text/page_refs.hpp"

struct IDWriteTextLayout;

namespace nori::graphics::text
{
class glyph_atlas;

struct vertex
{
    core::vec2 position;
    core::vec2 uv;
    core::vec4 color;
    core::vec4 outline_color;
    std::uint32_t mode;
    float cluster;
    core::vec2 pivot;
};

struct rect_batch
{
    std::uint32_t first_rect;
    std::uint32_t rect_count;
};

struct textured_batch
{
    enum class content_type : std::uint32_t
    {
        glyph,
        image
    };

    texture_handle texture;
    content_type content;
    std::uint32_t first_vertex;
    std::uint32_t vertex_count;
    bool has_outline;
};

using batch = std::variant<rect_batch, textured_batch>;

struct layout
{
    layout_desc desc;
    layout_metrics measured;
    std::vector<batch> batches;
    buffer_handle vertices;
    std::vector<rect_draw_desc> rectangles;
    page_refs atlas_pages;
};

result<std::vector<vertex>> build_layout(IDWriteTextLayout* native_layout, glyph_atlas* atlas, layout& output);
} // namespace nori::graphics::text
