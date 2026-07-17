#include "pch.hpp"
#include "explorer.hpp"
#include "noritor_application.hpp"
#include "utils.hpp"

explorer::explorer(noritor_application& app) :
    app_{app},
    path_{std::filesystem::current_path()},
    scroll_address_bar_to_end_{false},
    selected_item_id_{},
    selected_item_rect_{}
{
}

void explorer::render()
{
    if (!ImGui::Begin("탐색기"))
    {
        ImGui::End();
        return;
    }

    render_file_tree();
    ImGui::BeginGroup();
    render_address_bar();
    render_files();
    ImGui::EndGroup();
    ImGui::End();
}

void explorer::render_file_tree()
{
    if (!ImGui::BeginChild(
            "FileTree", ImVec2{175.0f, 0.0f}, ImGuiChildFlags_ResizeX, ImGuiWindowFlags_HorizontalScrollbar
        ))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.5f);
    for (auto [i, drive] : utils::get_drives() | std::views::enumerate)
    {
        ImGui::PushID(static_cast<int>(i));
        render_file_tree_node(drive);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::SameLine();
}

void explorer::render_file_tree_node(const std::filesystem::path& path)
{
    const std::string label = [&path]()
    {
        if (path == path.root_path())
        {
            return utils::to_utf8(path.root_name());
        }
        const std::string filename{utils::to_utf8(path.filename())};
        return utils::to_utf8(path);
    }();
    const ImGuiTreeNodeFlags flags = [this, &path]()
    {
        ImGuiTreeNodeFlags flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth};
        if (path_ == path)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!utils::has_sub_directory(path))
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        return flags;
    }();
    const bool is_opened{ImGui::TreeNodeEx(label.c_str(), flags)};
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        set_path(path);
    }
    if (is_opened)
    {
        std::error_code ec;
        for (
            const auto& entry :
            std::filesystem::directory_iterator{path, std::filesystem::directory_options::skip_permission_denied, ec}
        )
        {
            if (entry.is_directory(ec))
            {
                render_file_tree_node(entry.path());
            }
        }
        ImGui::TreePop();
    }
}

void explorer::render_address_bar()
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
    const std::string current_drive{utils::to_utf8(path_.root_name())};
    if (ImGui::BeginCombo("##DiskDrive", current_drive.c_str()))
    {
        for (const auto& drive : utils::get_drives())
        {
            const std::string label{utils::to_utf8(drive.root_name())};
            if (ImGui::Selectable(label.c_str(), drive.root_name() == path_.root_name()))
            {
                set_path(drive);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{3.0f, ImGui::GetStyle().ItemSpacing.y});
    std::filesystem::path current_path;
    std::filesystem::path selected_path;
    for (auto [i, part] : path_ | std::views::enumerate)
    {
        current_path /= part;
        if (part.has_root_name() || part.has_root_directory())
        {
            continue;
        }

        ImGui::PushID(static_cast<int>(i));
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
        set_path(selected_path);
    }
    else if (scroll_address_bar_to_end_)
    {
        scroll_address_bar_to_end_ = false;
        ImGui::SetScrollHereX(1.0f);
    }
    ImGui::EndChild();
}

void explorer::render_files()
{
    if (!ImGui::BeginChild("FileViewer", ImVec2{}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGuiWindow* window{ImGui::GetCurrentWindow()};
        const ImVec2 screen_mouse_pos{ImGui::GetMousePos()};
        const ImVec2 local_mouse_pos{screen_mouse_pos - window->Pos - window->Scroll};
        if (!selected_item_rect_.Contains(local_mouse_pos))
        {
            selected_item_id_ = {};
            selected_item_rect_ = {};
        }
    }

    const float item_spacing_y{ImGui::GetStyle().ItemSpacing.y};
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ImGui::GetStyle().ItemSpacing.x, 0.0f});
    ImGui::Dummy(ImVec2{0.0f, item_spacing_y});
    ImGui::PopStyleVar();

    std::error_code ec;
    std::vector<std::filesystem::directory_entry> directories;
    std::vector<std::filesystem::directory_entry> files;
    for (
        const auto& entry :
        std::filesystem::directory_iterator{path_, std::filesystem::directory_options::skip_permission_denied, ec}
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
    std::ranges::sort(
        directories,
        {},
        [](const auto& entry)
        {
            return entry.path().filename();
        }
    );
    std::ranges::sort(
        files,
        {},
        [](const auto& entry)
        {
            return entry.path().filename();
        }
    );

    for (const auto& entry : directories)
    {
        if (file_button(true, entry.path()))
        {
            set_path(entry.path());
        }
        ImGui::SameLine();
    }

    for (const auto& entry : files)
    {
        const std::filesystem::path path{entry.path()};
        if (file_button(false, path) && utils::is_nori_file(path))
        {
            app_.load_data_file(path);
        }
        ImGui::SameLine();
    }

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ImGui::GetStyle().ItemSpacing.x, 0.0f});
    ImGui::Dummy(ImVec2{item_spacing_y, 0.0f});
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

bool explorer::file_button(bool directory, const std::filesystem::path& path)
{
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
    const bool selected{selected_item_id_ == item_id};
    if (selected)
    {
        const ImVec2 cursor{ImGui::GetCursorScreenPos()};
        const ImVec2 size{selected_item_rect_.GetSize()};
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
        if (directory)
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
        selected_item_rect_ = ImRect{start_cursor_pos, end_cursor_pos};
    }

    bool is_clicked{false};
    ImGui::SetCursorPos(start_cursor_pos);
    ImGui::InvisibleButton(label.c_str(), end_cursor_pos - start_cursor_pos);
    const bool is_double_clicked{ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)};
    if (ImGui::IsItemClicked())
    {
        const ImGuiID id{ImGui::GetItemID()};
        if (is_double_clicked || selected_item_id_ == id)
        {
            is_clicked = true;
            selected_item_id_ = {};
            selected_item_rect_ = {};
        }
        else
        {
            selected_item_id_ = id;
            selected_item_rect_ = ImRect{start_cursor_pos, end_cursor_pos};
        }
    }

    if (!directory && utils::is_nori_file(path) && ImGui::BeginDragDropSource())
    {
        const std::wstring payload{path.wstring()};
        ImGui::SetDragDropPayload("NORITOR_DOCUMENT", payload.c_str(), (payload.size() + 1) * sizeof(wchar_t));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::SetCursorPos(start_cursor_pos);
    ImGui::Dummy(item_size);
    ImGui::PopStyleVar();
    return is_clicked;
}

void explorer::set_path(const std::filesystem::path& path)
{
    path_ = path;
    scroll_address_bar_to_end_ = true;
    selected_item_id_ = {};
    selected_item_rect_ = {};
}
