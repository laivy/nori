#include "pch.hpp"
#include "hierarchy.hpp"
#include "noritor_application.hpp"
#include "utils.hpp"

namespace
{
ImVec2 draw_node_icon(nori::resource::handle handle, const ImVec2& icon_min, float icon_length, bool is_root_node)
{
    const ImVec2 icon_size{icon_length, icon_length};
    const ImVec2 icon_max{icon_min + icon_size};
    ImDrawList* draw_list{ImGui::GetWindowDrawList()};
    const auto draw_centered_text = [draw_list, icon_min, icon_max](const char* text, ImU32 color)
    {
        const ImVec2 text_size{ImGui::CalcTextSize(text)};
        const ImVec2 text_pos{
            icon_min.x + (std::ceilf(icon_max.x - icon_min.x) - text_size.x) * 0.5f,
            icon_min.y + (std::ceilf(icon_max.y - icon_min.y) - text_size.y) * 0.5f
        };
        draw_list->AddText(text_pos, color, text);
    };

    if (is_root_node)
    {
        const ImU32 body_color{IM_COL32(74, 100, 137, 255)};
        const ImU32 label_color{IM_COL32(211, 218, 229, 255)};
        const ImU32 slot_color{IM_COL32(42, 53, 70, 255)};
        const ImVec2 body_min{icon_min + ImVec2{icon_size.x * 0.08f, icon_size.y * 0.05f}};
        const ImVec2 body_max{icon_min + ImVec2{icon_size.x * 0.92f, icon_size.y * 0.95f}};
        const ImVec2 slot_min{icon_min + ImVec2{icon_size.x * 0.22f, icon_size.y * 0.12f}};
        const ImVec2 slot_max{icon_min + ImVec2{icon_size.x * 0.70f, icon_size.y * 0.36f}};
        const ImVec2 notch_min{icon_min + ImVec2{icon_size.x * 0.70f, icon_size.y * 0.12f}};
        const ImVec2 notch_max{icon_min + ImVec2{icon_size.x * 0.82f, icon_size.y * 0.36f}};
        const ImVec2 label_min{icon_min + ImVec2{icon_size.x * 0.22f, icon_size.y * 0.58f}};
        const ImVec2 label_max{icon_min + ImVec2{icon_size.x * 0.78f, icon_size.y * 0.84f}};

        draw_list->AddRectFilled(body_min, body_max, body_color, 2.0f);
        draw_list->AddRectFilled(slot_min, slot_max, slot_color, 1.0f);
        draw_list->AddRectFilled(notch_min, notch_max, IM_COL32(154, 169, 190, 255), 1.0f);
        draw_list->AddRectFilled(label_min, label_max, label_color, 1.0f);
        return icon_max;
    }

    switch (nori::resource::get_type(handle))
    {
    case nori::resource::type::folder:
    {
        const ImVec2 tab_min{icon_min + ImVec2{icon_size.x * 0.08f, icon_size.y * 0.16f}};
        const ImVec2 tab_max{icon_min + ImVec2{icon_size.x * 0.46f, icon_size.y * 0.38f}};
        const ImVec2 body_min{icon_min + ImVec2{icon_size.x * 0.05f, icon_size.y * 0.30f}};
        const ImVec2 body_max{icon_min + ImVec2{icon_size.x * 0.95f, icon_size.y * 0.84f}};
        const ImU32 tab_color{IM_COL32(244, 196, 93, 255)};
        const ImU32 body_color{IM_COL32(224, 171, 63, 255)};

        draw_list->AddRectFilled(tab_min, tab_max, tab_color, 2.0f, ImDrawFlags_RoundCornersTop);
        draw_list->AddRectFilled(body_min, body_max, body_color, 2.0f);
        break;
    }
    case nori::resource::type::image:
    {
        draw_list->AddRectFilled(icon_min, icon_max, IM_COL32(71, 116, 165, 255), 2.0f);
        draw_list->AddCircleFilled(
            icon_min + ImVec2{icon_size.x * 0.72f, icon_size.y * 0.28f},
            icon_size.x * 0.11f,
            IM_COL32(245, 218, 126, 255)
        );
        draw_list->AddTriangleFilled(
            icon_min + ImVec2{icon_size.x * 0.12f, icon_size.y * 0.82f},
            icon_min + ImVec2{icon_size.x * 0.42f, icon_size.y * 0.45f},
            icon_min + ImVec2{icon_size.x * 0.72f, icon_size.y * 0.82f},
            IM_COL32(112, 174, 104, 255)
        );
        draw_list->AddRect(icon_min, icon_max, IM_COL32(178, 198, 219, 255), 2.0f);
        break;
    }
    case nori::resource::type::string:
    {
        draw_list->AddRectFilled(icon_min, icon_max, IM_COL32(112, 148, 98, 255), 2.0f);
        draw_centered_text("S", IM_COL32(235, 241, 232, 255));
        break;
    }
    case nori::resource::type::float32:
    {
        draw_list->AddRectFilled(icon_min, icon_max, IM_COL32(145, 105, 166, 255), 2.0f);
        draw_centered_text("F", IM_COL32(242, 235, 246, 255));
        break;
    }
    case nori::resource::type::int32:
    case nori::resource::type::int64:
    {
        draw_list->AddRectFilled(icon_min, icon_max, IM_COL32(178, 111, 82, 255), 2.0f);
        draw_centered_text("I", IM_COL32(248, 237, 231, 255));
        break;
    }
    default:
        draw_list->AddCircleFilled((icon_min + icon_max) * 0.5f, icon_size.x * 0.38f, IM_COL32(128, 135, 148, 255));
        break;
    }
    return icon_max;
}

void draw_node_content(nori::resource::handle handle, std::string_view label, float row_cursor_x, bool is_root_node)
{
    const ImGuiStyle& style{ImGui::GetStyle()};
    const ImVec2 item_min{ImGui::GetItemRectMin()};
    const ImVec2 item_size{ImGui::GetItemRectSize()};
    const float font_size{ImGui::GetFontSize()};
    const float root_scale{is_root_node ? 1.18f : 1.0f};
    const float text_size{font_size * root_scale};
    const float icon_length{font_size * (is_root_node ? 1.12f : 0.9f)};
    const float icon_start_x{row_cursor_x + font_size + style.FramePadding.x * 2.0f};
    const ImVec2 icon_min{icon_start_x, item_min.y + (item_size.y - icon_length) * 0.5f};
    const ImVec2 icon_max{draw_node_icon(handle, icon_min, icon_length, is_root_node)};
    const float icon_text_spacing{ImGui::CalcTextSize(" ").x};
    const ImVec2 text_pos{icon_max.x + icon_text_spacing, item_min.y + (item_size.y - text_size) * 0.5f};
    ImGui::GetWindowDrawList()->AddText(
        ImGui::GetFont(),
        text_size,
        text_pos,
        ImGui::GetColorU32(ImGuiCol_Text),
        label.data(),
        label.data() + label.size()
    );
}

void draw_insertion_preview(const ImVec2& min, const ImVec2& max)
{
    const ImGuiStyle& style{ImGui::GetStyle()};
    const float y{min.y + style.ItemSpacing.y * 0.5f};
    const ImVec2 line_min{min.x + style.FramePadding.x, y};
    const ImVec2 line_max{max.x - style.FramePadding.x, y};
    const ImU32 color{ImGui::GetColorU32(ImGuiCol_DragDropTarget)};
    ImDrawList* draw_list{ImGui::GetWindowDrawList()};
    draw_list->AddLine(line_min, line_max, color, 2.0f);
}
} // namespace

