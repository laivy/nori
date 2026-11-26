#include "pch.hpp"

#include <array>

#include <nori/asset.hpp>
#include <nori/graphics/imgui.hpp>
#include <nori/io.hpp>

#include "asset_manager.hpp"
#include "context.hpp"
#include "file_dialog.hpp"
#include "import_workflow.hpp"
#include "inspector.hpp"
#include "resource_helpers.hpp"
#include "utils.hpp"

namespace
{
enum class property_category
{
    basic_info,
    value
};

enum class property_field
{
    // property_category::basic_info
    name,
    type,

    // property_category::value
    value,
    child_count,
    image_dimensions,
    image_format,
    image_mip_levels,
    image_replace,
    binary_size
};

enum class property_kind
{
    boolean,
    int32,
    int64,
    float32,
    string,
    read_only
};

struct property_descriptor
{
    property_category category;
    property_field field;
    property_kind kind;
    bool editable;
};

constexpr std::array resource_types{
    nori::resource::property_type::empty,
    nori::resource::property_type::boolean,
    nori::resource::property_type::int32,
    nori::resource::property_type::int64,
    nori::resource::property_type::float32,
    nori::resource::property_type::string,
    nori::resource::property_type::image,
    nori::resource::property_type::binary
};

constexpr const char* category_label(property_category category)
{
    switch (category)
    {
    case property_category::basic_info:
    {
        return "기본 정보";
    }
    case property_category::value:
    {
        return "값";
    }
    default:
        break;
    }
    return "";
}

constexpr const char* property_label(property_field field)
{
    switch (field)
    {
    case property_field::name:
    {
        return "이름";
    }
    case property_field::type:
    {
        return "타입";
    }
    case property_field::value:
    {
        return "값";
    }
    case property_field::child_count:
    {
        return "자식 개수";
    }
    case property_field::image_dimensions:
    {
        return "크기";
    }
    case property_field::image_format:
    {
        return "포맷";
    }
    case property_field::image_mip_levels:
    {
        return "밉 레벨";
    }
    case property_field::image_replace:
    {
        return "파일";
    }
    case property_field::binary_size:
    {
        return "크기";
    }
    default:
        break;
    }
    return "";
}

property_kind value_kind(nori::resource::property_type type)
{
    switch (type)
    {
    case nori::resource::property_type::boolean:
    {
        return property_kind::boolean;
    }
    case nori::resource::property_type::int32:
    {
        return property_kind::int32;
    }
    case nori::resource::property_type::int64:
    {
        return property_kind::int64;
    }
    case nori::resource::property_type::float32:
    {
        return property_kind::float32;
    }
    case nori::resource::property_type::string:
    {
        return property_kind::string;
    }
    default:
        break;
    }
    return property_kind::read_only;
}

std::vector<property_descriptor> describe(nori::resource::property_type type, bool single_selection, bool same_type)
{
    std::vector<property_descriptor> result{{property_category::basic_info, property_field::type, property_kind::read_only, true}};
    if (single_selection)
    {
        result.insert(result.begin(), {property_category::basic_info, property_field::name, property_kind::string, true});
    }
    if (!same_type)
    {
        return result;
    }

    switch (type)
    {
    case nori::resource::property_type::empty:
    {
        if (single_selection)
        {
            result.push_back({property_category::value, property_field::child_count, property_kind::read_only, false});
        }
        break;
    }
    case nori::resource::property_type::boolean:
    case nori::resource::property_type::int32:
    case nori::resource::property_type::int64:
    case nori::resource::property_type::float32:
    case nori::resource::property_type::string:
    {
        result.push_back({property_category::value, property_field::value, value_kind(type), true});
        break;
    }
    case nori::resource::property_type::image:
    {
        result.push_back({property_category::value, property_field::image_dimensions, property_kind::read_only, false});
        result.push_back({property_category::value, property_field::image_format, property_kind::read_only, false});
        result.push_back({property_category::value, property_field::image_mip_levels, property_kind::read_only, false});
        if (single_selection)
        {
            result.push_back({property_category::value, property_field::image_replace, property_kind::read_only, true});
        }
        break;
    }
    case nori::resource::property_type::binary:
    {
        result.push_back({property_category::value, property_field::binary_size, property_kind::read_only, false});
        break;
    }
    default:
        break;
    }
    return result;
}

std::string image_format_name(nori::resource::image_format format)
{
    switch (format)
    {
    case nori::resource::image_format::rgba8_unorm:
    {
        return "rgba8_unorm";
    }
    case nori::resource::image_format::bc5_unorm:
    {
        return "bc5_unorm";
    }
    case nori::resource::image_format::bc7_unorm:
    {
        return "bc7_unorm";
    }
    case nori::resource::image_format::bc7_unorm_srgb:
    {
        return "bc7_unorm_srgb";
    }
    default:
        break;
    }
    return "unknown";
}
} // namespace

