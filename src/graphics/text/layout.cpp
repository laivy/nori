#include <dwrite.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>

#include "text/glyph_atlas.hpp"
#include "text/layout.hpp"
#include "text/layout_objects.hpp"

namespace
{
using nori::core::vec2;
using nori::core::vec4;
using nori::graphics::error_code;
using nori::graphics::result;
using nori::graphics::text::color_effect;
using nori::graphics::text::glyph_atlas;
using nori::graphics::text::image_object;
using nori::graphics::text::layout;
using nori::graphics::text::rect_batch;
using nori::graphics::text::textured_batch;
using nori::graphics::text::vertex;
using nori::graphics::texture_handle;

struct quad_bounds
{
    vec2 min;
    vec2 max;
};

class collector final : public IDWriteTextRenderer
{
public:
    collector(layout* output, glyph_atlas* atlas) :
        output_{output},
        atlas_{atlas}
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** value) override
    {
        if (!value)
        {
            return E_POINTER;
        }
        if (id == __uuidof(IUnknown) || id == __uuidof(IDWritePixelSnapping) || id == __uuidof(IDWriteTextRenderer))
        {
            *value = static_cast<IDWriteTextRenderer*>(this);
            AddRef();
            return S_OK;
        }
        *value = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        return --references_;
    }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override
    {
        *disabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override
    {
        *transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels_per_dip) override
    {
        *pixels_per_dip = 1.0f;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baseline_x, FLOAT baseline_y, DWRITE_MEASURING_MODE measuring_mode, const DWRITE_GLYPH_RUN* glyph_run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown* effect) override
    {
        const DWRITE_GLYPH_RUN& run{*glyph_run};
        const vec2 baseline{baseline_x, baseline_y};
        if (error_)
        {
            return E_FAIL;
        }
        if (run.glyphCount == 0)
        {
            return S_OK;
        }
        const bool is_right_to_left{(run.bidiLevel & 1u) != 0};
        constexpr auto no_cluster{std::numeric_limits<std::uint64_t>::max()};
        std::vector<std::uint64_t> clusters(run.glyphCount, no_cluster);
        if (description && description->clusterMap)
        {
            for (UINT32 character{}; character < description->stringLength; ++character)
            {
                auto& cluster{clusters[description->clusterMap[character]]};
                cluster = std::min(cluster, std::uint64_t{description->textPosition} + character);
            }
            std::uint64_t current{no_cluster};
            for (auto& cluster : clusters)
            {
                if (cluster != no_cluster)
                {
                    current = cluster;
                }
                cluster = current;
            }
        }
        float pen{};
        const auto& drawing_effect{*static_cast<color_effect*>(effect)};
        const float thickness{drawing_effect.outline ? drawing_effect.outline->thickness : 0.0f};
        for (UINT32 index{}; index < run.glyphCount; ++index)
        {
            const auto cached{atlas_->glyph(run.fontFace, run.fontEmSize, run.glyphIndices[index], run.isSideways != FALSE, measuring_mode, thickness)};
            if (!cached)
            {
                error_ = cached.error();
                return E_FAIL;
            }
            const DWRITE_GLYPH_OFFSET offset{
                run.glyphOffsets ? run.glyphOffsets[index].advanceOffset : 0.0f,
                run.glyphOffsets ? run.glyphOffsets[index].ascenderOffset : 0.0f
            };
            const float advance{run.glyphAdvances ? run.glyphAdvances[index] : 0.0f};
            if (cached->width != 0 && cached->height != 0)
            {
                const float origin_x{baseline.x + (is_right_to_left ? -pen - advance - offset.advanceOffset : pen + offset.advanceOffset)};
                const vec2 min{
                    std::round(origin_x) +
                        static_cast<float>(cached->left),
                    std::round(baseline.y - offset.ascenderOffset) +
                        static_cast<float>(cached->top)
                };
                const auto max{min + vec2{static_cast<float>(cached->width), static_cast<float>(cached->height)}};
                const auto key{clusters[index] == no_cluster ? fallback_cluster_++ : clusters[index]};
                const auto [it, inserted]{cluster_indices_.try_emplace(key, static_cast<std::uint32_t>(cluster_bounds_.size()))};
                if (inserted)
                {
                    cluster_bounds_.push_back({min, max});
                }
                else
                {
                    auto& bounds{cluster_bounds_[it->second]};
                    bounds = {
                        {std::min(bounds.min.x, min.x), std::min(bounds.min.y, min.y)},
                        {std::max(bounds.max.x, max.x), std::max(bounds.max.y, max.y)},
                    };
                }
                const auto color{static_cast<color_effect*>(effect)->color};
                output_->atlas_pages.retain(cached->page_index);
                append_quad(cached->texture, textured_batch::content_type::glyph, min, max, cached->uv_min, cached->uv_max, color, static_cast<float>(it->second), thickness > 0.0f ? drawing_effect.outline->color : vec4{});
            }
            pen += advance;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT x, FLOAT y, const DWRITE_UNDERLINE* line, IUnknown* effect) override
    {
        const float left{x - (line->readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? line->width : 0.0f)};
        return append_decoration({left, y + line->offset}, {line->width, line->thickness}, effect);
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH* line, IUnknown* effect) override
    {
        const float left{x - (line->readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? line->width : 0.0f)};
        return append_decoration({left, y + line->offset}, {line->width, line->thickness}, effect);
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT x, FLOAT y, IDWriteInlineObject* object, BOOL, BOOL, IUnknown*) override
    {
        const auto image{static_cast<image_object*>(object)->description()};
        const vec2 position{x, y};
        append_quad(image.texture, textured_batch::content_type::image, position, position + image.size, {}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
        return S_OK;
    }

    std::optional<error_code> error() const
    {
        return error_;
    }

    std::vector<vertex> finish()
    {
        for (auto& entry : vertices_)
        {
            if (entry.mode == std::to_underlying(textured_batch::content_type::glyph))
            {
                const auto& bounds{cluster_bounds_[static_cast<std::uint32_t>(entry.cluster)]};
                entry.pivot = (bounds.min + bounds.max) * 0.5f;
            }
        }
        return std::move(vertices_);
    }

private:
    void append_quad(texture_handle texture, textured_batch::content_type content, vec2 min, vec2 max, vec2 uv_min, vec2 uv_max, vec4 color, float cluster = 0.0f, vec4 outline_color = {})
    {
        auto current_batch{output_->batches.empty() ? nullptr : std::get_if<textured_batch>(&output_->batches.back())};
        if (!current_batch || current_batch->texture != texture || current_batch->content != content)
        {
            output_->batches.emplace_back(textured_batch{texture, content, static_cast<std::uint32_t>(vertices_.size()), 0});
            current_batch = &std::get<textured_batch>(output_->batches.back());
        }
        current_batch->vertex_count += 6;
        current_batch->has_outline = current_batch->has_outline || outline_color.w > 0.0f;
        const auto mode{std::to_underlying(content)};
        vertices_.insert(vertices_.end(), {{min, uv_min, color, outline_color, mode, cluster, {}}, {{max.x, min.y}, {uv_max.x, uv_min.y}, color, outline_color, mode, cluster, {}}, {{min.x, max.y}, {uv_min.x, uv_max.y}, color, outline_color, mode, cluster, {}}, {{min.x, max.y}, {uv_min.x, uv_max.y}, color, outline_color, mode, cluster, {}}, {{max.x, min.y}, {uv_max.x, uv_min.y}, color, outline_color, mode, cluster, {}}, {max, uv_max, color, outline_color, mode, cluster, {}}});
    }

    HRESULT append_decoration(vec2 position, vec2 size, IUnknown* effect)
    {
        if (output_->batches.empty() || !std::holds_alternative<rect_batch>(output_->batches.back()))
        {
            output_->batches.emplace_back(rect_batch{static_cast<std::uint32_t>(output_->rectangles.size()), 0});
        }
        ++std::get<rect_batch>(output_->batches.back()).rect_count;
        output_->rectangles.push_back({position, size, static_cast<color_effect*>(effect)->color});
        return S_OK;
    }

    layout* output_;
    glyph_atlas* atlas_;
    std::atomic<ULONG> references_{1};
    std::optional<error_code> error_;
    std::unordered_map<std::uint64_t, std::uint32_t> cluster_indices_;
    std::vector<quad_bounds> cluster_bounds_;
    std::vector<vertex> vertices_;
    std::uint64_t fallback_cluster_{std::uint64_t{1} << 32};
};
} // namespace

namespace nori::graphics::text
{
result<std::vector<vertex>> build_layout(IDWriteTextLayout* native_layout, glyph_atlas* atlas, layout& output)
{
    collector callbacks{&output, atlas};
    const HRESULT drawn{native_layout->Draw(nullptr, &callbacks, 0.0f, 0.0f)};
    if (const auto error{callbacks.error()}; error || FAILED(drawn))
    {
        return std::unexpected{error.value_or(error_code::resource_creation_failed)};
    }
    return callbacks.finish();
}
} // namespace nori::graphics::text