hierarchy::hierarchy(noritor_application& app) :
    app_{app},
    file_dialog_context_{}
{
}

void hierarchy::select(nori::resource::handle handle)
{
    context& ctx{app_.get_context()};
    ctx.selected = handle;
    selection_.clear();
    selection_.push_back(handle);
}

void hierarchy::open(nori::resource::handle handle)
{
    open_nodes_.insert(handle);
}

void hierarchy::clear_selection()
{
    context& ctx{app_.get_context()};
    ctx.selected.reset();
    selection_.clear();
}

void hierarchy::delete_selected()
{
    if (selection_.empty())
    {
        return;
    }

    std::vector<nori::resource::handle> targets{selection_};
    std::erase_if(
        targets,
        [this](nori::resource::handle handle)
        {
            return is_root(handle) || std::ranges::any_of(
                                          selection_,
                                          [this, handle](nori::resource::handle selected)
                                          {
                                              return selected != handle && is_descendant(handle, selected);
                                          }
                                      );
        }
    );

    for (nori::resource::handle selected : targets)
    {
        if (const nori::resource::handle parent{nori::resource::get_parent(selected)})
        {
            app_.set_modified(parent);
        }
        nori::resource::remove(selected);
        open_nodes_.erase(selected);
    }

    clear_selection();
}

