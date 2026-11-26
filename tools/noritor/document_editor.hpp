#pragma once
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nori/core/multicast_delegate.hpp>
#include <nori/resource/handle.hpp>

#include "document.hpp"

namespace nori::editor
{
class selection_manager
{
public:
    explicit selection_manager(nori::resource::handle document_root);

    nori::resource::handle selected() const;
    std::span<const nori::resource::handle> selection() const;
    std::vector<nori::resource::handle> top_level() const;

    void select(nori::resource::handle handle);
    void toggle(nori::resource::handle handle);
    void select_all(std::span<const nori::resource::handle> handles);
    void remove(std::span<const nori::resource::handle> invalidated);
    void clear();

    bool contains(nori::resource::handle handle) const;

private:
    nori::resource::handle document_root_;
    nori::resource::handle selected_;
    std::vector<nori::resource::handle> selection_;
};

class clipboard;
class temporary_storage;

class document_editor
{
    friend class document_manager;

public:
    document_editor(std::filesystem::path path, nori::resource::handle root);

    const std::filesystem::path& path() const;
    nori::resource::handle root() const;

    bool dirty() const;
    bool closing() const;
    bool can_undo() const;
    bool can_redo() const;

    auto& selection(this auto& self)
    {
        return self.selection_;
    }

    auto& changed(this auto& self)
    {
        return self.changed_;
    }

    nori::resource::result<command_effect> execute(std::unique_ptr<command> value);
    nori::resource::result<command_effect> undo();
    nori::resource::result<command_effect> redo();
    void break_merge();

    nori::resource::result<command_effect> set_name(nori::resource::handle target, std::string name);
    nori::resource::result<command_effect> set_type(std::span<const nori::resource::handle> targets, nori::resource::property_type type);
    nori::resource::result<command_effect> set_value(std::span<const nori::resource::handle> targets, property_snapshot value);

    std::string unique_child_name(nori::resource::handle parent, std::string_view base = "Property") const;
    nori::resource::result<command_effect> create(nori::resource::handle parent, std::string name, property_snapshot value);
    nori::resource::result<command_effect> import_model(nori::resource::handle parent, std::string name, nori::asset::cooked_model model);
    nori::resource::result<command_effect> remove(temporary_storage& temporary);
    nori::resource::result<command_effect> move(std::vector<move_command::entry> entries);
    nori::resource::result<command_effect> copy(temporary_storage& temporary, std::vector<copy_source> sources);
    nori::resource::result<command_effect> duplicate(temporary_storage& temporary);

    nori::resource::result<void> copy_to(clipboard& target);
    nori::resource::result<command_effect> cut(clipboard& target);
    nori::resource::result<command_effect> paste(clipboard& source, nori::resource::handle parent);

private:
    nori::resource::result<command_effect> execute_select(std::unique_ptr<command> value);
    nori::resource::result<command_effect> finish(command_result result, bool select_created);

    document document_;
    selection_manager selection_;

    nori::core::multicast_delegate<void(const command_effect&)> changed_;
};
} // namespace nori::editor
