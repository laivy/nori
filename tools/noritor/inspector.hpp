#pragma once
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include <nori/resource/handle.hpp>

#include "command.hpp"

namespace nori::editor
{
class context;
class document_editor;
class import_workflow;
class file_dialog;
class asset_manager;

class inspector
{
public:
    inspector(context* ctx, file_dialog* dialog, const asset_manager* assets, import_workflow* importer);

    inspector(const inspector&) = delete;
    inspector(inspector&&) = delete;
    inspector& operator=(const inspector&) = delete;
    inspector& operator=(inspector&&) = delete;

    void draw(document_editor& doc);
    void draw_dialog();

private:
    void focus_name(document_editor& doc);

    void rename(document_editor& target_document, nori::resource::handle target, std::string name);
    void set_type(document_editor& target_document, std::span<const nori::resource::handle> targets, nori::resource::property_type type);
    void set_value(document_editor& target_document, std::span<const nori::resource::handle> targets, property_snapshot value);
    void request_image_replacement(document_editor& target_document, nori::resource::handle target);
    void report(nori::resource::error_code error);

    context* context_;
    file_dialog* file_dialog_;
    const asset_manager* assets_;
    import_workflow* importer_;

    std::optional<resource_request_target> image_request_;
};
} // namespace nori::editor
