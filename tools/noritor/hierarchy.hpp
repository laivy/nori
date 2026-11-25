#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nori/resource/handle.hpp>
#include <nori/resource/value.hpp>

#include "command.hpp"

namespace nori::editor
{
class context;
class document_editor;
class import_workflow;
class file_dialog;
class asset_manager;

class hierarchy
{
public:
    hierarchy(context* ctx, file_dialog* dialog, const asset_manager* assets, import_workflow* importer);

    hierarchy(const hierarchy&) = delete;
    hierarchy(hierarchy&&) = delete;
    hierarchy& operator=(const hierarchy&) = delete;
    hierarchy& operator=(hierarchy&&) = delete;

    void select_all(document_editor& doc);

    void remove_selection(document_editor& target_document);
    void copy_selection(document_editor& target_document);
    void paste_selection(document_editor& target_document);
    void duplicate_selection(document_editor& target_document);

    void begin_rename(document_editor& doc);
    void focus_search(document_editor& doc);

    void draw(document_editor& doc);
    void draw_dialog();

private:
    void create(document_editor& target_document, nori::resource::handle parent, nori::resource::property_type type);
    void cut_selection(document_editor& target_document);
    void move_selection(document_editor& target_document, nori::resource::handle parent, std::optional<std::size_t> position = std::nullopt);

    void clear_selection(document_editor& doc);
    void select(document_editor& doc, nori::resource::handle handle);
    void toggle_selection(document_editor& doc, nori::resource::handle handle);
    bool is_selected(document_editor& doc, nori::resource::handle handle) const;

    void render_content(document_editor& doc);
    void render_filter_bar(document_editor& doc);
    void render_node(document_editor& doc, nori::resource::handle handle, float gap_height);
    void render_drop_gap(document_editor& doc, nori::resource::handle parent, nori::resource::handle anchor, float gap_height);
    void render_add_child_menu(document_editor& doc, nori::resource::handle parent);

    bool matches_self(document_editor& doc, nori::resource::handle handle) const;
    bool matches_filter(document_editor& doc, nori::resource::handle handle) const;

    bool is_open(nori::resource::handle handle) const;
    void set_open(nori::resource::handle handle, bool is_open);

    void handle_drop(nori::resource::handle dragged, nori::resource::handle parent, std::optional<std::size_t> position = std::nullopt);

    void request_new_image(document_editor& target_document, nori::resource::handle parent);
    void request_import_model(document_editor& target_document, nori::resource::handle parent);

    void report(nori::resource::error_code error);

    context* context_;
    file_dialog* file_dialog_;
    const asset_manager* assets_;
    import_workflow* importer_;

    nori::resource::handle focus_search_document_;

    nori::resource::handle remove_request_document_;
    std::optional<resource_request_target> image_request_;
    std::optional<resource_request_target> model_request_;
};
} // namespace nori::editor
