#include "pch.hpp"

#include <algorithm>
#include <format>
#include <unordered_set>
#include <utility>

#include <nori/core/encoding.hpp>

#include "clipboard.hpp"
#include "document_editor.hpp"
#include "log.hpp"
#include "resource_helpers.hpp"

namespace nori::editor
{
selection_manager::selection_manager(nori::resource::handle document_root) :
    document_root_{document_root},
    selected_{}
{
}

nori::resource::handle selection_manager::selected() const
{
    return selected_;
}

std::span<const nori::resource::handle> selection_manager::selection() const
{
    return selection_;
}

std::vector<nori::resource::handle> selection_manager::top_level() const
{
    std::vector<nori::resource::handle> result;
    for (const nori::resource::handle candidate : selection_)
    {
        bool descendant{};
        for (nori::resource::handle parent{nori::resource::parent_of(candidate)}; parent; parent = nori::resource::parent_of(parent))
        {
            if (std::ranges::contains(selection_, parent))
            {
                descendant = true;
                break;
            }
        }
        if (!descendant && !std::ranges::contains(result, candidate))
        {
            result.push_back(candidate);
        }
    }
    return result;
}

void selection_manager::select(nori::resource::handle handle)
{
    if (handle == document_root_)
    {
        clear();
        return;
    }
    selection_ = {handle};
    selected_ = handle;
}

void selection_manager::toggle(nori::resource::handle handle)
{
    if (handle == document_root_)
    {
        clear();
        return;
    }
    if (const auto it{std::ranges::find(selection_, handle)}; it != selection_.end())
    {
        selection_.erase(it);
        if (selected_ == handle)
        {
            selected_ = selection_.empty() ? nori::resource::handle{} : selection_.back();
        }
        return;
    }
    selection_.push_back(handle);
    selected_ = handle;
}

void selection_manager::select_all(std::span<const nori::resource::handle> handles)
{
    selection_.clear();
    for (const nori::resource::handle handle : handles)
    {
        if (handle != document_root_ && !std::ranges::contains(selection_, handle))
        {
            selection_.push_back(handle);
        }
    }
    selected_ = selection_.empty() ? nori::resource::handle{} : selection_.back();
}

void selection_manager::remove(std::span<const nori::resource::handle> invalidated)
{
    std::erase_if(selection_, [invalidated](nori::resource::handle handle)
    {
        return std::ranges::contains(invalidated, handle);
    });
    if (selected_ && std::ranges::contains(invalidated, selected_))
    {
        selected_ = selection_.empty() ? nori::resource::handle{} : selection_.back();
    }
}

void selection_manager::clear()
{
    selected_ = {};
    selection_.clear();
}

bool selection_manager::contains(nori::resource::handle handle) const
{
    return std::ranges::contains(selection_, handle);
}

document_editor::document_editor(std::filesystem::path path, nori::resource::handle root) :
    document_{std::move(path), root},
    selection_{root}
{
}

const std::filesystem::path& document_editor::path() const
{
    return document_.path();
}

nori::resource::handle document_editor::root() const
{
    return document_.root();
}

bool document_editor::dirty() const
{
    return document_.dirty();
}

bool document_editor::closing() const
{
    return document_.closing();
}

bool document_editor::can_undo() const
{
    return document_.can_undo();
}

bool document_editor::can_redo() const
{
    return document_.can_redo();
}

nori::resource::result<command_effect> document_editor::execute(std::unique_ptr<command> value)
{
    if (closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    return finish(document_.command_manager_.execute(std::move(value)), false);
}

nori::resource::result<command_effect> document_editor::undo()
{
    if (closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    if (!can_undo())
    {
        return std::unexpected{nori::resource::error_code::not_found};
    }
    return finish(document_.command_manager_.undo(), false);
}

nori::resource::result<command_effect> document_editor::redo()
{
    if (closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    if (!can_redo())
    {
        return std::unexpected{nori::resource::error_code::not_found};
    }
    return finish(document_.command_manager_.redo(), false);
}

void document_editor::break_merge()
{
    if (!closing())
    {
        document_.command_manager_.break_merge();
    }
}

nori::resource::result<command_effect> document_editor::set_name(nori::resource::handle target, std::string name)
{
    return execute(std::make_unique<set_name_command>(root(), target, std::move(name)));
}

nori::resource::result<command_effect> document_editor::set_type(std::span<const nori::resource::handle> targets, nori::resource::property_type type)
{
    return execute(std::make_unique<set_type_command>(root(), std::vector<nori::resource::handle>{targets.begin(), targets.end()}, type));
}

nori::resource::result<command_effect> document_editor::set_value(std::span<const nori::resource::handle> targets, property_snapshot value)
{
    return std::visit([&](auto&& current) -> nori::resource::result<command_effect>
    {
        using T = std::decay_t<decltype(current)>;
        if constexpr (std::is_same_v<T, std::monostate>)
        {
            return std::unexpected{nori::resource::error_code::type_mismatch};
        }
        else
        {
            return execute(std::make_unique<set_value_command<T>>(root(), std::vector<nori::resource::handle>{targets.begin(), targets.end()}, std::move(current)));
        }
    }, std::move(value));
}

std::string document_editor::unique_child_name(nori::resource::handle parent, std::string_view base) const
{
    std::unordered_set<std::string> names;
    for (const nori::resource::handle child : nori::editor::children_of_loaded(parent))
    {
        names.insert(nori::resource::name_of(child));
    }
    for (std::size_t index{1};; ++index)
    {
        const std::string name{index == 1 ? std::string{base} : std::format("{}{}", base, index)};
        if (!names.contains(name))
        {
            return name;
        }
    }
}

nori::resource::result<command_effect> document_editor::create(nori::resource::handle parent, std::string name, property_snapshot value)
{
    return std::visit([&](auto&& current) -> nori::resource::result<command_effect>
    {
        using T = std::decay_t<decltype(current)>;
        return execute_select(std::make_unique<create_command<T>>(root(), parent, std::move(name), std::move(current)));
    }, std::move(value));
}

nori::resource::result<command_effect> document_editor::import_model(nori::resource::handle parent, std::string name, nori::asset::cooked_model model)
{
    return execute_select(std::make_unique<import_model_command>(root(), parent, std::move(name), std::move(model)));
}

nori::resource::result<command_effect> document_editor::remove(temporary_storage& temporary)
{
    return execute(std::make_unique<remove_command>(&temporary, root(), selection_.top_level()));
}

nori::resource::result<command_effect> document_editor::move(std::vector<move_command::entry> entries)
{
    return execute(std::make_unique<move_command>(root(), std::move(entries)));
}

nori::resource::result<command_effect> document_editor::copy(temporary_storage& temporary, std::vector<copy_source> sources)
{
    return execute_select(std::make_unique<copy_command>(&temporary, root(), std::move(sources)));
}

nori::resource::result<command_effect> document_editor::duplicate(temporary_storage& temporary)
{
    std::vector<std::pair<nori::resource::handle, std::string>> reserved;
    std::vector<copy_source> sources;
    for (const auto handle : selection_.top_level())
    {
        const auto parent{nori::resource::parent_of(handle)};
        const std::string original{nori::resource::name_of(handle)};
        std::string name;
        for (std::uint64_t index{1};; ++index)
        {
            name = index == 1 ? std::format("{} Copy", original) : std::format("{} Copy {}", original, index);
            const auto existing{nori::resource::try_get(parent, name)};
            if (!existing && existing.error() == nori::resource::error_code::not_found && !std::ranges::contains(reserved, std::pair{parent, name}))
            {
                break;
            }
            if (!existing && existing.error() != nori::resource::error_code::not_found)
            {
                return std::unexpected{existing.error()};
            }
        }
        reserved.emplace_back(parent, name);
        sources.push_back({handle, parent, name});
    }
    if (sources.empty())
    {
        return command_effect{};
    }
    return copy(temporary, std::move(sources));
}

nori::resource::result<void> document_editor::copy_to(clipboard& target)
{
    return target.copy(*this);
}

nori::resource::result<command_effect> document_editor::cut(clipboard& target)
{
    return target.cut(*this);
}

nori::resource::result<command_effect> document_editor::paste(clipboard& source, nori::resource::handle parent)
{
    return source.paste(*this, parent);
}

nori::resource::result<command_effect> document_editor::execute_select(std::unique_ptr<command> value)
{
    if (closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    return finish(document_.command_manager_.execute(std::move(value)), true);
}

nori::resource::result<command_effect> document_editor::finish(command_result result, bool select_created)
{
    auto& effect{result.execution.effect};
    selection_.remove(effect.invalidated);
    if (result.execution.result && select_created)
    {
        selection_.select_all(effect.created);
    }
    if (!effect.created.empty() || !effect.updated.empty() || !effect.invalidated.empty())
    {
        changed_.broadcast(effect);
    }
    if (!result.execution.result)
    {
        log::error(std::format("[{}] 작업 실패 ({}): {}", nori::core::path_to_utf8(path().filename()), result.description, nori::resource::to_string(result.execution.result.error())));
        return std::unexpected{result.execution.result.error()};
    }
    if (result.execution.changed)
    {
        log::info(std::format("[{}] {}", nori::core::path_to_utf8(path().filename()), result.description));
    }
    return std::move(effect);
}
} // namespace nori::editor
