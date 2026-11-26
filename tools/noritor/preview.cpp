#include "pch.hpp"

#include <algorithm>
#include <format>
#include <string>

#include <imgui.h>

#include <nori/graphics/imgui.hpp>
#include <nori/graphics/operations.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/operations.hpp>
#include <nori/resource/value.hpp>

#include "document_editor.hpp"
#include "document_workspace.hpp"
#include "preview.hpp"

namespace
{
constexpr float default_zoom{1.0f};
constexpr float minimum_zoom{0.1f};
constexpr float maximum_zoom{8.0f};
constexpr float zoom_step{0.1f};
constexpr float scroll_step_multiplier{5.0f};
} // namespace

namespace nori::editor
{
nori::graphics::result<void> preview::update(document_editor& doc, document_workspace& workspace)
{
    preview_state& preview_data{workspace.preview()};
    const nori::resource::handle selected{doc.selection().selected()};
    if (!selected || nori::resource::type_of(selected) != nori::resource::property_type::image)
    {
        release(workspace);
        return {};
    }
    if (preview_data.texture && preview_data.source == selected && !preview_data.refresh)
    {
        return {};
    }
    const bool source_changed{preview_data.source != selected};
    preview_data.texture.reset();
    const auto image{nori::resource::try_get<nori::resource::image_view>(selected)};
    if (!image)
    {
        if (source_changed)
        {
            preview_data.source = selected;
            preview_data.image_size = {};
            preview_data.zoom = default_zoom;
            preview_data.reset_scroll = true;
        }
        preview_data.refresh = true;
        return std::unexpected{nori::graphics::error_code::invalid_argument};
    }
    preview_data.source = selected;
    preview_data.image_size = {static_cast<float>(image->width), static_cast<float>(image->height)};
    if (source_changed)
    {
        preview_data.zoom = default_zoom;
        preview_data.reset_scroll = true;
    }
    auto texture{nori::graphics::create_texture(selected)};
    if (!texture)
    {
        preview_data.refresh = true;
        return std::unexpected{texture.error()};
    }
    preview_data.texture = std::move(*texture);
    preview_data.refresh = false;
    return {};
}

void preview::release(document_workspace& workspace) const
{
    preview_state& preview_data{workspace.preview()};
    preview_data.source = {};
    preview_data.texture.reset();
    preview_data.image_size = {};
    preview_data.zoom = default_zoom;
    preview_data.reset_scroll = true;
    preview_data.refresh = false;
}

void preview::draw(document_editor& doc, document_workspace& workspace)
{
    if (!doc.selection().selected() || doc.selection().selected() != workspace.preview().source || !workspace.preview().texture)
    {
        return;
    }
    preview_state& preview_data{workspace.preview()};

    const nori::resource::handle root{doc.root()};
    const std::string child_name{std::format("image_view##image_view_{}_{}_{}_{}", root.hier.index, root.hier.generation, root.node.index, root.node.generation)};
    if (!ImGui::BeginChild(child_name.c_str(), ImVec2{}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::EndChild();
        return;
    }

    const ImVec2 available{ImGui::GetContentRegionAvail()};
    if (preview_data.image_size.x > 0.0f && preview_data.image_size.y > 0.0f && available.x >= 1.0f && available.y >= 1.0f)
    {
        const auto size{preview_data.image_size * preview_data.zoom};
        const ImVec2 image_offset{
            std::max((available.x - size.x) * 0.5f, 0.0f),
            std::max((available.y - size.y) * 0.5f, 0.0f),
        };
        const ImVec2 cursor{ImGui::GetCursorPos()};
        ImGui::SetCursorPos(cursor + image_offset);
        const ImVec2 image_screen_pos{ImGui::GetCursorScreenPos()};
        ImGui::Image(nori::graphics::imgui::texture_id(preview_data.texture.get()), {size.x, size.y});

        if (preview_data.reset_scroll)
        {
            ImGui::SetScrollX(0.0f);
            ImGui::SetScrollY(0.0f);
            preview_data.reset_scroll = false;
        }

        const ImGuiIO& io{ImGui::GetIO()};
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
        {
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl))
            {
                const float new_zoom{std::clamp(preview_data.zoom + io.MouseWheel * zoom_step, minimum_zoom, maximum_zoom)};
                if (new_zoom != preview_data.zoom)
                {
                    const ImVec2 mouse_pos{ImGui::GetMousePos()};
                    const ImVec2 mouse_offset{mouse_pos - image_screen_pos};
                    const ImVec2 image_coordinate{mouse_offset / preview_data.zoom};
                    const auto new_size{preview_data.image_size * new_zoom};
                    const ImVec2 new_image_offset{
                        std::max((available.x - new_size.x) * 0.5f, 0.0f),
                        std::max((available.y - new_size.y) * 0.5f, 0.0f),
                    };
                    const ImVec2 image_content_pos{image_screen_pos.x + ImGui::GetScrollX(), image_screen_pos.y + ImGui::GetScrollY()};
                    const ImVec2 new_image_content_pos{image_content_pos + new_image_offset - image_offset};

                    preview_data.zoom = new_zoom;
                    ImGui::SetScrollX(std::max(new_image_content_pos.x - mouse_pos.x + image_coordinate.x * new_zoom, 0.0f));
                    ImGui::SetScrollY(std::max(new_image_content_pos.y - mouse_pos.y + image_coordinate.y * new_zoom, 0.0f));
                }
            }
            else
            {
                const float scroll_step{ImGui::GetFontSize() * scroll_step_multiplier};
                if (ImGui::IsKeyDown(ImGuiMod_Shift))
                {
                    ImGui::SetScrollX(std::clamp(ImGui::GetScrollX() - io.MouseWheel * scroll_step, 0.0f, ImGui::GetScrollMaxX()));
                }
                else
                {
                    ImGui::SetScrollY(std::clamp(ImGui::GetScrollY() - io.MouseWheel * scroll_step, 0.0f, ImGui::GetScrollMaxY()));
                }
            }
        }
    }

    const std::string zoom_label{std::format("배율: {:.0f}%", preview_data.zoom * 100.0f)};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos() + ImGui::GetStyle().WindowPadding, ImGui::GetColorU32(ImGuiCol_Text), zoom_label.c_str());
    ImGui::EndChild();
}

bool preview::has_content(document_editor& doc, document_workspace& workspace) const
{
    if (!doc.selection().selected())
    {
        return false;
    }
    if (doc.selection().selected() != workspace.preview().source)
    {
        return false;
    }
    if (!workspace.preview().texture)
    {
        return false;
    }
    return true;
}
} // namespace nori::editor
