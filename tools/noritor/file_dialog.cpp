#include "pch.hpp"
#include "file_dialog.hpp"
#include "utils.hpp"

namespace
{
struct file_dialog_context
{
    std::vector<std::filesystem::path> extensions;
    std::filesystem::path path;
    std::filesystem::path selected_path;
    ImGuiID selected_item_id;
    ImRect selected_item_rect;
};

enum class action
{
    none,
    open,
    cancel
};

bool is_selectable_file(std::span<const std::filesystem::path> extensions, const std::filesystem::path& file_path)
{
    const std::u8string extension{utils::to_lower(file_path.extension().u8string())};
    return std::ranges::any_of(
        extensions,
        [&extension](const std::filesystem::path& candidate)
        {
            return utils::to_lower(candidate.u8string()) == extension;
        }
    );
}

void reset_selection(file_dialog_context& ctx)
{
    ctx.selected_path.clear();
    ctx.selected_item_id = 0;
    ctx.selected_item_rect = {};
}

void set_path(file_dialog_context& ctx, const std::filesystem::path& path)
{
    ctx.path = path;
    reset_selection(ctx);
}

void render_file_tree_node(file_dialog_context& ctx, const std::filesystem::path& path)
{
    const bool has_sub_folder{utils::has_sub_directory(path)};
    const std::filesystem::path label_path{path == path.root_path() ? path.root_name() : path.filename()};
    const std::string label{utils::to_utf8(label_path)};

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

void render_file_tree(file_dialog_context& ctx)
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

void render_address_bar(file_dialog_context& ctx)
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
    const std::string current_drive{utils::to_utf8(ctx.path.root_name())};
    if (ImGui::BeginCombo("##DiskDrive", current_drive.c_str()))
    {
        for (const auto& drive : utils::get_drives())
        {
            const std::string label{utils::to_utf8(drive.root_name())};
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
        if (ImGui::Button(utils::to_utf8(part).c_str()))
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
                    if (ImGui::Selectable(utils::to_utf8(sub_folder.filename()).c_str()))
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

bool file_button(file_dialog_context& ctx, const std::filesystem::path& path)
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

    const std::string label{utils::to_utf8(path.filename())};
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
    ImVec2 end_cursor_pos{};
    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2{item_size.x, item_spacing.y});

        const ImVec2 icon_cursor{ImGui::GetCursorScreenPos() + ImVec2{(item_width - icon_size.x) * 0.5f, 0.0f}};
        ImGui::Dummy(ImVec2{item_width, icon_size.y});
        if (is_directory)
        {
            utils::draw_folder_icon(ImGui::GetWindowDrawList(), icon_cursor, icon_size);
        }
        else
        {
            utils::draw_file_icon(ImGui::GetWindowDrawList(), icon_cursor, icon_size);
        }
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

bool render_files(file_dialog_context& ctx)
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
        if (file_button(ctx, path))
        {
            set_path(ctx, path);
        }
        ImGui::SameLine();
    }
    bool open_selected{false};
    for (const auto& entry : files)
    {
        const std::filesystem::path& path{entry.path()};
        if (file_button(ctx, path) && is_selectable_file(ctx.extensions, path))
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

action render_action_bar(file_dialog_context& ctx)
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

    std::string selected_file_name{utils::to_utf8(ctx.selected_path.filename())};
    ImGui::InputText("##SelectedFileName", &selected_file_name, ImGuiInputTextFlags_ReadOnly);

    ImGui::SameLine();
    ImGui::BeginDisabled(selected_file_name.empty());
    const bool open{ImGui::Button("열기", ImVec2{button_width, 0.0f})};
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

action render_explorer(file_dialog_context& ctx)
{
    render_file_tree(ctx);
    ImGui::BeginGroup();
    render_address_bar(ctx);
    action action{action::none};
    if (render_files(ctx))
    {
        action = action::open;
    }
    else
    {
        action = render_action_bar(ctx);
    }
    ImGui::EndGroup();
    return action;
}

std::unordered_map<std::string, file_dialog_context> contexts;
} // namespace

namespace file_dialog
{
std::filesystem::path render(std::string_view label)
{
    const std::string key{label};
    auto it{contexts.find(key)};
    if (it == contexts.end())
    {
        return {};
    }

    file_dialog_context& ctx{it->second};
    if (!ImGui::IsPopupOpen(key.c_str(), ImGuiPopupFlags_None))
    {
        ImGui::OpenPopup(key.c_str());
    }

    constexpr ImVec2 window_size_ratio{0.6f, 0.6f};
    const ImGuiViewport* viewport{ImGui::GetWindowViewport()};
    const ImVec2 window_size{viewport->WorkSize.x * window_size_ratio.x, viewport->WorkSize.y * window_size_ratio.y};
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(key.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        return {};
    }

    std::error_code ec;
    if (!std::filesystem::exists(ctx.path, ec))
    {
        set_path(ctx, std::filesystem::current_path());
    }

    const action action{render_explorer(ctx)};
    std::filesystem::path selected_path;
    switch (action)
    {
    case action::open:
    {
        selected_path = ctx.selected_path;
        contexts.erase(key);
        ImGui::CloseCurrentPopup();
        break;
    }
    case action::cancel:
    {
        contexts.erase(key);
        ImGui::CloseCurrentPopup();
        break;
    }
    default:
        break;
    }
    ImGui::EndPopup();
    return selected_path;
}

void open(std::string_view label, std::vector<std::filesystem::path> extensions)
{
    std::string key{label};
    auto it{contexts.find(key)};
    if (it != contexts.end())
    {
        return;
    }
    file_dialog_context ctx{.extensions = std::move(extensions), .path = std::filesystem::current_path()};
    contexts.emplace(std::move(key), std::move(ctx));
}
} // namespace file_dialog
