#pragma once

namespace nori::editor
{
class application_workflow;
class document_editor;

class workflow_view
{
public:
    explicit workflow_view(application_workflow* workflow);
    void draw_close_confirmation(document_editor& doc);
    void draw_document_dialog();

private:
    void draw_save_as_overwrite_confirmation();
    application_workflow* workflow_;
};
} // namespace nori::editor
