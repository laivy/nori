#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include "text/font_registry.hpp"

namespace
{
constexpr nori::graphics::error_code to_error_code(HRESULT error)
{
    return error == DWRITE_E_FILEFORMAT || error == E_INVALIDARG ?
        nori::graphics::error_code::invalid_argument :
        nori::graphics::error_code::resource_creation_failed;
}

nori::graphics::result<std::wstring> family_name(IDWriteFontFace3* face)
{
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
    UINT32 length{};
    if (FAILED(face->GetFamilyNames(&names)) || names->GetCount() == 0 || FAILED(names->GetStringLength(0, &length)))
    {
        return std::unexpected{nori::graphics::error_code::resource_creation_failed};
    }

    std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
    if (FAILED(names->GetString(0, name.data(), length + 1)))
    {
        return std::unexpected{nori::graphics::error_code::resource_creation_failed};
    }

    name.resize(length);
    return name;
}

std::optional<float> find_axis_value(std::span<const DWRITE_FONT_AXIS_VALUE> axes, DWRITE_FONT_AXIS_TAG tag)
{
    const auto it{std::ranges::find(axes, tag, &DWRITE_FONT_AXIS_VALUE::axisTag)};
    return it == axes.end() ? std::nullopt : std::optional<float>{it->value};
}

DWRITE_FONT_STYLE font_style(std::span<const DWRITE_FONT_AXIS_VALUE> axes, IDWriteFontFace3* face)
{
    if (find_axis_value(axes, DWRITE_FONT_AXIS_TAG_ITALIC).value_or(face->GetStyle() == DWRITE_FONT_STYLE_ITALIC ? 1.0f : 0.0f) == 1.0f)
    {
        return DWRITE_FONT_STYLE_ITALIC;
    }

    const auto it{std::ranges::find(axes, DWRITE_FONT_AXIS_TAG_SLANT, &DWRITE_FONT_AXIS_VALUE::axisTag)};
    if (it != axes.end())
    {
        return it->value == 0.0f ? DWRITE_FONT_STYLE_NORMAL : DWRITE_FONT_STYLE_OBLIQUE;
    }

    return face->GetStyle() == DWRITE_FONT_STYLE_OBLIQUE ? DWRITE_FONT_STYLE_OBLIQUE : DWRITE_FONT_STYLE_NORMAL;
}
} // namespace

