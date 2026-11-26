#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <dwrite_3.h>
#include <wrl/client.h>

#include <nori/core/vector.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::resource
{
class texture_manager;
} // namespace nori::graphics::resource

namespace nori::graphics::text
{
class glyph_rasterizer;

struct glyph_location
{
    std::uint32_t page_index;
    texture_handle texture;
    std::int32_t left;
    std::int32_t top;
    std::uint32_t width;
    std::uint32_t height;
    core::vec2 uv_min;
    core::vec2 uv_max;
};

class glyph_atlas
{
public:
    glyph_atlas(IDWriteFactory5* factory, resource::texture_manager* textures);
    ~glyph_atlas();

    glyph_atlas(const glyph_atlas&) = delete;
    glyph_atlas& operator=(const glyph_atlas&) = delete;

    result<glyph_location> glyph(IDWriteFontFace* face, float em_size, std::uint16_t index, bool sideways, DWRITE_MEASURING_MODE measuring_mode, float outline_thickness);
    void retain(std::uint32_t page_index);
    void release(std::span<const std::uint32_t> pages);
    void collect();

private:
    class key
    {
    public:
        bool operator==(const key&) const = default;

        IDWriteFontFace* face;
        std::uint32_t size;
        std::uint16_t glyph;
        bool sideways;
        DWRITE_MEASURING_MODE measuring_mode;
        std::uint32_t outline_thickness;
    };

    class key_hash
    {
    public:
        std::size_t operator()(const key& value) const noexcept;
    };

    struct page
    {
        texture_handle texture;
        std::uint32_t cursor_x;
        std::uint32_t cursor_y;
        std::uint32_t row_height;
        std::uint32_t references;
    };

    struct allocation
    {
        std::uint32_t page_index;
        std::uint32_t x;
        std::uint32_t y;
    };

    result<allocation> allocate(std::uint32_t width, std::uint32_t height);

    Microsoft::WRL::ComPtr<IDWriteFactory5> factory_;
    resource::texture_manager* const textures_;
    std::unique_ptr<glyph_rasterizer> rasterizer_;
    std::vector<std::optional<page>> atlas_pages_;
    std::vector<Microsoft::WRL::ComPtr<IDWriteFontFace>> cached_faces_;
    std::unordered_map<key, glyph_location, key_hash> glyphs_;
};
} // namespace nori::graphics::text