void hierarchy::toggle_selection(nori::resource::handle handle)
{
    context& ctx{app_.get_context()};
    if (auto it{std::ranges::find(selection_, handle)}; it != selection_.end())
    {
        selection_.erase(it);
        if (ctx.selected == handle)
        {
            if (selection_.empty())
            {
                ctx.selected.reset();
            }
            else
            {
                ctx.selected = selection_.back();
            }
        }
        return;
    }

    selection_.push_back(handle);
    ctx.selected = handle;
}

void hierarchy::close_root(nori::resource::handle handle)
{
    context& ctx{app_.get_context()};
    if (!is_root(handle))
    {
        return;
    }

    std::erase_if(
        selection_,
        [this, handle](nori::resource::handle selected)
        {
            return selected == handle || is_descendant(selected, handle);
        }
    );
    if (ctx.selected && (*ctx.selected == handle || is_descendant(*ctx.selected, handle)))
    {
        ctx.selected.reset();
    }
    std::erase_if(
        ctx.roots,
        [handle](const context::root& root)
        {
            return root.handle == handle;
        }
    );
    docked_roots_.erase(handle);
    open_nodes_.erase(handle);
}

void hierarchy::move_selection_to(nori::resource::handle parent)
{
    if (selection_.empty())
    {
        return;
    }

    std::vector<nori::resource::handle> targets{selection_};
    std::erase_if(
        targets,
        [this, parent](nori::resource::handle handle)
        {
            return !can_move_to(handle, parent) || std::ranges::any_of(
                                                       selection_,
                                                       [this, handle](nori::resource::handle selected)
                                                       {
                                                           return selected != handle && is_descendant(handle, selected);
                                                       }
                                                   );
        }
    );

    for (nori::resource::handle handle : targets)
    {
        app_.set_modified(handle);
        std::ignore = nori::resource::set_parent(handle, parent);
    }
    if (!targets.empty())
    {
        open_nodes_.insert(parent);
        app_.set_modified(parent);
    }
}

void hierarchy::render()
{
    context& ctx{app_.get_context()};
    for (std::size_t index{0}; index < ctx.roots.size();)
    {
        const nori::resource::handle root{ctx.roots.at(index).handle};
        render_root(index);
        if (index < ctx.roots.size() && ctx.roots.at(index).handle == root)
        {
            ++index;
        }
    }
    render_image_file_dialog();
}

