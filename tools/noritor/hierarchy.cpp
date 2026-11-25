#include "pch.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <nori/asset.hpp>
#include <nori/core/encoding.hpp>
#include <nori/io.hpp>
#include <nori/resource/binary.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/operations.hpp>

#include "asset_manager.hpp"
#include "context.hpp"
#include "document_editor.hpp"
#include "file_dialog.hpp"
#include "hierarchy.hpp"
#include "import_workflow.hpp"
#include "inspector.hpp"
#include "resource_helpers.hpp"
#include "utils.hpp"

namespace
{
using nori::editor::asset_manager;
using nori::editor::document_editor;

std::vector<nori::resource::handle> ordered_drag_handles(document_editor& source, nori::resource::handle dragged)
{
    if (!source.selection().contains(dragged))
    {
        return {dragged};
    }

    const std::vector<nori::resource::handle> top_level{source.selection().top_level()};
    const std::unordered_set<nori::resource::handle> selected(top_level.begin(), top_level.end());
    std::vector<nori::resource::handle> ordered;
    const auto collect = [&selected, &ordered](this auto& self, nori::resource::handle parent) -> void
    {
        for (const nori::resource::handle child : nori::editor::children_of_loaded(parent))
        {
            if (selected.contains(child))
            {
                ordered.push_back(child);
                continue;
            }
            self(child);
        }
    };
    collect(source.root());
    return ordered;
}

std::size_t insertion_position(nori::resource::handle parent, std::span<const nori::resource::handle> dragged, nori::resource::handle anchor = {})
{
    const std::unordered_set<nori::resource::handle> excluded(dragged.begin(), dragged.end());
    std::size_t position{};
    for (const nori::resource::handle child : nori::editor::children_of_loaded(parent))
    {
        if (anchor && child == anchor)
        {
            return position;
        }
        if (!excluded.contains(child))
        {
            ++position;
        }
    }
    return position;
}

void draw_node_content(const asset_manager& assets, std::string_view image_path, std::string_view label, float row_cursor_x, bool is_root_node, ImVec2 item_min, ImVec2 item_size)
{
    const ImGuiStyle& style{ImGui::GetStyle()};
    const float font_size{ImGui::GetFontSize()};
    const float root_scale{is_root_node ? 1.2f : 1.0f};
    const float text_size{font_size * root_scale};
    const ImVec2 icon_size{16.0f, 16.0f};
    const ImVec2 icon_min{
        row_cursor_x + font_size + style.FramePadding.x * 2.0f,
        item_min.y + (item_size.y - icon_size.y) * 0.5f
    };

    ImVec2 text_pos{icon_min.x, item_min.y + (item_size.y - text_size) * 0.5f};
    if (!image_path.empty())
    {
        const ImVec2 cursor{ImGui::GetCursorScreenPos()};
        ImGui::SetCursorScreenPos(icon_min);
        assets.image(image_path);
        ImGui::SetCursorScreenPos(cursor);
        text_pos.x += icon_size.x + style.ItemInnerSpacing.x;
    }
    ImGui::GetWindowDrawList()->AddText(
        ImGui::GetFont(),
        text_size,
        text_pos,
        ImGui::GetColorU32(ImGuiCol_Text),
        label.data(),
        label.data() + label.size()
    );
}
} // namespace

