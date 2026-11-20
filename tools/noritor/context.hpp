#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

#include <nori/resource/handle.hpp>

#include "asset_manager.hpp"
#include "clipboard.hpp"
#include "document_manager.hpp"
#include "document_workspace.hpp"
#include "settings.hpp"
#include "temporary_storage.hpp"

namespace nori::editor
{
class context
{
public:
    context();

    context(const context&) = delete;
    context(context&&) = delete;
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    asset_manager& assets();
    settings& preferences();
    temporary_storage& temporary();
    document_manager& documents();
    nori::editor::clipboard& clipboard();
    document_workspace& workspace(nori::resource::handle root);
    void remove_workspace(nori::resource::handle root);

    std::string_view status() const;
    void set_status(std::string message);

private:
    struct workspace_entry
    {
        document_workspace workspace;
        nori::core::multicast_delegate<void(const command_effect&)>::handle subscription;
    };

    asset_manager assets_;
    settings settings_;
    temporary_storage temporary_storage_;
    document_manager document_manager_;
    nori::editor::clipboard clipboard_;

    std::unordered_map<nori::resource::handle, workspace_entry> workspaces_;

    std::string status_message_;
};
} // namespace nori::editor
