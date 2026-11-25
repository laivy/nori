#include "pch.hpp"

#include <nori/core/encoding.hpp>

#include "application_workflow.hpp"
#include "context.hpp"
#include "file_dialog.hpp"
#include "workflow_view.hpp"

namespace nori::editor
{
workflow_view::workflow_view(application_workflow* workflow) :
    workflow_{workflow}
{
}

void workflow_view::draw_close_confirmation(document_editor& doc)
{
    if (!workflow_->close_confirmation_document_ || workflow_->close_confirmation_document_ != doc.root())
    {
        return;
    }

    if (!doc.dirty())
    {
        workflow_->confirm_close(doc, false);
        return;
    }

    ImGui::OpenPopup("저장되지 않은 변경 사항##close_document");
    if (!ImGui::BeginPopupModal("저장되지 않은 변경 사항##close_document", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const std::string name{nori::core::path_to_utf8(doc.path().filename())};
    ImGui::Text("%s의 변경 사항을 저장할까요?", name.c_str());
    if (ImGui::Button("저장"))
    {
        if (workflow_->confirm_close(doc, true))
        {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("저장 안 함"))
    {
        workflow_->confirm_close(doc, false);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("취소"))
    {
        workflow_->cancel_close();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void workflow_view::draw_document_dialog()
{
    if (workflow_->exit_workflow_.closing())
    {
        return;
    }
    draw_save_as_overwrite_confirmation();
    if (!workflow_->save_as_overwrite_path_.empty())
    {
        return;
    }
    if (!workflow_->file_dialog_request_)
    {
        return;
    }
    const bool save_as{workflow_->file_dialog_request_ == application_workflow::file_dialog_request::save_as_document};
    const std::string_view label{save_as ? "다른 이름으로 저장(.nori)##noritor_application" : "열기(.nori)##noritor_application"};
    const file_dialog::result result{workflow_->file_dialog_->render(label, workflow_->context_->assets())};
    if (result.state == file_dialog::status::pending)
    {
        return;
    }
    workflow_->finish_file_dialog(result.state == file_dialog::status::selected ? &result.path : nullptr);
    draw_save_as_overwrite_confirmation();
}

void workflow_view::draw_save_as_overwrite_confirmation()
{
    if (!workflow_->validate_overwrite())
    {
        return;
    }

    ImGui::OpenPopup("파일 덮어쓰기##save_as");
    if (!ImGui::BeginPopupModal("파일 덮어쓰기##save_as", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    const std::string path{nori::core::path_to_utf8(workflow_->save_as_overwrite_path_)};
    ImGui::Text("'%s' 파일을 덮어쓸까요?", path.c_str());
    if (ImGui::Button("덮어쓰기"))
    {
        workflow_->confirm_overwrite(true);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("취소"))
    {
        workflow_->confirm_overwrite(false);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace nori::editor
