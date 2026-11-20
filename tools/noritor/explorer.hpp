#pragma once
#include <filesystem>
#include <string_view>

#include <imgui_internal.h>

namespace nori::editor
{
class context;
class asset_manager;

class explorer
{
public:
    explorer(context* ctx, const asset_manager* assets);

    explorer(const explorer&) = delete;
    explorer(explorer&&) = delete;
    explorer& operator=(const explorer&) = delete;
    explorer& operator=(explorer&&) = delete;

    void render();

private:
    void render_file_tree();
    void render_address_bar();
    void render_directory_contents();

    bool file_button(std::filesystem::path path, std::string_view image_path, bool draggable);
    void set_path(std::filesystem::path path);

    context* context_;
    const asset_manager* assets_;
    std::filesystem::path path_;
    bool scroll_address_bar_to_end_;
    ImGuiID selected_item_id_;
    ImRect selected_item_rect_;
};
} // namespace nori::editor
