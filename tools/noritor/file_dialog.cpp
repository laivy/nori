#include "pch.hpp"

#include <algorithm>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <nori/core/encoding.hpp>

#include "asset_manager.hpp"
#include "file_dialog.hpp"
#include "utils.hpp"

namespace
{
using nori::editor::asset_manager;
using nori::editor::file_dialog;
namespace utils = nori::editor::utils;
enum class action
{
    none,
    open,
    cancel
};

bool is_selectable_file(std::span<const std::filesystem::path> extensions, std::filesystem::path file_path)
{
    const std::string extension{utils::to_lower(nori::core::path_to_utf8(file_path.extension()))};
    return std::ranges::any_of(extensions, [&extension](std::filesystem::path candidate)
    {
        return utils::to_lower(nori::core::path_to_utf8(candidate)) == extension;
    });
}

void reset_selection(file_dialog::context& ctx)
{
    ctx.selected_path.clear();
    ctx.selected_item_id = 0;
    ctx.selected_item_rect = {};
}

void set_path(file_dialog::context& ctx, std::filesystem::path path)
{
    ctx.path = path;
    reset_selection(ctx);
}

void render_file_tree_node(file_dialog::context& ctx, std::filesystem::path path)
{
    const bool has_sub_folder{utils::has_sub_directory(path)};
    const std::filesystem::path label_path{path == path.root_path() ? path.root_name() : path.filename()};
    const std::string label{nori::core::path_to_utf8(label_path)};

    ImGuiTreeNodeFlags flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth};
    if (!has_sub_folder)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (path == ctx.path)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool opened{ImGui::TreeNodeEx(label.c_str(), flags)};
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        set_path(ctx, path);
    }

    if (opened)
    {
        std::error_code ec;
        for (
            const auto& entry :
            std::filesystem::directory_iterator{path, std::filesystem::directory_options::skip_permission_denied, ec}
        )
        {
            if (entry.is_directory(ec))
            {
                render_file_tree_node(ctx, entry.path());
            }
        }
        ImGui::TreePop();
    }
}