namespace nori::editor
{
inspector::inspector(context* ctx, file_dialog* dialog, const asset_manager* assets, import_workflow* importer) :
    context_{ctx},
    file_dialog_{dialog},
    assets_{assets},
    importer_{importer}
{
}

void inspector::draw(document_editor& doc)
{
    const std::span<const nori::resource::handle> selection{doc.selection().selection()};
    if (selection.empty() || !doc.selection().selected())
    {
        ImGui::TextDisabled("선택된 항목 없음");
        return;
    }

    const nori::resource::handle primary{doc.selection().selected()};
    const nori::resource::property_type primary_type{nori::resource::type_of(primary)};
    const bool same_type{std::ranges::all_of(selection, [primary_type](nori::resource::handle handle)
    {
        return nori::resource::type_of(handle) == primary_type;
    })};
    const bool single_selection{selection.size() == 1};
    const std::vector<property_descriptor> properties{describe(primary_type, single_selection, same_type)};

    ImGui::PushID(static_cast<int>(primary.hier.index));
    ImGui::PushID(static_cast<int>(primary.hier.generation));
    ImGui::PushID(static_cast<int>(primary.node.index));
    ImGui::PushID(static_cast<int>(primary.node.generation));
    if (!single_selection)
    {
        ImGui::Text("%zu개 항목 선택", selection.size());
    }

    std::optional<nori::resource::property_type> pending_type_change;

    const auto draw_property = [&, this](property_descriptor property)
    {
        bool modified{false};
        switch (property.field)
        {
        case property_field::name:
        {
            std::string value{nori::resource::name_of(primary)};
            if (context_->workspace(doc.root()).inspector().focus_name)
            {
                ImGui::SetKeyboardFocusHere();
                context_->workspace(doc.root()).inspector().focus_name = false;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##name", &value, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                rename(doc, primary, std::move(value));
            }
            break;
        }
        case property_field::type:
        {
            const char* const preview{same_type ? utils::to_string(primary_type) : "혼합"};
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##type", preview))
            {
                for (const nori::resource::property_type type : resource_types)
                {
                    const bool selectable{is_primitive_type(type)};
                    if (!selectable)
                    {
                        ImGui::BeginDisabled();
                    }
                    const bool selected{same_type && type == primary_type};
                    if (ImGui::Selectable(utils::to_string(type), selected) && selectable && (!same_type || type != primary_type))
                    {
                        pending_type_change = type;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (!selectable)
                    {
                        ImGui::EndDisabled();
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case property_field::value:
        {
            switch (property.kind)
            {
            case property_kind::boolean:
            {
                bool value{nori::resource::get<bool>(primary)};
                modified = ImGui::Checkbox("##value", &value);
                if (modified)
                {
                    set_value(doc, selection, value);
                }
                break;
            }
            case property_kind::int32:
            {
                std::int32_t value{nori::resource::get<std::int32_t>(primary)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                modified = ImGui::InputScalar("##value", ImGuiDataType_S32, &value);
                if (modified)
                {
                    set_value(doc, selection, value);
                }
                break;
            }
            case property_kind::int64:
            {
                std::int64_t value{nori::resource::get<std::int64_t>(primary)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                modified = ImGui::InputScalar("##value", ImGuiDataType_S64, &value);
                if (modified)
                {
                    set_value(doc, selection, value);
                }
                break;
            }
            case property_kind::float32:
            {
                float value{nori::resource::get<float>(primary)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                modified = ImGui::InputFloat("##value", &value, 0.0f, 0.0f, "%.6f");
                if (modified)
                {
                    set_value(doc, selection, value);
                }
                break;
            }
            case property_kind::string:
            {
                std::string value{nori::resource::get<std::string>(primary)};
                const bool mixed{std::ranges::any_of(selection, [&value](nori::resource::handle handle)
                {
                    return nori::resource::get<std::string>(handle) != value;
                })};
                if (mixed)
                {
                    value.clear();
                }
                ImGui::SetNextItemWidth(-FLT_MIN);
                modified = ImGui::InputTextMultiline("##value", &value, ImVec2{-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f});
                if (modified)
                {
                    set_value(doc, selection, std::move(value));
                }
                break;
            }
            default:
                break;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                doc.break_merge();
            }
            break;
        }
        case property_field::child_count:
        {
            ImGui::Text("%zu개", nori::editor::children_of_loaded(primary).size());
            break;
        }
        case property_field::image_dimensions:
        {
            const nori::resource::image_view image{nori::resource::get<nori::resource::image_view>(primary)};
            ImGui::Text("%u x %u", image.width, image.height);
            break;
        }
        case property_field::image_format:
        {
            const nori::resource::image_view image{nori::resource::get<nori::resource::image_view>(primary)};
            ImGui::TextUnformatted(image_format_name(image.format).c_str());
            break;
        }
        case property_field::image_mip_levels:
        {
            const nori::resource::image_view image{nori::resource::get<nori::resource::image_view>(primary)};
            ImGui::Text("%u", image.mip_levels);
            break;
        }
        case property_field::image_replace:
        {
            if (ImGui::Button("이미지 교체...", ImVec2{-FLT_MIN, 0.0f}))
            {
                request_image_replacement(doc, primary);
            }
            break;
        }
        case property_field::binary_size:
        {
            const nori::resource::binary_view binary{nori::resource::get<nori::resource::binary_view>(primary)};
            ImGui::Text("%zu bytes", binary.data.size());
            break;
        }
        default:
            break;
        }
    };

    const auto draw_property_table = [&](auto&& category_properties, property_category category)
    {
        const bool has_table_properties{std::ranges::any_of(category_properties, [](const property_descriptor& property)
        {
            return !(property.field == property_field::value && property.editable);
        })};
        if (!has_table_properties)
        {
            return;
        }

        if (!ImGui::BeginTable(category_label(category), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
        {
            return;
        }
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        for (const property_descriptor& property : category_properties)
        {
            if (property.field == property_field::value && property.editable)
            {
                continue;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(property_label(property.field));
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(static_cast<int>(property.field));
            draw_property(property);
            ImGui::PopID();
        }
        ImGui::EndTable();
    };

    const auto draw_basic_info = [&]
    {
        auto category_properties{
            properties |
            std::views::filter([](const property_descriptor& property)
        {
            return property.category == property_category::basic_info;
        })
        };
        if (std::ranges::empty(category_properties))
        {
            return;
        }
        if (!ImGui::CollapsingHeader(category_label(property_category::basic_info), ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }
        draw_property_table(category_properties, property_category::basic_info);
    };

    const auto draw_value = [&]
    {
        auto category_properties{
            properties |
            std::views::filter([](const property_descriptor& property)
        {
            return property.category == property_category::value;
        })
        };
        if (std::ranges::empty(category_properties))
        {
            return;
        }
        if (!ImGui::CollapsingHeader(category_label(property_category::value), ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        for (const property_descriptor& property : category_properties)
        {
            if (property.field != property_field::value || !property.editable)
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(property.field));
            draw_property(property);
            ImGui::PopID();
            break;
        }
        draw_property_table(category_properties, property_category::value);
    };

    draw_basic_info();
    draw_value();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();

    if (pending_type_change)
    {
        set_type(doc, selection, *pending_type_change);
    }
}

void inspector::draw_dialog()
{
    if (!image_request_)
    {
        return;
    }
    const file_dialog::result dialog_result{file_dialog_->render("이미지 교체##inspector", *assets_)};
    if (dialog_result.state != file_dialog::status::selected)
    {
        if (dialog_result.state != file_dialog::status::pending)
        {
            image_request_.reset();
        }
        return;
    }

    resource_request_target request{std::move(*image_request_)};
    image_request_.reset();
    importer_->start_replacement(dialog_result.path, std::move(request));
}

void inspector::focus_name(document_editor& doc)
{
    context_->workspace(doc.root()).inspector().focus_name = true;
}

void inspector::rename(document_editor& target_document, nori::resource::handle target, std::string name)
{
    if (target_document.closing() || target == target_document.root())
    {
        return;
    }
    if (const auto res{target_document.set_name(target, std::move(name))}; !res)
    {
        report(res.error());
    }
}

void inspector::set_type(document_editor& target_document, std::span<const nori::resource::handle> targets, nori::resource::property_type type)
{
    if (target_document.closing() || !is_primitive_type(type))
    {
        return;
    }

    if (const auto res{target_document.set_type(targets, type)}; !res)
    {
        report(res.error());
    }
}

void inspector::set_value(document_editor& target_document, std::span<const nori::resource::handle> targets, property_snapshot value)
{
    if (target_document.closing())
    {
        return;
    }

    if (const auto result{target_document.set_value(targets, std::move(value))}; !result)
    {
        report(result.error());
    }
}

void inspector::request_image_replacement(document_editor& target_document, nori::resource::handle target)
{
    const auto path{relative_property_path(target_document.root(), target)};
    if (!path)
    {
        report(path.error());
        return;
    }
    image_request_ = resource_request_target{target_document.root(), target, *path};
    file_dialog_->open(
        "이미지 교체##inspector",
        {".dds", ".tga", ".bmp", ".png", ".jpg", ".jpeg", ".gif", ".tif", ".tiff", ".jxr", ".wdp", ".hdp"}
    );
}

void inspector::report(nori::resource::error_code error)
{
    context_->set_status(std::string{nori::resource::to_string(error)});
}
} // namespace nori::editor
