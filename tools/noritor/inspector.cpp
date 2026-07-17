#include "pch.hpp"
#include <nori/graphics/imgui_accessor.hpp>
#include "file_dialog.hpp"
#include "inspector.hpp"
#include "noritor_application.hpp"
#include "utils.hpp"

inspector::inspector(noritor_application& app) :
    app_{app},
    file_dialog_context_{}
{
}

void inspector::render()
{
    context& ctx{app_.get_context()};
    if (!ImGui::Begin("속성"))
    {
        ImGui::End();
        render_image_file_dialog();
        return;
    }

    if (!ctx.selected)
    {
        ImGui::TextDisabled("선택된 항목 없음");
        ImGui::End();
        render_image_file_dialog();
        return;
    }

    const nori::resource::handle handle{*ctx.selected};
    const nori::resource::type type{nori::resource::get_type(handle)};
    bool is_modified{false};

    ImGui::PushID(static_cast<int>(handle.index));

    std::string name{nori::resource::get_name(handle)};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("이름", &name, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        is_modified = nori::resource::set_name(handle, name).has_value();
    }

    ImGui::Separator();
    ImGui::Text("타입: %s", utils::to_string(type));
    switch (type)
    {
    case nori::resource::type::int32:
    {
        int value{nori::resource::get_int32(handle)};
        if (ImGui::InputScalar("값", ImGuiDataType_S32, &value))
        {
            nori::resource::set_value(handle, static_cast<std::int32_t>(value));
            is_modified = true;
        }
        break;
    }
    case nori::resource::type::int64:
    {
        std::int64_t value{nori::resource::get_int64(handle)};
        if (ImGui::InputScalar("값", ImGuiDataType_S64, &value))
        {
            nori::resource::set_value(handle, static_cast<std::int64_t>(value));
            is_modified = true;
        }
        break;
    }
    case nori::resource::type::float32:
    {
        float value{nori::resource::get_float(handle)};
        if (ImGui::InputFloat("값", &value, 0.0f, 0.0f, "%.6f"))
        {
            nori::resource::set_value(handle, value);
            is_modified = true;
        }
        break;
    }
    case nori::resource::type::string:
    {
        std::string value{nori::resource::get_string(handle)};
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextMultiline("값", &value, ImVec2{-FLT_MIN, ImGui::GetTextLineHeight() * 5.0f}))
        {
            nori::resource::set_value(handle, value);
            is_modified = true;
        }
        break;
    }
    case nori::resource::type::image:
    {
        const nori::resource::image img{nori::resource::get_image(handle)};
        ImGui::Text("%u x %u", img.width, img.height);
        const ImVec2 avail{ImGui::GetContentRegionAvail()};
        const float scale{
            img.width > 0 && img.height > 0 ? std::min(avail.x / img.width, avail.y / img.height) : 1.0f
        };
        const nori::core::float2 size{img.width * scale, img.height * scale};
        nori::graphics::imgui::image(handle, size);

        if (ImGui::Button("이미지 선택...", ImVec2{-FLT_MIN, 0.0f}))
        {
            file_dialog::open("이미지(.png)##Inspector", {".png"});
            file_dialog_context_ = {.handle = handle};
        }
        break;
    }
    default:
        ImGui::TextDisabled("폴더 노드는 편집할 값이 없습니다.");
        break;
    }

    if (is_modified)
    {
        app_.set_modified(handle);
    }

    ImGui::PopID();
    ImGui::End();
    render_image_file_dialog();
}

void inspector::render_image_file_dialog()
{
    const std::filesystem::path path{file_dialog::render("이미지(.png)##Inspector")};
    if (path.empty())
    {
        return;
    }
    const nori::resource::handle handle{file_dialog_context_.handle};
    const std::optional<nori::resource::image_asset> image{utils::load_png_image(path)};
    if (handle && image)
    {
        nori::resource::set_value(handle, *image);
        nori::graphics::imgui::invalidate_image(handle);
        app_.set_modified(handle);
        app_.select_resource(handle);
    }
    file_dialog_context_ = {};
}
