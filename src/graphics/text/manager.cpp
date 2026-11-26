#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <wrl/implements.h>

#include <nori/core/encoding.hpp>
#include <nori/core/slot_map.hpp>
#include <nori/resource/binary.hpp>
#include <nori/resource/operations.hpp>

#include "resource/buffer_manager.hpp"
#include "text/font_registry.hpp"
#include "text/glyph_atlas.hpp"
#include "text/layout.hpp"
#include "text/layout_objects.hpp"
#include "text/manager.hpp"

namespace
{
constexpr float unconstrained_extent{16'777'216.0f};

constexpr DWRITE_TEXT_ALIGNMENT to_text_alignment(nori::graphics::text::alignment value)
{
    switch (value)
    {
    case nori::graphics::text::alignment::left:
    {
        return DWRITE_TEXT_ALIGNMENT_LEADING;
    }
    case nori::graphics::text::alignment::center:
    {
        return DWRITE_TEXT_ALIGNMENT_CENTER;
    }
    case nori::graphics::text::alignment::right:
    {
        return DWRITE_TEXT_ALIGNMENT_TRAILING;
    }
    default:
        std::unreachable();
    }
}

constexpr DWRITE_WORD_WRAPPING to_text_wrapping(nori::graphics::text::wrapping value)
{
    switch (value)
    {
    case nori::graphics::text::wrapping::none:
    {
        return DWRITE_WORD_WRAPPING_NO_WRAP;
    }
    case nori::graphics::text::wrapping::wrap:
    {
        return DWRITE_WORD_WRAPPING_WRAP;
    }
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::graphics::text
{
class manager::impl
{
public:
    ~impl()
    {
        for (auto& [_, entry] : layouts_)
        {
            release_layout(entry);
        }
        layouts_.clear();
        atlas_.reset();
    }

    static std::unique_ptr<impl> create(manager::params args)
    {
        Microsoft::WRL::ComPtr<IDWriteFactory> base_factory;
        if (FAILED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), &base_factory)))
        {
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IDWriteFactory5> factory;
        if (FAILED(base_factory.As(&factory)))
        {
            return nullptr;
        }

        auto fonts{font_registry::create(factory.Get())};
        if (!fonts)
        {
            return nullptr;
        }

        auto cached_pipelines{pipeline_cache::create(args.pipelines, args.samplers)};
        if (!cached_pipelines)
        {
            return nullptr;
        }

        return std::unique_ptr<impl>{new impl{args, std::move(factory), std::move(fonts), std::move(cached_pipelines)}};
    }

    result<font_handle> create_font(font_desc desc)
    {
        return fonts_->create_font(desc);
    }

    result<font_handle> create_font(nori::resource::handle source, std::uint32_t face_index)
    {
        const auto binary{nori::resource::try_get<nori::resource::binary_view>(source)};
        if (!binary)
        {
            return std::unexpected{binary.error() == nori::resource::error_code::type_mismatch ? error_code::invalid_argument : error_code::resource_creation_failed};
        }

        return create_font({binary->data, face_index});
    }

    void destroy(font_handle handle)
    {
        fonts_->destroy(handle);
    }

    result<layout_handle> create_layout(layout_desc desc, std::string_view utf8)
    {
        const std::array<item, 1> items{span{utf8, {}}};
        return create_layout(desc, items);
    }

    result<layout_handle> create_layout(layout_desc desc, std::span<const item> items)
    {
        auto entry{build(desc, items)};
        if (!entry)
        {
            return std::unexpected{entry.error()};
        }
        return layouts_.emplace(std::move(*entry));
    }

    result<void> update(layout_handle handle, std::string_view utf8)
    {
        return update(handle, at(handle).desc, utf8);
    }

    result<void> update(layout_handle handle, layout_desc desc, std::string_view utf8)
    {
        const std::array<item, 1> items{span{utf8, {}}};
        return update(handle, desc, items);
    }

    result<void> update(layout_handle handle, std::span<const item> items)
    {
        return update(handle, at(handle).desc, items);
    }

    result<void> update(layout_handle handle, layout_desc desc, std::span<const item> items)
    {
        auto replacement{build(desc, items)};
        if (!replacement)
        {
            return std::unexpected{replacement.error()};
        }
        auto& current{layouts_.at(handle)};
        release_layout(current);
        current = std::move(*replacement);
        return {};
    }

    void destroy(layout_handle handle)
    {
        auto& entry{layouts_.at(handle)};
        release_layout(entry);
        layouts_.erase(handle);
    }

    std::optional<render_state> prepare_render_state(const resource::render_target_signature& target, shader_handle vertex_shader, shader_handle pixel_shader)
    {
        return pipeline_cache_->prepare(target, vertex_shader, pixel_shader);
    }

    void invalidate(shader_handle shader)
    {
        pipeline_cache_->invalidate(shader);
    }

    const layout& at(layout_handle handle) const noexcept
    {
        return layouts_.at(handle);
    }

    layout_metrics metrics(layout_handle handle) const
    {
        return layouts_.at(handle).measured;
    }

private:
    struct prepared_text
    {
        std::wstring utf16;
        std::vector<DWRITE_TEXT_RANGE> ranges;
    };

    explicit impl(manager::params args, Microsoft::WRL::ComPtr<IDWriteFactory5> factory, std::unique_ptr<font_registry> fonts, std::unique_ptr<pipeline_cache> cached_pipelines) :
        buffers_{args.buffers},
        factory_{std::move(factory)},
        fonts_{std::move(fonts)},
        atlas_{std::make_unique<glyph_atlas>(factory_.Get(), args.textures)},
        pipeline_cache_{std::move(cached_pipelines)}
    {
    }

    result<layout> build(layout_desc desc, std::span<const item> items)
    {
        const auto text{prepare_text(items)};
        if (!text)
        {
            return std::unexpected{text.error()};
        }

        const auto native_layout{create_dwrite_layout(desc, text->utf16)};
        if (!native_layout)
        {
            return std::unexpected{native_layout.error()};
        }

        if (const auto res{apply_text_items(native_layout->Get(), desc, items, text->ranges)}; !res)
        {
            return std::unexpected{res.error()};
        }

        const auto measured{measure_layout(native_layout->Get())};
        if (!measured)
        {
            return std::unexpected{measured.error()};
        }

        layout entry{desc, *measured, {}, {}, {}, page_refs{atlas_.get()}};

        const auto vertices{build_layout(native_layout->Get(), atlas_.get(), entry)};
        if (!vertices)
        {
            return std::unexpected{vertices.error()};
        }

        const auto buffer{create_vertex_buffer(*vertices)};
        if (!buffer)
        {
            return std::unexpected{buffer.error()};
        }
        entry.vertices = *buffer;

        atlas_->collect();
        return entry;
    }

    static result<prepared_text> prepare_text(std::span<const item> items)
    {
        std::wstring utf16;
        std::vector<DWRITE_TEXT_RANGE> ranges;
        for (const auto& entry : items)
        {
            if (std::holds_alternative<inline_image>(entry))
            {
                ranges.push_back({static_cast<UINT32>(utf16.size()), 1});
                utf16.push_back(L'\xFFFC');
                continue;
            }
            const auto& run{std::get<span>(entry)};
            const auto converted{core::utf8_to_utf16(run.text)};
            if ((!run.text.empty() && converted.empty()) || utf16.size() + converted.size() > std::numeric_limits<UINT32>::max())
            {
                return std::unexpected{error_code::invalid_argument};
            }
            ranges.push_back({static_cast<UINT32>(utf16.size()), static_cast<UINT32>(converted.size())});
            utf16 += converted;
        }
        return prepared_text{std::move(utf16), std::move(ranges)};
    }

    result<Microsoft::WRL::ComPtr<IDWriteTextLayout4>> create_dwrite_layout(layout_desc desc, std::wstring_view utf16)
    {
        const auto default_selection{fonts_->select(desc.font, desc.bold, desc.italic)};
        if (!default_selection)
        {
            return std::unexpected{default_selection.error()};
        }
        const auto& font{**default_selection};
        const DWRITE_TEXT_ALIGNMENT native_alignment{to_text_alignment(desc.max_width ? desc.alignment_mode : alignment::left)};
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> native_layout;
        if (FAILED(factory_->CreateTextFormat(
                font.family.c_str(),
                font.collection.Get(),
                font.face->GetWeight(),
                font.face->GetStyle(),
                font.face->GetStretch(),
                desc.font_size,
                L"",
                &format
            )) ||
            FAILED(format->SetTextAlignment(native_alignment)) ||
            FAILED(format->SetWordWrapping(to_text_wrapping(desc.wrapping_mode))) ||
            FAILED(factory_->CreateTextLayout(
                utf16.data(),
                static_cast<UINT32>(utf16.size()),
                format.Get(),
                desc.max_width.value_or(unconstrained_extent),
                unconstrained_extent,
                &native_layout
            )))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        Microsoft::WRL::ComPtr<IDWriteFontFallbackBuilder> fallback_builder;
        Microsoft::WRL::ComPtr<IDWriteFontFallback> no_fallback;
        Microsoft::WRL::ComPtr<IDWriteTextLayout4> layout4;
        if (FAILED(factory_->CreateFontFallbackBuilder(&fallback_builder)) ||
            FAILED(fallback_builder->CreateFontFallback(&no_fallback)) ||
            FAILED(native_layout.As(&layout4)) ||
            FAILED(layout4->SetFontFallback(no_fallback.Get())) ||
            FAILED(layout4->SetAutomaticFontAxes(DWRITE_AUTOMATIC_FONT_AXES_NONE)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        return layout4;
    }

    result<void> apply_text_items(IDWriteTextLayout4* native_layout, layout_desc desc, std::span<const item> items, std::span<const DWRITE_TEXT_RANGE> ranges)
    {
        for (std::size_t i{0}; i < items.size(); ++i)
        {
            if (const auto image{std::get_if<inline_image>(&items[i])})
            {
                const auto object{Microsoft::WRL::Make<image_object>(*image)};
                if (FAILED(native_layout->SetInlineObject(object.Get(), ranges[i])))
                {
                    return std::unexpected{error_code::resource_creation_failed};
                }
                continue;
            }
            const auto& run{std::get<span>(items[i])};
            const auto selection{fonts_->select(run.style_override.font.value_or(desc.font), run.style_override.bold.value_or(desc.bold), run.style_override.italic.value_or(desc.italic))};
            if (!selection)
            {
                return std::unexpected{selection.error()};
            }
            const auto& selected{**selection};
            if (FAILED(native_layout->SetFontCollection(selected.collection.Get(), ranges[i])) ||
                FAILED(native_layout->SetFontFamilyName(selected.family.c_str(), ranges[i])) ||
                FAILED(native_layout->SetFontWeight(selected.face->GetWeight(), ranges[i])) ||
                FAILED(native_layout->SetFontStyle(selected.face->GetStyle(), ranges[i])) ||
                FAILED(native_layout->SetFontStretch(selected.face->GetStretch(), ranges[i])) ||
                FAILED(native_layout->SetFontAxisValues(selected.axes.data(), static_cast<UINT32>(selected.axes.size()), ranges[i])) ||
                FAILED(native_layout->SetFontSize(run.style_override.font_size.value_or(desc.font_size), ranges[i])) ||
                FAILED(native_layout->SetUnderline(run.style_override.underline.value_or(desc.underline), ranges[i])) ||
                FAILED(native_layout->SetStrikethrough(run.style_override.strikethrough.value_or(desc.strikethrough), ranges[i])))
            {
                return std::unexpected{error_code::resource_creation_failed};
            }
            const auto drawing_effect{Microsoft::WRL::Make<color_effect>(run.style_override.color.value_or(desc.color), run.style_override.outline ? run.style_override.outline : desc.outline)};
            if (FAILED(native_layout->SetDrawingEffect(drawing_effect.Get(), ranges[i])))
            {
                return std::unexpected{error_code::resource_creation_failed};
            }
        }

        return {};
    }

    static result<layout_metrics> measure_layout(IDWriteTextLayout* native_layout)
    {
        DWRITE_TEXT_METRICS native_metrics{};
        if (FAILED(native_layout->GetMetrics(&native_metrics)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        std::vector<DWRITE_LINE_METRICS> lines(native_metrics.lineCount);
        UINT32 line_count{};
        if (!lines.empty() && FAILED(native_layout->GetLineMetrics(lines.data(), static_cast<UINT32>(lines.size()), &line_count)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        return layout_metrics{
            native_metrics.width,
            native_metrics.widthIncludingTrailingWhitespace,
            native_metrics.height,
            lines.empty() ? 0.0f : lines.front().baseline,
            native_metrics.lineCount
        };
    }

    result<buffer_handle> create_vertex_buffer(std::span<const vertex> vertices)
    {
        if (vertices.empty())
        {
            return buffer_handle{};
        }

        return buffers_->create({vertices.size_bytes(), buffer_usage::vertex, buffer_access::gpu_only}, std::as_bytes(vertices));
    }

    void release_layout(const layout& entry)
    {
        if (entry.vertices)
        {
            buffers_->destroy(entry.vertices);
        }
    }

    resource::buffer_manager* buffers_;
    Microsoft::WRL::ComPtr<IDWriteFactory5> factory_;
    std::unique_ptr<font_registry> fonts_;
    std::unique_ptr<glyph_atlas> atlas_;
    core::slot_map<layout_handle, layout> layouts_;
    std::unique_ptr<pipeline_cache> pipeline_cache_;
};

manager::~manager() noexcept = default;

std::unique_ptr<manager> manager::create(params args)
{
    auto implementation{impl::create(args)};
    if (!implementation)
    {
        return nullptr;
    }
    return std::unique_ptr<manager>{new manager{std::move(implementation)}};
}

result<font_handle> manager::create_font(font_desc desc)
{
    return impl_->create_font(desc);
}

result<font_handle> manager::create_font(nori::resource::handle source, std::uint32_t face_index)
{
    return impl_->create_font(source, face_index);
}

void manager::destroy(font_handle handle)
{
    return impl_->destroy(handle);
}

result<layout_handle> manager::create_layout(layout_desc desc, std::string_view utf8)
{
    return impl_->create_layout(desc, utf8);
}

result<layout_handle> manager::create_layout(layout_desc desc, std::span<const item> items)
{
    return impl_->create_layout(desc, items);
}

result<void> manager::update(layout_handle handle, std::string_view utf8)
{
    return impl_->update(handle, utf8);
}

result<void> manager::update(layout_handle handle, layout_desc desc, std::string_view utf8)
{
    return impl_->update(handle, desc, utf8);
}

result<void> manager::update(layout_handle handle, std::span<const item> items)
{
    return impl_->update(handle, items);
}

result<void> manager::update(layout_handle handle, layout_desc desc, std::span<const item> items)
{
    return impl_->update(handle, desc, items);
}

void manager::destroy(layout_handle handle)
{
    return impl_->destroy(handle);
}

std::optional<render_state> manager::prepare_render_state(const resource::render_target_signature& target, shader_handle vertex_shader, shader_handle pixel_shader)
{
    return impl_->prepare_render_state(target, vertex_shader, pixel_shader);
}

void manager::invalidate(shader_handle shader)
{
    return impl_->invalidate(shader);
}

const layout& manager::at(layout_handle handle) const noexcept
{
    return impl_->at(handle);
}

layout_metrics manager::metrics(layout_handle handle) const
{
    return impl_->metrics(handle);
}

manager::manager(std::unique_ptr<impl> implementation) noexcept :
    impl_{std::move(implementation)}
{
}
} // namespace nori::graphics::text
