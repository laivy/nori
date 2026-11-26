#include "pch.hpp"

#include <nori/core/encoding.hpp>
#include <nori/resource/path.hpp>

#include "application_workflow.hpp"
#include "context.hpp"
#include "file_dialog.hpp"
#include "log.hpp"
#include "preview.hpp"
#include "utils.hpp"

namespace nori::editor
{
application_workflow::application_workflow(context* ctx, file_dialog* dialog, const preview* preview_renderer) :
    context_{ctx},
    file_dialog_{dialog},
    preview_{preview_renderer}
{
}

bool application_workflow::requested() const
{
    return exit_workflow_.requested();
}

bool application_workflow::closing() const
{
    return exit_workflow_.closing();
}

void application_workflow::process_pending_closes()
{
    for (auto it{pending_close_.begin()}; it != pending_close_.end();)
    {
        document_editor* const target{context_->documents().find_root(*it)};
        if (!target)
        {
            it = pending_close_.erase(it);
            continue;
        }
        preview_->release(context_->workspace(target->root()));
        context_->remove_workspace(target->root());
        context_->documents().close(*target);
        it = pending_close_.erase(it);
    }
}

void application_workflow::close_exit_documents()
{
    if (!exit_workflow_.closing())
    {
        return;
    }

    for (document_editor& target : context_->documents().documents())
    {
        preview_->release(context_->workspace(target.root()));
        context_->remove_workspace(target.root());
    }
    context_->documents().close_all();
}

void application_workflow::advance_exit()
{
    if (!exit_workflow_.requested() || exit_workflow_.closing() || close_confirmation_document_ || close_document_request_ || !pending_close_.empty())
    {
        return;
    }
    if (const auto next{exit_workflow_.next_confirmation(context_->documents().documents())})
    {
        close_confirmation_document_ = next;
        return;
    }

    exit_workflow_.begin_closing();
    for (document_editor& target : context_->documents().documents())
    {
        context_->documents().begin_close(target);
    }
}

void application_workflow::process_close_request()
{
    if (close_document_request_)
    {
        close_document(close_document_request_);
        close_document_request_ = {};
    }
}

void application_workflow::create_document()
{
    const auto created{context_->documents().create()};
    if (!created)
    {
        set_status(std::format("새 문서를 만들지 못했습니다: {}", nori::resource::to_string(created.error())));
        return;
    }
}

void application_workflow::open_document(std::filesystem::path path)
{
    if (exit_workflow_.closing())
    {
        return;
    }
    if (!utils::is_nori_file(path))
    {
        log::error("지원하지 않는 파일 형식: {}", nori::core::path_to_utf8(path));
        return;
    }
    const auto opened{context_->documents().open(path)};
    if (!opened)
    {
        set_status(std::format("문서를 불러오지 못했습니다: {}", nori::resource::to_string(opened.error())));
        return;
    }
}

void application_workflow::save_active_document()
{
    document_editor* target{context_->documents().active()};
    if (!target)
    {
        return;
    }
    save_document(target->root());
}

void application_workflow::open_save_as_dialog()
{
    document_editor* const target{context_->documents().active()};
    if (!target || target->closing())
    {
        return;
    }
    file_dialog_request_ = file_dialog_request::save_as_document;
    file_dialog_document_ = target->root();
    file_dialog_->save(
        "다른 이름으로 저장(.nori)##noritor_application",
        {std::filesystem::path{nori::resource::path::extension}},
        target->path()
    );
}

void application_workflow::save_all_documents()
{
    const std::size_t count{context_->documents().documents().size()};
    if (const auto res{context_->documents().save_all()})
    {
        set_status(std::format("{}개 문서를 저장했습니다.", count));
    }
    else
    {
        set_status(std::format("문서 저장 실패: {}", nori::resource::to_string(res.error())));
    }
}

void application_workflow::request_close_document(nori::resource::handle handle)
{
    if (find_document(handle))
    {
        close_confirmation_document_ = handle;
    }
}

void application_workflow::request_exit()
{
    if (exit_workflow_.requested())
    {
        return;
    }
    exit_workflow_.request();
    advance_exit();
}

void application_workflow::open_document_dialog()
{
    file_dialog_request_ = file_dialog_request::open_document;
    file_dialog_->open("열기(.nori)##noritor_application", {std::filesystem::path{nori::resource::path::extension}});
}

bool application_workflow::save_document(nori::resource::handle handle)
{
    document_editor* const target{find_document(handle)};
    if (!target)
    {
        set_status("저장할 문서가 없습니다.");
        return false;
    }

    if (const auto res{context_->documents().save(*target)}; !res)
    {
        set_status(std::format("저장 실패: {}", nori::core::path_to_utf8(target->path())));
        return false;
    }
    set_status(std::format("저장했습니다: {}", nori::core::path_to_utf8(target->path().filename())));
    return true;
}

void application_workflow::close_document(nori::resource::handle handle)
{
    document_editor* const target{find_document(handle)};
    if (!target)
    {
        return;
    }

    if (!target->closing())
    {
        context_->documents().begin_close(*target);
    }
    if (!std::ranges::contains(pending_close_, target->root()))
    {
        pending_close_.push_back(target->root());
    }
}

document_editor* application_workflow::find_document(nori::resource::handle handle)
{
    return context_->documents().find(handle);
}

const document_editor* application_workflow::find_document(nori::resource::handle handle) const
{
    return context_->documents().find(handle);
}

void application_workflow::approve_document_exit(nori::resource::handle handle)
{
    exit_workflow_.approve(handle);
}

bool application_workflow::save_document_as(nori::resource::handle handle, const std::filesystem::path& path, save_as_policy policy)
{
    document_editor* const target{context_->documents().find_root(handle)};
    if (!target || target->closing())
    {
        return false;
    }
    const auto result{context_->documents().save_as(*target, path, policy)};
    if (!result)
    {
        set_status(std::format("다른 이름으로 저장 실패: {}", nori::resource::to_string(result.error())));
        return false;
    }
    if (*result == save_as_status::overwrite_required)
    {
        save_as_overwrite_path_ = path;
        save_as_overwrite_document_ = target->root();
        return false;
    }
    set_status(std::format("저장했습니다: {}", nori::core::path_to_utf8(target->path().filename())));
    return true;
}

bool application_workflow::confirm_close(document_editor& doc, bool save)
{
    if (save && !save_document(doc.root()))
    {
        return false;
    }
    if (exit_workflow_.requested())
    {
        approve_document_exit(doc.root());
    }
    else
    {
        close_document_request_ = doc.root();
    }
    close_confirmation_document_ = {};
    return true;
}

void application_workflow::cancel_close()
{
    exit_workflow_.cancel();
    close_confirmation_document_ = {};
}

void application_workflow::finish_file_dialog(const std::filesystem::path* path)
{
    const auto request{std::exchange(file_dialog_request_, {})};
    const auto target_root{std::exchange(file_dialog_document_, {})};
    if (!path)
    {
        return;
    }
    if (request == file_dialog_request::open_document)
    {
        open_document(*path);
        return;
    }
    save_document_as(target_root, *path, save_as_policy::require_confirmation);
}

bool application_workflow::validate_overwrite()
{
    if (save_as_overwrite_path_.empty())
    {
        return false;
    }
    const auto target{context_->documents().find_root(save_as_overwrite_document_)};
    if (!target || target->closing())
    {
        confirm_overwrite(false);
        return false;
    }
    return true;
}

void application_workflow::confirm_overwrite(bool overwrite)
{
    const std::filesystem::path destination{std::move(save_as_overwrite_path_)};
    const auto handle{std::exchange(save_as_overwrite_document_, {})};
    save_as_overwrite_path_.clear();
    if (overwrite)
    {
        save_document_as(handle, destination, save_as_policy::overwrite);
    }
}

void application_workflow::set_status(std::string message)
{
    context_->set_status(std::move(message));
}
} // namespace nori::editor
