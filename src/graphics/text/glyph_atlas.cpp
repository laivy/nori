#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <functional>
#include <ranges>
#include <unordered_set>

#include <nori/core/hash.hpp>

#include "resource/texture_manager.hpp"
#include "text/glyph_atlas.hpp"
#include "text/glyph_rasterizer.hpp"

namespace
{
constexpr std::uint32_t atlas_size{2048};
constexpr std::uint32_t atlas_padding{1};
} // namespace

namespace nori::graphics::text
{
std::size_t glyph_atlas::key_hash::operator()(const key& value) const noexcept
{
    std::size_t result{std::hash<IDWriteFontFace*>{}(value.face)};
    core::hash_combine(result, std::hash<std::uint32_t>{}(value.size));
    core::hash_combine(result, std::hash<std::uint16_t>{}(value.glyph));
    core::hash_combine(result, std::hash<bool>{}(value.sideways));
    core::hash_combine(result, std::hash<int>{}(value.measuring_mode));
    core::hash_combine(result, std::hash<std::uint32_t>{}(value.outline_thickness));
    return result;
}

glyph_atlas::glyph_atlas(IDWriteFactory5* factory, resource::texture_manager* textures) :
    factory_{factory},
    textures_{textures},
    rasterizer_{std::make_unique<glyph_rasterizer>()}
{
}

glyph_atlas::~glyph_atlas()
{
    for (const auto& slot : atlas_pages_)
    {
        if (slot)
        {
            textures_->destroy(slot->texture);
        }
    }
}

result<glyph_location> glyph_atlas::glyph(IDWriteFontFace* face, float em_size, std::uint16_t index, bool sideways, DWRITE_MEASURING_MODE measuring_mode, float outline_thickness)
{
    const key cache_key{face, std::bit_cast<std::uint32_t>(em_size), index, sideways, measuring_mode, std::bit_cast<std::uint32_t>(outline_thickness)};
    if (const auto it{glyphs_.find(cache_key)}; it != glyphs_.end())
    {
        return it->second;
    }

    const DWRITE_GLYPH_RUN run{
        .fontFace = face,
        .fontEmSize = em_size,
        .glyphCount = 1,
        .glyphIndices = &index,
        .isSideways = sideways
    };
    Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;
    if (FAILED(factory_->CreateGlyphRunAnalysis(
            &run,
            nullptr,
            DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
            measuring_mode,
            DWRITE_GRID_FIT_MODE_ENABLED,
            DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
            0.0f,
            0.0f,
            &analysis
        )))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    RECT bounds{};
    if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    const RECT fill_bounds{bounds};
    const std::uint32_t fill_width{static_cast<std::uint32_t>(bounds.right - bounds.left)};
    const std::uint32_t fill_height{static_cast<std::uint32_t>(bounds.bottom - bounds.top)};
    if (fill_width == 0 || fill_height == 0)
    {
        const glyph_location empty{
            .left = bounds.left,
            .top = bounds.top
        };
        return empty;
    }

    if (outline_thickness >= static_cast<float>(atlas_size) * 0.5f)
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    const LONG expansion{outline_thickness > 0.0f ? static_cast<LONG>(std::ceil(outline_thickness)) + 2 : 0};
    bounds = {bounds.left - expansion, bounds.top - expansion, bounds.right + expansion, bounds.bottom + expansion};
    const auto width{static_cast<std::uint32_t>(bounds.right - bounds.left)};
    const auto height{static_cast<std::uint32_t>(bounds.bottom - bounds.top)};
    const auto placement{allocate(width, height)};
    if (!placement)
    {
        return std::unexpected{placement.error()};
    }

    const auto row_pitch{(width + atlas_padding * 2) * 4};
    std::vector<std::byte> coverage(static_cast<std::size_t>(row_pitch) * (height + atlas_padding * 2));
    if (outline_thickness > 0.0f)
    {
        const auto outer{rasterizer_->raster_outline(face, index, em_size, sideways, outline_thickness, bounds)};
        if (!outer)
        {
            return std::unexpected{outer.error()};
        }
        for (std::uint32_t y{}; y < height; ++y)
        {
            for (std::uint32_t x{}; x < width; ++x)
            {
                const auto pixel{(y + atlas_padding) * row_pitch + (x + atlas_padding) * 4};
                coverage[pixel] = outer->fill[y * width + x];
                coverage[pixel + 1] = outer->outline[y * width + x];
            }
        }
    }
    else
    {
        std::vector<std::byte> fill(static_cast<std::size_t>(fill_width) * fill_height);
        if (FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &fill_bounds, reinterpret_cast<BYTE*>(fill.data()), static_cast<UINT32>(fill.size()))))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }
        for (std::uint32_t y{}; y < fill_height; ++y)
        {
            for (std::uint32_t x{}; x < fill_width; ++x)
            {
                coverage[(y + atlas_padding) * row_pitch + (x + atlas_padding) * 4] = fill[y * fill_width + x];
            }
        }
    }
    const texture_region region{placement->x - atlas_padding, placement->y - atlas_padding, width + atlas_padding * 2, height + atlas_padding * 2, 0};
    const texture_data data{coverage, row_pitch};
    const texture_handle texture{atlas_pages_[placement->page_index]->texture};
    if (const auto res{textures_->update(texture, region, data)}; !res)
    {
        return std::unexpected{res.error()};
    }

    const glyph_location entry{
        placement->page_index,
        texture,
        bounds.left,
        bounds.top,
        width,
        height,
        core::vec2{static_cast<float>(placement->x) / atlas_size, static_cast<float>(placement->y) / atlas_size},
        core::vec2{static_cast<float>(placement->x + width) / atlas_size, static_cast<float>(placement->y + height) / atlas_size}
    };
    if (!std::ranges::contains(cached_faces_, face, [](const auto& cached)
    {
        return cached.Get();
    }))
    {
        cached_faces_.emplace_back(face);
    }
    glyphs_.emplace(cache_key, entry);
    return entry;
}