namespace nori::graphics::text
{
font_registry::~font_registry()
{
    fonts_.clear();
    factory_->UnregisterFontFileLoader(font_loader_.Get());
}

std::unique_ptr<font_registry> font_registry::create(IDWriteFactory5* factory)
{
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> font_loader;
    if (FAILED(factory->CreateInMemoryFontFileLoader(&font_loader)) ||
        FAILED(factory->RegisterFontFileLoader(font_loader.Get())))
    {
        return nullptr;
    }

    return std::unique_ptr<font_registry>{new font_registry{factory, std::move(font_loader)}};
}

result<font_handle> font_registry::create_font(font_desc desc)
{
    if (desc.data.empty() || desc.data.size() > std::numeric_limits<UINT32>::max())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    Microsoft::WRL::ComPtr<IDWriteFontFile> file;
    if (FAILED(font_loader_->CreateInMemoryFontFileReference(
            factory_.Get(),
            desc.data.data(),
            static_cast<UINT32>(desc.data.size()),
            nullptr,
            &file
        )))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    BOOL supported{};
    DWRITE_FONT_FILE_TYPE file_type{};
    DWRITE_FONT_FACE_TYPE face_type{};
    UINT32 face_count{};
    if (const HRESULT hr{file->Analyze(&supported, &file_type, &face_type, &face_count)}; FAILED(hr))
    {
        return std::unexpected{to_error_code(hr)};
    }
    if (file_type == DWRITE_FONT_FILE_TYPE_UNKNOWN)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (!supported)
    {
        return std::unexpected{error_code::unsupported};
    }
    if (desc.face_index >= face_count)
    {
        return std::unexpected{error_code::invalid_argument};
    }

    IDWriteFontFile* source_file{file.Get()};
    Microsoft::WRL::ComPtr<IDWriteFontFace> base_face;
    Microsoft::WRL::ComPtr<IDWriteFontFace5> selected_face;
    if (const HRESULT hr{factory_->CreateFontFace(face_type, 1, &source_file, desc.face_index, DWRITE_FONT_SIMULATIONS_NONE, &base_face)}; FAILED(hr))
    {
        return std::unexpected{to_error_code(hr)};
    }
    if (FAILED(base_face.As(&selected_face)))
    {
        return std::unexpected{error_code::unsupported};
    }

    const auto selected_family{family_name(selected_face.Get())};
    if (!selected_family)
    {
        return std::unexpected{selected_family.error()};
    }
    std::vector<Microsoft::WRL::ComPtr<IDWriteFontFace5>> faces;
    for (UINT32 index{0}; index < face_count; ++index)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFace> raw_face;
        Microsoft::WRL::ComPtr<IDWriteFontFace5> candidate;
        if (FAILED(factory_->CreateFontFace(face_type, 1, &source_file, index, DWRITE_FONT_SIMULATIONS_NONE, &raw_face)) || FAILED(raw_face.As(&candidate)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }
        const auto candidate_family{family_name(candidate.Get())};
        if (!candidate_family)
        {
            return std::unexpected{candidate_family.error()};
        }
        if (*candidate_family == *selected_family)
        {
            faces.push_back(std::move(candidate));
        }
    }

    auto baseline{make_selection(std::move(selected_face))};
    if (!baseline)
    {
        return std::unexpected{baseline.error()};
    }

    font entry{std::move(file), std::move(faces), {}};
    entry.selections[0] = std::move(*baseline);
    return fonts_.emplace(std::move(entry));
}

void font_registry::destroy(font_handle handle)
{
    fonts_.erase(handle);
}

result<const font_selection*> font_registry::select(font_handle handle, bool bold, bool italic)
{
    font& resource{fonts_.at(handle)};
    const std::size_t selection_index{static_cast<std::size_t>(bold) + 2 * static_cast<std::size_t>(italic)};
    if (auto& selection{resource.selections[selection_index]})
    {
        return &*selection;
    }

    const font_selection& baseline{*resource.selections[0]};
    for (const auto& candidate : resource.faces)
    {
        Microsoft::WRL::ComPtr<IDWriteFontResource> font_source;
        if (FAILED(candidate->GetFontResource(&font_source)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        std::vector<DWRITE_FONT_AXIS_RANGE> ranges(font_source->GetFontAxisCount());
        std::vector<DWRITE_FONT_AXIS_VALUE> axes(candidate->GetFontAxisValueCount());
        if (FAILED(font_source->GetFontAxisRanges(ranges.data(), static_cast<UINT32>(ranges.size()))) ||
            FAILED(candidate->GetFontAxisValues(axes.data(), static_cast<UINT32>(axes.size()))))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        bool matches{true};
        for (auto& axis : axes)
        {
            const auto base_it{std::ranges::find(baseline.axes, axis.axisTag, &DWRITE_FONT_AXIS_VALUE::axisTag)};
            float requested{base_it == baseline.axes.end() ? axis.value : base_it->value};
            if (bold && axis.axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT)
            {
                requested = static_cast<float>(DWRITE_FONT_WEIGHT_BOLD);
            }
            if (italic && axis.axisTag == DWRITE_FONT_AXIS_TAG_ITALIC)
            {
                requested = 1.0f;
            }
            const auto it{std::ranges::find(ranges, axis.axisTag, &DWRITE_FONT_AXIS_RANGE::axisTag)};
            if (it != ranges.end() && it->minValue < it->maxValue)
            {
                if (requested < it->minValue || requested > it->maxValue)
                {
                    matches = false;
                    break;
                }
                axis.value = requested;
            }
        }
        if (!matches)
        {
            continue;
        }

        Microsoft::WRL::ComPtr<IDWriteFontFace5> chosen;
        if (FAILED(font_source->CreateFontFace(DWRITE_FONT_SIMULATIONS_NONE, axes.data(), static_cast<UINT32>(axes.size()), &chosen)))
        {
            return std::unexpected{error_code::resource_creation_failed};
        }

        float target_weight{static_cast<float>(DWRITE_FONT_WEIGHT_BOLD)};
        if (!bold)
        {
            target_weight = find_axis_value(baseline.axes, DWRITE_FONT_AXIS_TAG_WEIGHT).value_or(static_cast<float>(baseline.face->GetWeight()));
        }

        const auto target_width{find_axis_value(baseline.axes, DWRITE_FONT_AXIS_TAG_WIDTH)};
        bool width_matches{};
        if (!target_width)
        {
            width_matches = chosen->GetStretch() == baseline.face->GetStretch();
        }
        else
        {
            width_matches = find_axis_value(axes, DWRITE_FONT_AXIS_TAG_WIDTH) == target_width;
        }

        DWRITE_FONT_STYLE target_style{DWRITE_FONT_STYLE_ITALIC};
        if (!italic)
        {
            target_style = font_style(baseline.axes, baseline.face.Get());
        }

        if (!width_matches ||
            find_axis_value(axes, DWRITE_FONT_AXIS_TAG_WEIGHT).value_or(static_cast<float>(chosen->GetWeight())) != target_weight ||
            font_style(axes, chosen.Get()) != target_style)
        {
            continue;
        }
        auto selection{make_selection(std::move(chosen))};
        if (!selection)
        {
            return std::unexpected{selection.error()};
        }
        return &resource.selections[selection_index].emplace(std::move(*selection));
    }
    return &resource.selections[selection_index].emplace(baseline);
}

font_registry::font_registry(IDWriteFactory5* factory, Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> font_loader) :
    factory_{factory},
    font_loader_{std::move(font_loader)}
{
}

result<font_selection> font_registry::make_selection(Microsoft::WRL::ComPtr<IDWriteFontFace5> face)
{
    std::vector<DWRITE_FONT_AXIS_VALUE> axes(face->GetFontAxisValueCount());
    Microsoft::WRL::ComPtr<IDWriteFontResource> font_source;
    Microsoft::WRL::ComPtr<IDWriteFontFaceReference1> reference;
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> builder;
    Microsoft::WRL::ComPtr<IDWriteFontSet> font_set;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection;
    if (FAILED(face->GetFontAxisValues(axes.data(), static_cast<UINT32>(axes.size()))) ||
        FAILED(face->GetFontResource(&font_source)) ||
        FAILED(font_source->CreateFontFaceReference(DWRITE_FONT_SIMULATIONS_NONE, axes.data(), static_cast<UINT32>(axes.size()), &reference)) ||
        FAILED(factory_->CreateFontSetBuilder(&builder)) ||
        FAILED(builder->AddFontFaceReference(reference.Get())) ||
        FAILED(builder->CreateFontSet(&font_set)) ||
        FAILED(factory_->CreateFontCollectionFromFontSet(font_set.Get(), &collection)))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    auto name{family_name(face.Get())};
    if (!name)
    {
        return std::unexpected{name.error()};
    }
    return font_selection{std::move(face), std::move(collection), std::move(*name), std::move(axes)};
}
} // namespace nori::graphics::text