namespace nori::editor
{
hierarchy::hierarchy(context* ctx, file_dialog* dialog, const asset_manager* assets, import_workflow* importer) :
    context_{ctx},
    file_dialog_{dialog},
    assets_{assets},
    importer_{importer},
    focus_search_document_{},
    remove_request_document_{}
{
}

void hierarchy::select_all(document_editor& doc)
{
    std::vector<nori::resource::handle> handles;
    const auto collect = [&handles](this auto& self, nori::resource::handle parent) -> void
    {
        for (const nori::resource::handle child : nori::editor::children_of_loaded(parent))
        {
            handles.push_back(child);
            self(child);
        }
    };
    collect(doc.root());
    doc.selection().select_all(handles);
}

void hierarchy::remove_selection(document_editor& target_document)
{
    if (target_document.closing())
    {
        return;
    }
    if (const auto res{target_document.remove(context_->temporary())}; !res)
    {
        report(res.error());
    }
}

void hierarchy::copy_selection(document_editor& target_document)
{
    if (!target_document.closing())
    {
        if (const auto res{target_document.copy_to(context_->clipboard())}; !res)
        {
            report(res.error());
        }
    }
}

void hierarchy::paste_selection(document_editor& target_document)
{
    if (target_document.closing())
    {
        return;
    }
    if (context_->clipboard().empty())
    {
        context_->set_status("클립보드가 비어 있습니다.");
        return;
    }
    nori::resource::handle parent{target_document.root()};
    if (const auto selected{target_document.selection().selected()})
    {
        parent = selected;
    }
    if (const auto res{target_document.paste(context_->clipboard(), parent)}; !res)
    {
        report(res.error());
    }
    else
    {
        context_->workspace(target_document.root()).hierarchy().open_nodes.insert(parent);
    }
}

void hierarchy::duplicate_selection(document_editor& target_document)
{
    if (target_document.closing())
    {
        return;
    }
    if (const auto res{target_document.duplicate(context_->temporary())}; !res)
    {
        report(res.error());
    }
}

void hierarchy::begin_rename(document_editor& doc)
{
    if (doc.closing() || !doc.selection().selected())
    {
        return;
    }
    context_->workspace(doc.root()).inspector().focus_name = true;
}

void hierarchy::focus_search(document_editor& doc)
{
    context_->documents().set_active(doc.root());
    focus_search_document_ = doc.root();
}

void hierarchy::draw(document_editor& doc)
{
    render_content(doc);
    if (remove_request_document_ == doc.root())
    {
        remove_request_document_ = {};
        remove_selection(doc);
    }
}

void hierarchy::draw_dialog()
{
    if (image_request_)
    {
        const file_dialog::result result{file_dialog_->render("새 이미지##hierarchy", *assets_)};
        if (result.state != file_dialog::status::selected)
        {
            if (result.state != file_dialog::status::pending)
            {
                image_request_.reset();
            }
            return;
        }

        resource_request_target request{std::move(*image_request_)};
        image_request_.reset();
        importer_->start_image(result.path, std::move(request));
        return;
    }
    if (!model_request_)
    {
        return;
    }

    const file_dialog::result result{file_dialog_->render("FBX 모델 가져오기##hierarchy", *assets_)};
    if (result.state != file_dialog::status::selected)
    {
        if (result.state != file_dialog::status::pending)
        {
            model_request_.reset();
        }
        return;
    }

    resource_request_target request{std::move(*model_request_)};
    model_request_.reset();
    importer_->start_model(result.path, std::move(request));
}

void hierarchy::create(document_editor& target_document, nori::resource::handle parent, nori::resource::property_type type)
{
    if (target_document.closing())
    {
        return;
    }
    if (type == nori::resource::property_type::image)
    {
        request_new_image(target_document, parent);
        return;
    }
    const std::string name{target_document.unique_child_name(parent)};
    property_snapshot value;
    switch (type)
    {
    case nori::resource::property_type::empty:
    {
        value = std::monostate{};
        break;
    }
    case nori::resource::property_type::boolean:
    {
        value = false;
        break;
    }
    case nori::resource::property_type::int32:
    {
        value = std::int32_t{};
        break;
    }
    case nori::resource::property_type::int64:
    {
        value = std::int64_t{};
        break;
    }
    case nori::resource::property_type::float32:
    {
        value = 0.0f;
        break;
    }
    case nori::resource::property_type::string:
    {
        value = std::string{};
        break;
    }
    case nori::resource::property_type::binary:
    {
        value = nori::resource::binary{};
        break;
    }
    default:
        return;
    }
    if (const auto effect{target_document.create(parent, name, std::move(value))}; !effect)
    {
        report(effect.error());
        return;
    }
    context_->workspace(target_document.root()).hierarchy().open_nodes.insert(parent);
}

void hierarchy::cut_selection(document_editor& target_document)
{
    if (!target_document.closing())
    {
        if (const auto res{target_document.cut(context_->clipboard())}; !res)
        {
            report(res.error());
        }
    }
}

void hierarchy::move_selection(document_editor& target_document, nori::resource::handle parent, std::optional<std::size_t> position)
{
    if (target_document.closing())
    {
        return;
    }
    if (!target_document.selection().selected())
    {
        return;
    }
    const std::vector<nori::resource::handle> selected{ordered_drag_handles(
        target_document,
        target_document.selection().selected()
    )};
    std::vector<move_command::entry> entries;
    for (std::size_t index{0}; index < selected.size(); ++index)
    {
        const nori::resource::handle handle{selected[index]};
        if (handle == target_document.root())
        {
            continue;
        }
        entries.push_back({handle, parent, position ? std::optional<std::size_t>{*position + index} : std::nullopt});
    }
    if (const auto res{target_document.move(std::move(entries))}; !res)
    {
        report(res.error());
        return;
    }
    context_->workspace(target_document.root()).hierarchy().open_nodes.insert(parent);
}

void hierarchy::clear_selection(document_editor& doc)
{
    doc.selection().clear();
}

void hierarchy::select(document_editor& doc, nori::resource::handle handle)
{
    context_->documents().set_active(doc.root());
    doc.selection().select(handle);
}

void hierarchy::toggle_selection(document_editor& doc, nori::resource::handle handle)
{
    doc.selection().toggle(handle);
}

bool hierarchy::is_selected(document_editor& doc, nori::resource::handle handle) const
{
    return doc.selection().contains(handle);
}

void hierarchy::render_content(document_editor& doc)
{
    render_filter_bar(doc);
    const ImGuiStyle& style{ImGui::GetStyle()};
    const float gap_height{style.ItemSpacing.y};
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{style.ItemSpacing.x, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_TreeLinesSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TreeLinesRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_TreeLines, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    bool first{true};
    for (const nori::resource::handle child : nori::editor::children_of_loaded(doc.root()))
    {
        if (!matches_filter(doc, child))
        {
            continue;
        }
        if (!first)
        {
            render_drop_gap(doc, doc.root(), child, gap_height);
        }
        render_node(doc, child, gap_height);
        first = false;
    }
    if (!first)
    {
        render_drop_gap(doc, doc.root(), {}, gap_height);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    const ImVec2 blank_size{ImGui::GetContentRegionAvail()};
    if (blank_size.x > 0.0f && blank_size.y > 0.0f)
    {
        ImGui::InvisibleButton("##hierarchy_blank_area", blank_size);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            clear_selection(doc);
        }
        if (ImGui::BeginPopupContextItem("HierarchyMenu", ImGuiPopupFlags_MouseButtonRight))
        {
            render_add_child_menu(doc, doc.root());
            if (ImGui::MenuItem("붙여넣기", "Ctrl+V"))
            {
                paste_selection(doc);
            }
            ImGui::EndPopup();
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
    {
        clear_selection(doc);
    }
}

void hierarchy::render_filter_bar(document_editor& doc)
{
    constexpr ImVec2 search_icon_size{16.0f, 16.0f};
    const float filter_bar_y{ImGui::GetCursorPosY()};
    const float icon_y{filter_bar_y + (ImGui::GetFrameHeight() - search_icon_size.y) * 0.5f};
    ImGui::SetCursorPosY(icon_y);
    assets_->image("16/search", search_icon_size);
    ImGui::SameLine();

    ImGui::SetCursorPosY(filter_bar_y);
    ImGui::SetNextItemWidth(-120.0f);
    if (focus_search_document_ == doc.root())
    {
        ImGui::SetKeyboardFocusHere();
        focus_search_document_ = {};
    }
    ImGui::InputTextWithHint("##hierarchy_filter", "이름 검색...", &context_->workspace(doc.root()).hierarchy().filter);
    ImGui::SameLine();

    const char* current_name{context_->workspace(doc.root()).hierarchy().type_filter ? utils::to_string(*context_->workspace(doc.root()).hierarchy().type_filter) : "전체"};
    ImGui::SetCursorPosY(filter_bar_y);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ImGui::ClearActiveID();
    }
    if (ImGui::BeginCombo("##hierarchy_type_filter", current_name))
    {
        if (ImGui::Selectable("전체", !context_->workspace(doc.root()).hierarchy().type_filter))
        {
            context_->workspace(doc.root()).hierarchy().type_filter.reset();
        }
        for (const nori::resource::property_type type : {nori::resource::property_type::empty, nori::resource::property_type::boolean, nori::resource::property_type::int32, nori::resource::property_type::int64, nori::resource::property_type::float32, nori::resource::property_type::string, nori::resource::property_type::image, nori::resource::property_type::binary})
        {
            if (ImGui::Selectable(utils::to_string(type), context_->workspace(doc.root()).hierarchy().type_filter == type))
            {
                context_->workspace(doc.root()).hierarchy().type_filter = type;
            }
        }
        ImGui::EndCombo();
    }
}

void hierarchy::render_node(document_editor& doc, nori::resource::handle handle, float gap_height)
{
    if (!matches_filter(doc, handle))
    {
        return;
    }
    const std::vector<nori::resource::handle> children{nori::editor::children_of_loaded(handle)};
    std::string label{nori::resource::name_of(handle)};
    if (label.empty())
    {
        label = "(이름 없음)";
    }
    std::string_view image_path;
    switch (nori::resource::type_of(handle))
    {
    case nori::resource::property_type::empty:
    {
        image_path = "16/folder";
        break;
    }
    case nori::resource::property_type::boolean:
    {
        image_path = "16/bool";
        break;
    }
    case nori::resource::property_type::int32:
    case nori::resource::property_type::int64:
    {
        image_path = "16/digit";
        break;
    }
    case nori::resource::property_type::float32:
    {
        image_path = "16/real";
        break;
    }
    case nori::resource::property_type::string:
    {
        image_path = "16/string";
        break;
    }
    case nori::resource::property_type::image:
    {
        image_path = "16/image";
        break;
    }
    case nori::resource::property_type::binary:
    {
        image_path = "16/binary";
        break;
    }
    default:
        break;
    }

    ImGui::PushID(static_cast<int>(handle.hier.index));
    ImGui::PushID(static_cast<int>(handle.hier.generation));
    ImGui::PushID(static_cast<int>(handle.node.index));
    ImGui::PushID(static_cast<int>(handle.node.generation));
    ImGuiTreeNodeFlags flags{
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanFullWidth |
        ImGuiTreeNodeFlags_DrawLinesToNodes
    };
    if (is_selected(doc, handle))
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }
    if ((!context_->workspace(doc.root()).hierarchy().filter.empty() || context_->workspace(doc.root()).hierarchy().type_filter) && !matches_self(doc, handle))
    {
        context_->workspace(doc.root()).hierarchy().open_nodes.insert(handle);
    }
    ImGui::SetNextItemOpen(is_open(handle));
    const float row_cursor_x{ImGui::GetCursorScreenPos().x};
    const bool is_opened{ImGui::TreeNodeEx("##node", flags)};
    const ImVec2 item_min{ImGui::GetItemRectMin()};
    const ImVec2 item_size{ImGui::GetItemRectSize()};
    if (ImGui::IsItemClicked())
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            toggle_selection(doc, handle);
        }
        else
        {
            select(doc, handle);
        }
    }
    if (ImGui::IsItemToggledOpen())
    {
        set_open(handle, is_opened);
    }