void glyph_atlas::retain(std::uint32_t page_index)
{
    ++atlas_pages_[page_index]->references;
}

void glyph_atlas::release(std::span<const std::uint32_t> pages)
{
    for (const auto page_index : pages)
    {
        assert(atlas_pages_[page_index]->references != 0);
        --atlas_pages_[page_index]->references;
    }
    collect();
}

void glyph_atlas::collect()
{
    bool has_collected_pages{};
    for (auto& slot : atlas_pages_)
    {
        if (slot && slot->references == 0)
        {
            textures_->destroy(slot->texture);
            slot.reset();
            has_collected_pages = true;
        }
    }
    if (!has_collected_pages)
    {
        return;
    }

    std::unordered_set<IDWriteFontFace*> live_faces;
    live_faces.reserve(cached_faces_.size());
    std::erase_if(glyphs_, [this, &live_faces](const auto& entry)
    {
        const auto& [cache_key, image]{entry};
        if (!atlas_pages_[image.page_index])
        {
            return true;
        }
        live_faces.insert(cache_key.face);
        return false;
    });
    std::erase_if(cached_faces_, [&live_faces](const auto& face)
    {
        return !live_faces.contains(face.Get());
    });
}

result<glyph_atlas::allocation> glyph_atlas::allocate(std::uint32_t width, std::uint32_t height)
{
    if (width + atlas_padding * 2 > atlas_size || height + atlas_padding * 2 > atlas_size)
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    const std::uint32_t padded_width{width + atlas_padding * 2};
    const std::uint32_t padded_height{height + atlas_padding * 2};
    for (auto&& [index, slot] : std::views::enumerate(atlas_pages_))
    {
        if (!slot)
        {
            continue;
        }

        std::uint32_t cursor_x{slot->cursor_x};
        std::uint32_t cursor_y{slot->cursor_y};
        std::uint32_t row_height{slot->row_height};
        if (cursor_x + padded_width > atlas_size)
        {
            cursor_x = 0;
            cursor_y += row_height;
            row_height = 0;
        }
        if (cursor_y + padded_height <= atlas_size)
        {
            const std::uint32_t x{cursor_x + atlas_padding};
            const std::uint32_t y{cursor_y + atlas_padding};
            slot->cursor_x = cursor_x + padded_width;
            slot->cursor_y = cursor_y;
            slot->row_height = std::max(row_height, padded_height);
            return allocation{static_cast<std::uint32_t>(index), x, y};
        }
    }

    const auto texture{textures_->create({texture_format::rgba8_unorm, atlas_size, atlas_size, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination})};
    if (!texture)
    {
        return std::unexpected{texture.error()};
    }

    const auto it{std::ranges::find_if(atlas_pages_, [](const auto& slot)
    {
        return !slot;
    })};
    const std::uint32_t page_index{static_cast<std::uint32_t>(it - atlas_pages_.begin())};
    const page new_page{*texture, padded_width, 0, padded_height, 0};
    if (it == atlas_pages_.end())
    {
        atlas_pages_.emplace_back(new_page);
    }
    else
    {
        *it = new_page;
    }
    return allocation{
        page_index,
        atlas_padding,
        atlas_padding
    };
}
} // namespace nori::graphics::text
