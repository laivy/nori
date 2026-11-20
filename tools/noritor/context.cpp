#include "pch.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "context.hpp"

namespace nori::editor
{
context::context() :
    document_manager_{},
    clipboard_{&temporary_storage_},
    status_message_{}
{
}

asset_manager& context::assets()
{
    return assets_;
}

settings& context::preferences()
{
    return settings_;
}

temporary_storage& context::temporary()
{
    return temporary_storage_;
}

document_manager& context::documents()
{
    return document_manager_;
}

clipboard& context::clipboard()
{
    return clipboard_;
}

document_workspace& context::workspace(nori::resource::handle root)
{
    const auto existing{workspaces_.find(root)};
    if (existing != workspaces_.end())
    {
        return existing->second.workspace;
    }
    const auto editor{document_manager_.find_root(root)};
    assert(editor);
    const auto subscription{editor->changed().add([this, root](const command_effect& effect)
    {
        workspaces_.at(root).workspace.apply(effect);
    })};
    return workspaces_.emplace(root, workspace_entry{document_workspace{root}, subscription}).first->second.workspace;
}

void context::remove_workspace(nori::resource::handle root)
{
    const auto it{workspaces_.find(root)};
    if (it == workspaces_.end())
    {
        return;
    }
    if (const auto editor{document_manager_.find_root(root)})
    {
        std::ignore = editor->changed().remove(it->second.subscription);
    }
    workspaces_.erase(it);
}

std::string_view context::status() const
{
    return status_message_;
}

void context::set_status(std::string message)
{
    status_message_ = std::move(message);
}
} // namespace nori::editor
