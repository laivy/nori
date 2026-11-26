#pragma once
#include <dwrite.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <nori/graphics/error_code.hpp>

namespace nori::graphics::text
{
struct glyph_masks
{
    std::vector<std::byte> fill;
    std::vector<std::byte> outline;
};

class glyph_rasterizer
{
public:
    glyph_rasterizer();
    ~glyph_rasterizer();

    glyph_rasterizer(const glyph_rasterizer&) = delete;
    glyph_rasterizer& operator=(const glyph_rasterizer&) = delete;

    result<glyph_masks> raster_outline(IDWriteFontFace* face, std::uint16_t index, float em_size, bool sideways, float thickness, RECT bounds);

private:
    class impl;

    std::unique_ptr<impl> impl_;
};
}
