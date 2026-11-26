#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nori/resource/handle.hpp>

#include "exit_workflow.hpp"

namespace nori::editor
{
class context;
class document_editor;
class file_dialog;
class preview;
enum class save_as_policy;

class application_workflow
{
    friend class workflow_view;

public:
    application_workflow(context* ctx, file_dialog* dialog, const preview* preview_renderer);

    bool requested() const;
    bool closing() const;

    void process_pending_closes();
    void close_exit_documents();
    void advance_exit();
    void process_close_request();

    void create_document();
    void open_document(std::filesystem::path path);
    void save_active_document();
    void open_save_as_dialog();
    void save_all_documents();

    void request_close_document(nori::resource::handle handle);
    void request_exit();
    void open_document_dialog();

private:
    enum class file_dialog_request
    {
        open_document,
        save_as_document
    };

    bool save_document(nori::resource::handle handle);
    void close_document(nori::resource::handle handle);
    document_editor* find_document(nori::resource::handle handle);
    const document_editor* find_document(nori::resource::handle handle) const;
    void approve_document_exit(nori::resource::handle handle);
    bool save_document_as(nori::resource::handle handle, const std::filesystem::path& path, save_as_policy policy);
    bool confirm_close(document_editor& doc, bool save);
    void cancel_close();
    void finish_file_dialog(const std::filesystem::path* path);
    bool validate_overwrite();
    void confirm_overwrite(bool overwrite);
    void set_status(std::string message);

    context* context_;
    file_dialog* file_dialog_;
    const preview* preview_;

    nori::resource::handle close_confirmation_document_{};
    nori::resource::handle close_document_request_{};
    std::vector<nori::resource::handle> pending_close_;

    std::optional<file_dialog_request> file_dialog_request_;
    nori::resource::handle file_dialog_document_{};
    std::filesystem::path save_as_overwrite_path_;
    nori::resource::handle save_as_overwrite_document_{};

    exit_workflow exit_workflow_;
};
} // namespace nori::editor
