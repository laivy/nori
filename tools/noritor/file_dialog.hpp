#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

namespace nori::editor
{
class asset_manager;

class file_dialog
{
public:
    enum class status
    {
        pending,
        cancelled,
        selected,
        failed
    };

    struct result
    {
        status state;
        std::filesystem::path path;
    };

    struct context
    {
        std::vector<std::filesystem::path> extensions;
        std::filesystem::path path;
        std::filesystem::path selected_path;
        ImGuiID selected_item_id;
        ImRect selected_item_rect;
        std::string file_name;
        bool save;
    };

    file_dialog() = default;

    file_dialog(const file_dialog&) = delete;
    file_dialog(file_dialog&&) = delete;
    file_dialog& operator=(const file_dialog&) = delete;
    file_dialog& operator=(file_dialog&&) = delete;

    result render(std::string_view label, const asset_manager& assets);

    void open(std::string_view label, std::vector<std::filesystem::path> extensions);
    void save(std::string_view label, std::vector<std::filesystem::path> extensions, const std::filesystem::path& initial_path);

private:
    std::unordered_map<std::string, context> contexts_;
};
} // namespace nori::editor
