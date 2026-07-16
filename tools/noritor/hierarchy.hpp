#pragma once
#include <cstddef>
#include <unordered_set>
#include <vector>
#include "context.hpp"

class noritor_application;

class hierarchy
{
private:
    struct file_dialog_context
    {
        nori::resource::handle parent;
    };

public:
    explicit hierarchy(noritor_application& app);

    hierarchy(const hierarchy&) = delete;
    hierarchy(hierarchy&&) = delete;
    hierarchy& operator=(const hierarchy&) = delete;
    hierarchy& operator=(hierarchy&&) = delete;

    void render();
    void select(nori::resource::handle handle);
    void open(nori::resource::handle handle);

    void clear_selection();
    void delete_selected();

private:
    void render_root(std::size_t root_index);
    void render_node(nori::resource::handle handle, bool is_root_node);
    void render_add_child_menu(nori::resource::handle parent);
    void render_image_file_dialog();

    void add_child(nori::resource::handle parent, nori::resource::type type);
    void toggle_selection(nori::resource::handle handle);
    void close_root(nori::resource::handle handle);
    void move_selection_to(nori::resource::handle parent);
    std::string create_unique_child_name(nori::resource::handle parent) const;
    std::string get_temp_prop_name() const;
    bool is_selected(nori::resource::handle handle) const;
    bool is_open(nori::resource::handle handle) const;
    bool is_root(nori::resource::handle handle) const;
    bool is_descendant(nori::resource::handle handle, nori::resource::handle ancestor) const;
    bool can_move_to(nori::resource::handle handle, nori::resource::handle parent) const;
    void set_open(nori::resource::handle handle, bool is_open);

private:
    noritor_application& app_;
    std::unordered_set<nori::resource::handle> docked_roots_;
    std::unordered_set<nori::resource::handle> open_nodes_;
    std::vector<nori::resource::handle> selection_;
    file_dialog_context file_dialog_context_;
};