void render_file_tree(file_dialog::context& ctx)
{
    if (!ImGui::BeginChild("Tree", ImVec2{175.0f, 0.0f}, ImGuiChildFlags_ResizeX, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.5f);
    int index{};
    for (const auto& drive : utils::get_drives())
    {
        ImGui::PushID(index++);
        render_file_tree_node(ctx, drive);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::SameLine();
}

void render_address_bar(file_dialog::context& ctx)
{
    if (!ImGui::BeginChild(
            "AddressBar",
            ImVec2{0.0f, ImGui::GetFrameHeight()},
            ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar
        ))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() + ImGui::CalcTextSize("AAA").x);
    const std::string current_drive{nori::core::path_to_utf8(ctx.path.root_name())};
    if (ImGui::BeginCombo("##disk_drive", current_drive.c_str()))
    {
        for (const auto& drive : utils::get_drives())
        {
            const std::string label{nori::core::path_to_utf8(drive.root_name())};
            if (ImGui::Selectable(label.c_str(), drive.root_name() == ctx.path.root_name()))
            {
                set_path(ctx, drive);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{3.0f, ImGui::GetStyle().ItemSpacing.y});
    std::filesystem::path selected_path;
    std::filesystem::path current_path;
    int part_index{};
    for (const auto& part : ctx.path)
    {
        current_path /= part;
        if (part.has_root_name() || part.has_root_directory())
        {
            continue;
        }

        ImGui::PushID(part_index++);
        ImGui::SameLine();
        if (ImGui::Button(nori::core::path_to_utf8(part).c_str()))
        {
            selected_path = current_path;
        }

        std::vector<std::filesystem::path> sub_folders;
        std::error_code ec;
        for (
            const auto& entry : std::filesystem::directory_iterator{
                current_path, std::filesystem::directory_options::skip_permission_denied, ec
            }
        )
        {
            if (entry.is_directory(ec))
            {
                sub_folders.push_back(entry.path());
            }
        }

        if (!sub_folders.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(">"))
            {
                ImGui::OpenPopup("FolderList");
            }
            if (ImGui::BeginPopup("FolderList"))
            {
                for (const auto& sub_folder : sub_folders)
                {
                    if (ImGui::Selectable(nori::core::path_to_utf8(sub_folder.filename()).c_str()))
                    {
                        selected_path = sub_folder;
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    if (!selected_path.empty())
    {
        set_path(ctx, selected_path);
    }

    ImGui::EndChild();
}

bool file_button(file_dialog::context& ctx, std::filesystem::path path, const asset_manager& assets)
{
    std::error_code ec;
    const bool is_directory{std::filesystem::is_directory(path, ec)};
    const float font_size{ImGui::GetFontSize()};
    const float item_width{font_size * 5.0f};
    const ImVec2 icon_size{font_size * 4.0f, font_size * 4.0f};
    constexpr float text_spacing_y{-3.0f};

    const ImVec2 item_spacing{ImGui::GetStyle().ItemSpacing};
    const ImVec2 item_size{
        item_width,
        item_spacing.y +
            icon_size.y +
            item_spacing.y +
            font_size * utils::file_name_line_max +
            text_spacing_y * (utils::file_name_line_max - 1) +
            item_spacing.y
    };

    if (ImGui::GetCursorPosX() > 0.0f && ImGui::GetContentRegionAvail().x < item_width + item_spacing.x)
    {
        ImGui::Spacing();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
    const float cursor_x{ImGui::GetCursorPosX()};
    if (cursor_x == 0.0f)
    {
        ImGui::Dummy(ImVec2{item_spacing.x, item_size.y});
        ImGui::SameLine();
    }

    const std::string label{nori::core::path_to_utf8(path.filename())};
    const ImGuiID item_id{ImGui::GetID(label.c_str())};
    const bool selected{ctx.selected_item_id == item_id};
    if (selected)
    {
        const ImVec2 cursor{ImGui::GetCursorScreenPos()};
        const ImVec2 size{ctx.selected_item_rect.GetSize()};
        ImDrawList* draw_list{ImGui::GetWindowDrawList()};
        draw_list->AddRectFilled(cursor, cursor + size, IM_COL32(80, 80, 80, 128));
        draw_list->AddRect(cursor, cursor + size, ImGui::GetColorU32(ImGuiCol_Text));
    }

    const ImVec2 start_cursor_pos{ImGui::GetCursorPos()};
    ImVec2 end_cursor_pos;
    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2{item_size.x, item_spacing.y});

        const ImVec2 icon_cursor{ImGui::GetCursorScreenPos() + ImVec2{(item_width - icon_size.x) * 0.5f, 0.0f}};
        ImGui::Dummy(ImVec2{item_width, icon_size.y});
        ImGui::SetCursorScreenPos(icon_cursor);
        assets.image(is_directory ? "64/folder" : "64/file");
        ImGui::SetCursorScreenPos(icon_cursor + ImVec2{0.0f, icon_size.y});
        ImGui::Spacing();

        const float text_wrap_width{item_width * 0.9f};
        for (const auto& line : utils::split_file_name(label, text_wrap_width))
        {
            const ImVec2 text_size{ImGui::CalcTextSize(line.data(), line.data() + line.size())};
            ImGui::Dummy(ImVec2{(item_width - text_size.x) * 0.5f, text_size.y});
            ImGui::SameLine();
            ImGui::TextUnformatted(line.data(), line.data() + line.size());
        }

        ImGui::Dummy(ImVec2{item_width, item_spacing.y});
        end_cursor_pos.x = start_cursor_pos.x + item_width;
        end_cursor_pos.y = ImGui::GetCursorPosY();
    }
    ImGui::EndGroup();

    if (selected)
    {
        ctx.selected_item_rect = ImRect{start_cursor_pos, end_cursor_pos};
    }

    bool is_clicked{false};
    ImGui::SetCursorPos(start_cursor_pos);
    ImGui::InvisibleButton(label.c_str(), end_cursor_pos - start_cursor_pos);
    const bool is_double_clicked{ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)};
    if (ImGui::IsItemClicked())
    {
        const ImGuiID id{ImGui::GetItemID()};
        if (ctx.selected_item_id == id || is_double_clicked)
        {
            is_clicked = true;
        }
        else
        {
            if (is_selectable_file(ctx.extensions, path))
            {
                ctx.selected_path = path;
            }
            else
            {
                ctx.selected_path.clear();
            }
            ctx.selected_item_id = id;
            ctx.selected_item_rect = ImRect{start_cursor_pos, end_cursor_pos};
        }
    }

    ImGui::SetCursorPos(start_cursor_pos);
    ImGui::Dummy(item_size);
    ImGui::PopStyleVar();
    return is_clicked;
}

bool render_files(file_dialog::context& ctx, const asset_manager& assets)
{
    if (!ImGui::BeginChild(
            "FileViewer",
            ImVec2{0.0f, -(ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y)},
            ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar
        ))
    {
        ImGui::EndChild();
        return false;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGuiWindow* window{ImGui::GetCurrentWindow()};
        const ImVec2 screen_mouse_pos{ImGui::GetMousePos()};
        const ImVec2 local_mouse_pos{screen_mouse_pos - window->Pos - window->Scroll};
        if (!ctx.selected_item_rect.Contains(local_mouse_pos))
        {
            reset_selection(ctx);
        }
    }

    const float item_spacing_y{ImGui::GetStyle().ItemSpacing.y};
    ImGui::Dummy(ImVec2{0.0f, item_spacing_y});

    std::error_code ec;
    std::vector<std::filesystem::directory_entry> directories;
    std::vector<std::filesystem::directory_entry> files;
    for (
        const auto& entry :
        std::filesystem::directory_iterator{ctx.path, std::filesystem::directory_options::skip_permission_denied, ec}
    )
    {
        if (entry.is_directory(ec))
        {
            directories.push_back(entry);
        }
        else if (entry.is_regular_file(ec))
        {
            files.push_back(entry);
        }
    }

    std::ranges::sort(directories);
    std::ranges::sort(files);
    for (const auto& entry : directories)
    {
        const std::filesystem::path& path{entry.path()};
        if (file_button(ctx, path, assets))
        {
            set_path(ctx, path);
        }
        ImGui::SameLine();
    }
    bool open_selected{false};
    for (const auto& entry : files)
    {
        const std::filesystem::path& path{entry.path()};
        if (file_button(ctx, path, assets) && is_selectable_file(ctx.extensions, path))
        {
            ctx.selected_path = path;
            open_selected = true;
        }
        ImGui::SameLine();
    }

    ImGui::Spacing();
    ImGui::Dummy(ImVec2{0.0f, item_spacing_y});
    ImGui::EndChild();
    return open_selected;
}

action render_action_bar(file_dialog::context& ctx)
{
    ImGui::Spacing();

    constexpr float button_width{80.0f};
    const float input_text_width = []
    {
        float width{ImGui::GetContentRegionAvail().x};
        width -= (button_width + ImGui::GetStyle().ItemSpacing.x) * 2.0f;
        return width;
    }();
    ImGui::SetNextItemWidth(input_text_width);

    if (!ctx.save)
    {
        ctx.file_name = nori::core::path_to_utf8(ctx.selected_path.filename());
    }
    const ImGuiInputTextFlags input_flags{ctx.save ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly};
    if (ImGui::InputText("##selected_file_name", &ctx.file_name, input_flags) && ctx.save)
    {
        ctx.selected_path = ctx.path / nori::core::utf8_to_path(ctx.file_name);
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(ctx.file_name.empty());
    const bool open{ImGui::Button(ctx.save ? "저장" : "열기", ImVec2{button_width, 0.0f})};
    ImGui::EndDisabled();

    ImGui::SameLine();
    const bool cancel{ImGui::Button("취소", ImVec2{button_width, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)};
    if (open)
    {
        return action::open;
    }
    if (cancel)
    {
        return action::cancel;
    }
    return action::none;
}

action render_explorer(file_dialog::context& ctx, const asset_manager& assets)
{
    render_file_tree(ctx);
    ImGui::BeginGroup();
    render_address_bar(ctx);
    action current_action{action::none};
    if (render_files(ctx, assets))
    {
        current_action = action::open;
    }
    else
    {
        current_action = render_action_bar(ctx);
    }
    ImGui::EndGroup();
    return current_action;
}
} // namespace

namespace nori::editor
{
file_dialog::result file_dialog::render(std::string_view label, const asset_manager& assets)
{
    const std::string key{label};
    auto it{contexts_.find(key)};
    if (it == contexts_.end())
    {
        return {.state = status::cancelled};
    }

    context& ctx{it->second};
    if (!ImGui::IsPopupOpen(key.c_str(), ImGuiPopupFlags_None))
    {
        ImGui::OpenPopup(key.c_str());
    }

    constexpr ImVec2 window_size_ratio{0.6f, 0.6f};
    const ImGuiViewport* viewport{ImGui::GetWindowViewport()};
    const ImVec2 window_size{viewport->WorkSize.x * window_size_ratio.x, viewport->WorkSize.y * window_size_ratio.y};
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(key.c_str(), nullptr))
    {
        return {};
    }

    std::error_code ec;
    const bool exists{std::filesystem::exists(ctx.path, ec)};
    if (ec)
    {
        contexts_.erase(key);
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return {.state = status::failed};
    }
    if (!exists)
    {
        const std::filesystem::path current{std::filesystem::current_path(ec)};
        if (ec)
        {
            contexts_.erase(key);
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return {.state = status::failed};
        }
        set_path(ctx, current);
    }

    const action dialog_action{render_explorer(ctx, assets)};
    std::filesystem::path selected_path;
    switch (dialog_action)
    {
    case action::open:
    {
        selected_path = ctx.save ? ctx.path / nori::core::utf8_to_path(ctx.file_name) : ctx.selected_path;
        if (ctx.save && selected_path.extension().empty() && !ctx.extensions.empty())
        {
            selected_path.replace_extension(ctx.extensions.front());
        }
        contexts_.erase(key);
        ImGui::CloseCurrentPopup();
        break;
    }
    case action::cancel:
    {
        contexts_.erase(key);
        ImGui::CloseCurrentPopup();
        break;
    }
    default:
        break;
    }
    ImGui::EndPopup();
    if (dialog_action == action::open)
    {
        return {.state = status::selected, .path = std::move(selected_path)};
    }
    if (dialog_action == action::cancel)
    {
        return {.state = status::cancelled};
    }
    return {};
}

void file_dialog::open(std::string_view label, std::vector<std::filesystem::path> extensions)
{
    std::string key{label};
    if (contexts_.contains(key))
    {
        return;
    }
    std::error_code ec;
    const std::filesystem::path current{std::filesystem::current_path(ec)};
    if (ec)
    {
        return;
    }
    context ctx{
        .extensions = std::move(extensions),
        .path = current
    };
    contexts_.emplace(std::move(key), std::move(ctx));
}

void file_dialog::save(std::string_view label, std::vector<std::filesystem::path> extensions, const std::filesystem::path& initial_path)
{
    std::string key{label};
    if (contexts_.contains(key))
    {
        return;
    }
    std::error_code ec;
    const std::filesystem::path current{std::filesystem::current_path(ec)};
    if (ec)
    {
        return;
    }
    context ctx{
        .extensions = std::move(extensions),
        .path = initial_path.has_parent_path() ? initial_path.parent_path() : current,
        .file_name = nori::core::path_to_utf8(initial_path.filename()),
        .save = true
    };
    contexts_.emplace(std::move(key), std::move(ctx));
}
} // namespace nori::editor