    if (ImGui::BeginDragDropSource())
    {
        if (!is_selected(doc, handle))
        {
            select(doc, handle);
        }
        ImGui::SetDragDropPayload("NORITOR_HIERARCHY_NODE", &handle, sizeof(handle));
        ImGui::Text("%zu개 항목", doc.selection().selection().size());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        constexpr ImGuiDragDropFlags accept_flags{
            ImGuiDragDropFlags_AcceptBeforeDelivery |
            ImGuiDragDropFlags_AcceptNoDrawDefaultRect
        };
        if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("NORITOR_HIERARCHY_NODE", accept_flags)})
        {
            const nori::resource::handle dragged{*static_cast<const nori::resource::handle*>(payload->Data)};
            const auto source_document{context_->documents().find(dragged)};
            if (source_document)
            {
                const std::vector<nori::resource::handle> handles{ordered_drag_handles(*source_document, dragged)};
                if (payload->IsPreview())
                {
                    const ImU32 color{ImGui::GetColorU32(ImGuiCol_DragDropTarget)};
                    ImGui::GetWindowDrawList()->AddRect(item_min, ImVec2{item_min.x + item_size.x, item_min.y + item_size.y}, color, 0.0f, 2.0f, ImDrawFlags_None);
                }
                if (payload->IsDelivery())
                {
                    handle_drop(dragged, handle, insertion_position(handle, handles));
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ImGui::GetStyle().ItemSpacing.x, gap_height});
    if (ImGui::BeginPopupContextItem("NodeMenu"))
    {
        if (!is_selected(doc, handle))
        {
            select(doc, handle);
        }
        render_add_child_menu(doc, handle);
        ImGui::Separator();
        if (ImGui::MenuItem("이름 변경", "F2"))
        {
            begin_rename(doc);
        }
        if (ImGui::MenuItem("복제", "Ctrl+D"))
        {
            duplicate_selection(doc);
        }
        if (ImGui::MenuItem("복사", "Ctrl+C"))
        {
            copy_selection(doc);
        }
        if (ImGui::MenuItem("붙여넣기", "Ctrl+V"))
        {
            paste_selection(doc);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("삭제"))
        {
            remove_request_document_ = doc.root();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    draw_node_content(*assets_, image_path, label, row_cursor_x, false, item_min, item_size);

    if (is_opened)
    {
        bool first{true};
        for (nori::resource::handle child : children)
        {
            if (!matches_filter(doc, child))
            {
                continue;
            }
            render_drop_gap(doc, handle, child, gap_height);
            render_node(doc, child, gap_height);
            first = false;
        }
        if (!first)
        {
            render_drop_gap(doc, handle, {}, gap_height);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void hierarchy::render_drop_gap(document_editor& doc, nori::resource::handle parent, nori::resource::handle anchor, float gap_height)
{
    ImGui::PushID(static_cast<int>(parent.hier.index));
    ImGui::PushID(static_cast<int>(parent.hier.generation));
    ImGui::PushID(static_cast<int>(parent.node.index));
    ImGui::PushID(static_cast<int>(parent.node.generation));
    ImGui::PushID(anchor ? static_cast<int>(anchor.node.index) : -1);
    ImGui::PushID(anchor ? static_cast<int>(anchor.node.generation) : -1);
    ImGui::InvisibleButton(anchor ? "##hierarchy_drop_gap" : "##hierarchy_drop_gap_end", ImVec2{ImGui::GetContentRegionAvail().x, gap_height});

    const bool filtered{!context_->workspace(doc.root()).hierarchy().filter.empty() || context_->workspace(doc.root()).hierarchy().type_filter.has_value()};
    if (!filtered && ImGui::BeginDragDropTarget())
    {
        constexpr ImGuiDragDropFlags accept_flags{
            ImGuiDragDropFlags_AcceptBeforeDelivery |
            ImGuiDragDropFlags_AcceptNoDrawDefaultRect
        };
        if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("NORITOR_HIERARCHY_NODE", accept_flags)})
        {
            const nori::resource::handle dragged{*static_cast<const nori::resource::handle*>(payload->Data)};
            if (const auto source_document{context_->documents().find(dragged)})
            {
                const std::vector<nori::resource::handle> handles{ordered_drag_handles(*source_document, dragged)};
                const std::size_t position{insertion_position(parent, handles, anchor)};
                if (payload->IsPreview())
                {
                    const ImVec2 item_min{ImGui::GetItemRectMin()};
                    const ImVec2 item_size{ImGui::GetItemRectSize()};
                    const ImU32 color{ImGui::GetColorU32(ImGuiCol_DragDropTarget)};
                    const float y{item_min.y + item_size.y * 0.5f};
                    ImGui::GetWindowDrawList()->AddLine(
                        ImVec2{item_min.x, y},
                        ImVec2{item_min.x + item_size.x, y},
                        color,
                        2.0f
                    );
                }
                if (payload->IsDelivery())
                {
                    handle_drop(dragged, parent, position);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void hierarchy::render_add_child_menu(document_editor& doc, nori::resource::handle parent)
{
    if (!ImGui::BeginMenu("추가"))
    {
        return;
    }
    for (const auto& [type, name] : std::views::zip(utils::data_types, utils::data_type_names))
    {
        if (!ImGui::MenuItem(name))
        {
            continue;
        }
        create(doc, parent, type);
    }
    if (ImGui::MenuItem("FBX 모델 가져오기"))
    {
        request_import_model(doc, parent);
    }
    ImGui::EndMenu();
}

bool hierarchy::matches_self(document_editor& doc, nori::resource::handle handle) const
{
    if (context_->workspace(doc.root()).hierarchy().type_filter && nori::resource::type_of(handle) != *context_->workspace(doc.root()).hierarchy().type_filter)
    {
        return false;
    }
    if (context_->workspace(doc.root()).hierarchy().filter.empty())
    {
        return true;
    }
    const std::string filter{utils::to_lower(context_->workspace(doc.root()).hierarchy().filter)};
    const std::string name{utils::to_lower(nori::resource::name_of(handle))};
    return name.find(filter) != std::string::npos;
}

bool hierarchy::matches_filter(document_editor& doc, nori::resource::handle handle) const
{
    if (matches_self(doc, handle))
    {
        return true;
    }
    for (const nori::resource::handle child : nori::editor::children_of_loaded(handle))
    {
        if (matches_filter(doc, child))
        {
            return true;
        }
    }
    return false;
}

bool hierarchy::is_open(nori::resource::handle handle) const
{
    if (const document_editor* const doc{context_->documents().find(handle)})
    {
        return context_->workspace(doc->root()).hierarchy().open_nodes.contains(handle);
    }
    return false;
}

void hierarchy::set_open(nori::resource::handle handle, bool is_open)
{
    if (is_open)
    {
        if (document_editor* const doc{context_->documents().find(handle)})
        {
            context_->workspace(doc->root()).hierarchy().open_nodes.insert(handle);
        }
    }
    else
    {
        if (document_editor* const doc{context_->documents().find(handle)})
        {
            context_->workspace(doc->root()).hierarchy().open_nodes.erase(handle);
        }
    }
}

void hierarchy::handle_drop(nori::resource::handle dragged, nori::resource::handle parent, std::optional<std::size_t> position)
{
    document_editor* const source_document{context_->documents().find(dragged)};
    document_editor* const destination_document{context_->documents().find(parent)};
    if (!source_document || !destination_document || source_document->closing() || destination_document->closing())
    {
        return;
    }

    if (source_document->root() == destination_document->root())
    {
        if (!source_document->selection().contains(dragged))
        {
            source_document->selection().select(dragged);
        }
        move_selection(*source_document, parent, position);
        return;
    }

    const std::vector<nori::resource::handle> selected{ordered_drag_handles(*source_document, dragged)};
    std::vector<copy_source> sources;
    sources.reserve(selected.size());
    for (std::size_t index{0}; index < selected.size(); ++index)
    {
        const nori::resource::handle source{selected[index]};
        sources.push_back({source, parent, nori::resource::name_of(source), position ? std::optional<std::size_t>{*position + index} : std::nullopt});
    }
    const auto effect{destination_document->copy(context_->temporary(), std::move(sources))};
    if (!effect)
    {
        report(effect.error());
        return;
    }
    context_->workspace(destination_document->root()).hierarchy().open_nodes.insert(parent);
}

void hierarchy::request_new_image(document_editor& target_document, nori::resource::handle parent)
{
    const auto path{relative_property_path(target_document.root(), parent)};
    if (!path)
    {
        report(path.error());
        return;
    }
    image_request_ = resource_request_target{target_document.root(), parent, *path};
    file_dialog_->open(
        "새 이미지##hierarchy",
        {".dds", ".tga", ".bmp", ".png", ".jpg", ".jpeg", ".gif", ".tif", ".tiff", ".jxr", ".wdp", ".hdp"}
    );
}

void hierarchy::request_import_model(document_editor& target_document, nori::resource::handle parent)
{
    const auto path{relative_property_path(target_document.root(), parent)};
    if (!path)
    {
        report(path.error());
        return;
    }
    model_request_ = resource_request_target{target_document.root(), parent, *path};
    file_dialog_->open("FBX 모델 가져오기##hierarchy", {".fbx"});
}

void hierarchy::report(nori::resource::error_code error)
{
    context_->set_status(std::string{nori::resource::to_string(error)});
}
} // namespace nori::editor