void hierarchy::render_root(std::size_t root_index)
{
    context& ctx{app_.get_context()};
    context::root& root{ctx.roots.at(root_index)};
    std::string title{std::format("{}##Hierarchy_{}", utils::to_utf8(root.path.filename()), root.handle.index)};
    if (root.is_modified)
    {
        title.insert(0, "* ");
    }

    if (!docked_roots_.contains(root.handle) && ctx.dock_space_id != 0)
    {
        ImGui::SetNextWindowDockID(ctx.dock_space_id, ImGuiCond_FirstUseEver);
        docked_roots_.insert(root.handle);
    }

    if (!ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("저장"))
        {
            select(root.handle);
            app_.save_selected_file();
        }
        if (ImGui::MenuItem("닫기"))
        {
            close_root(root.handle);
            ImGui::EndMenuBar();
            ImGui::End();
            return;
        }
        ImGui::EndMenuBar();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_TreeLinesSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TreeLinesRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_TreeLines, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    for (nori::resource::handle child : nori::resource::get_children(root.handle))
    {
        render_node(child, false);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const ImVec2 blank_size{ImGui::GetContentRegionAvail()};
    if (blank_size.x > 0.0f && blank_size.y > 0.0f)
    {
        const ImVec2 blank_min{ImGui::GetCursorScreenPos()};
        const ImVec2 blank_max{blank_min + blank_size};
        ImGui::InvisibleButton("##HierarchyBlankArea", blank_size);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            clear_selection();
        }
        const bool show_drop_preview{
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::GetDragDropPayload() &&
            ImGui::GetDragDropPayload()->IsDataType("NORITOR_HIERARCHY_NODE")
        };
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("NORITOR_DOCUMENT")})
            {
                app_.load_data_file(static_cast<const wchar_t*>(payload->Data));
            }
            constexpr ImGuiDragDropFlags accept_flags{ImGuiDragDropFlags_AcceptNoDrawDefaultRect};
            if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("NORITOR_HIERARCHY_NODE", accept_flags)})
            {
                const nori::resource::handle dragged{*static_cast<const nori::resource::handle*>(payload->Data)};
                if (is_selected(dragged))
                {
                    move_selection_to(root.handle);
                }
                else if (can_move_to(dragged, root.handle))
                {
                    app_.set_modified(dragged);
                    std::ignore = nori::resource::set_parent(dragged, root.handle);
                    open_nodes_.insert(root.handle);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (show_drop_preview)
        {
            draw_insertion_preview(blank_min, blank_max);
        }
        if (ImGui::BeginPopupContextItem("HierarchyBlankMenu"))
        {
            render_add_child_menu(root.handle);

            const bool can_delete{std::ranges::all_of(
                selection_,
                [this, &root](nori::resource::handle selected)
                {
                    const context::root* selected_root{app_.get_root(selected)};
                    return selected_root && selected_root->handle == root.handle && !is_root(selected);
                }
            )};
            if (ImGui::MenuItem("삭제", nullptr, false, !selection_.empty() && can_delete))
            {
                delete_selected();
            }
            ImGui::EndPopup();
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
    {
        clear_selection();
    }

    ImGui::End();
}

void hierarchy::render_node(nori::resource::handle handle, bool is_root_node)
{
    const std::vector<nori::resource::handle> children{nori::resource::get_children(handle)};
    std::string label{nori::resource::get_name(handle)};
    if (label.empty())
    {
        label = "(이름 없음)";
    }
    if (is_root_node)
    {
        if (const context::root* root{app_.get_root(handle)})
        {
            label = utils::to_utf8(root->path.filename());
            if (root->is_modified)
            {
                label.insert(0, "* ");
            }
        }
    }

    ImGui::PushID(static_cast<int>(handle.index));
    ImGuiTreeNodeFlags flags{
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanFullWidth |
        ImGuiTreeNodeFlags_DrawLinesToNodes
    };
    if (is_selected(handle))
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }
    ImGui::SetNextItemOpen(is_open(handle));
    if (is_root_node)
    {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.1f);
    }
    const float row_cursor_x{ImGui::GetCursorScreenPos().x};
    const bool is_opened{ImGui::TreeNodeEx("##Node", flags)};
    if (is_root_node)
    {
        ImGui::PopFont();
    }
    draw_node_content(handle, label, row_cursor_x, is_root_node);
    if (is_root_node)
    {
        if (const context::root* root{app_.get_root(handle)})
        {
            const std::string path{utils::to_utf8(root->path)};
            ImGui::SetItemTooltip("%s", path.c_str());
        }
    }
    if (ImGui::IsItemClicked())
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            toggle_selection(handle);
        }
        else
        {
            select(handle);
        }
    }
    if (ImGui::IsItemToggledOpen())
    {
        set_open(handle, is_opened);
    }

    if (ImGui::BeginDragDropSource())
    {
        if (!is_selected(handle))
        {
            select(handle);
        }
        ImGui::SetDragDropPayload("NORITOR_HIERARCHY_NODE", &handle, sizeof(handle));
        ImGui::Text("%zu개 항목", selection_.size());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("NORITOR_HIERARCHY_NODE")})
        {
            const nori::resource::handle dragged{*static_cast<const nori::resource::handle*>(payload->Data)};
            if (is_selected(dragged))
            {
                move_selection_to(handle);
            }
            else if (can_move_to(dragged, handle))
            {
                app_.set_modified(dragged);
                std::ignore = nori::resource::set_parent(dragged, handle);
                open_nodes_.insert(handle);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem("NodeMenu"))
    {
        if (!is_selected(handle))
        {
            select(handle);
        }
        render_add_child_menu(handle);
        ImGui::Separator();
        if (is_root_node)
        {
            if (ImGui::MenuItem("닫기"))
            {
                close_root(handle);
            }
            if (ImGui::MenuItem("저장"))
            {
                app_.save_selected_file();
            }
        }
        else if (ImGui::MenuItem("삭제"))
        {
            delete_selected();
        }
        ImGui::EndPopup();
    }

    if (is_opened)
    {
        for (nori::resource::handle child : children)
        {
            render_node(child, false);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void hierarchy::render_add_child_menu(nori::resource::handle parent)
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
        add_child(parent, type);
        switch (type)
        {
        case nori::resource::type::image:
        {
            file_dialog::open("이미지(.png)##Hierarchy", {".png"});
            file_dialog_context_ = {.parent = parent};
            break;
        }
        default:
            break;
        }
    }
    ImGui::EndMenu();
}

void hierarchy::render_image_file_dialog()
{
    const std::filesystem::path path{file_dialog::render("이미지(.png)##Hierarchy")};
    if (path.empty())
    {
        return;
    }
    const nori::resource::handle child{nori::resource::create(get_temp_prop_name())};
    if (!child)
    {
        return;
    }
    const nori::resource::handle parent{file_dialog_context_.parent};
    if (!parent)
    {
        nori::resource::remove(child);
        return;
    }
    if (!nori::resource::set_parent(child, parent))
    {
        nori::resource::remove(child);
        return;
    }
    if (!nori::resource::set_name(child, create_unique_child_name(parent)))
    {
        nori::resource::remove(child);
        return;
    }
    const std::optional<nori::resource::image_asset> image{utils::load_png_image(path)};
    if (!image)
    {
        nori::resource::remove(child);
        return;
    }
    nori::resource::set_value(child, *image);
    open_nodes_.insert(parent);
    app_.set_modified(parent);
    select(child);
    file_dialog_context_ = {};
}

void hierarchy::add_child(nori::resource::handle parent, nori::resource::type type)
{
    const std::string name{create_unique_child_name(parent)};
    if (type == nori::resource::type::image)
    {
        open_nodes_.insert(parent);
        return;
    }

    const nori::resource::handle child{nori::resource::create(get_temp_prop_name())};
    if (!child)
    {
        return;
    }
    if (!nori::resource::set_parent(child, parent))
    {
        nori::resource::remove(child);
        return;
    }
    std::ignore = nori::resource::set_name(child, name);

    switch (type)
    {
    case nori::resource::type::int32:
        nori::resource::set_value(child, std::int32_t{});
        break;
    case nori::resource::type::int64:
        nori::resource::set_value(child, std::int64_t{});
        break;
    case nori::resource::type::float32:
        nori::resource::set_value(child, 0.0f);
        break;
    case nori::resource::type::string:
        nori::resource::set_value(child, std::string{});
        break;
    default:
        break;
    }

    open_nodes_.insert(parent);
    select(child);
    app_.set_modified(parent);
}

std::string hierarchy::create_unique_child_name(nori::resource::handle parent) const
{
    std::unordered_set<std::string> sibling_names;
    for (nori::resource::handle child : nori::resource::get_children(parent))
    {
        sibling_names.insert(nori::resource::get_name(child));
    }

    constexpr std::string_view base_name{"Property"};
    if (!sibling_names.contains(std::string{base_name}))
    {
        return std::string{base_name};
    }

    std::size_t index{2};
    while (true)
    {
        std::string name{std::format("{}{}", base_name, index++)};
        if (!sibling_names.contains(name))
        {
            return name;
        }
    }
    return {};
}

std::string hierarchy::get_temp_prop_name() const
{
    std::size_t i{1};
    while (true)
    {
        std::string name{std::format("__internal{}", i++)};
        if (!nori::resource::get(name))
        {
            return name;
        }
    }
    return {};
}

bool hierarchy::is_selected(nori::resource::handle handle) const
{
    return std::ranges::contains(selection_, handle);
}

bool hierarchy::is_open(nori::resource::handle handle) const
{
    return open_nodes_.contains(handle);
}

bool hierarchy::is_root(nori::resource::handle handle) const
{
    const context& ctx{app_.get_context()};
    return std::ranges::contains(ctx.roots, handle, &context::root::handle);
}

bool hierarchy::is_descendant(nori::resource::handle handle, nori::resource::handle ancestor) const
{
    for (nori::resource::handle parent{nori::resource::get_parent(handle)}; parent; parent = nori::resource::get_parent(parent))
    {
        if (parent == ancestor)
        {
            return true;
        }
    }
    return false;
}

bool hierarchy::can_move_to(nori::resource::handle handle, nori::resource::handle parent) const
{
    if (is_root(handle) || handle == parent)
    {
        return false;
    }
    if (nori::resource::get_parent(handle) == parent)
    {
        return false;
    }
    return !is_descendant(parent, handle);
}

void hierarchy::set_open(nori::resource::handle handle, bool is_open)
{
    if (is_open)
    {
        open_nodes_.insert(handle);
    }
    else
    {
        open_nodes_.erase(handle);
    }
}
