#pragma once
#include <filesystem>

class noritor_application;

class explorer
{
public:
    explicit explorer(noritor_application& app);

    explorer(const explorer&) = delete;
    explorer(explorer&&) = delete;
    explorer& operator=(const explorer&) = delete;
    explorer& operator=(explorer&&) = delete;

    void render();

private:
    void render_file_tree();
    void render_file_tree_node(const std::filesystem::path& path);
    void render_address_bar();
    void render_files();
    bool file_button(bool directory, const std::filesystem::path& path);
    void set_path(const std::filesystem::path& path);

private:
    noritor_application& app_;
    std::filesystem::path path_;
    bool scroll_address_bar_to_end_;
    ImGuiID selected_item_id_;
    ImRect selected_item_rect_;
};
