#include "pch.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/resource/operations.hpp>
#include <nori/resource/path.hpp>

#include "command.hpp"
#include "resource_helpers.hpp"
#include "temporary_storage.hpp"

namespace
{
using nori::editor::command_effect;
using nori::editor::command_execution;
using nori::editor::property_snapshot;
using nori::editor::temporary_storage;

command_execution success(command_effect effect, bool changed)
{
    return {std::move(effect), {}, changed};
}

command_execution failure(nori::resource::error_code error, command_effect effect = {})
{
    return {std::move(effect), std::unexpected{error}, false};
}

void append_unique(std::vector<nori::resource::handle>& destination, std::span<const nori::resource::handle> source)
{
    for (const nori::resource::handle handle : source)
    {
        if (!std::ranges::contains(destination, handle))
        {
            destination.push_back(handle);
        }
    }
}

std::string display_path(std::string_view path)
{
    return path.empty() ? "/" : std::format("/{}", path);
}

std::string append_property_path(std::string_view parent, std::string_view name)
{
    const nori::resource::path name_path{name};
    if (parent.empty())
    {
        return name_path.string();
    }
    return (nori::resource::path{parent} / name_path).string();
}

template <typename T>
std::string display_value(const T& value)
{
    if constexpr (std::same_as<T, std::string>)
    {
        return std::format("'{}'", value);
    }
    else if constexpr (std::same_as<T, nori::resource::image>)
    {
        return std::format("image ({}x{})", value.width, value.height);
    }
    else if constexpr (std::same_as<T, nori::resource::binary>)
    {
        return std::format("binary ({} bytes)", value.data.size());
    }
    else if constexpr (std::same_as<T, std::monostate>)
    {
        return "empty";
    }
    else
    {
        return std::format("{}", value);
    }
}

property_snapshot default_property_snapshot(nori::resource::property_type type)
{
    switch (type)
    {
    case nori::resource::property_type::boolean:
    {
        return false;
    }
    case nori::resource::property_type::int32:
    {
        return std::int32_t{};
    }
    case nori::resource::property_type::int64:
    {
        return std::int64_t{};
    }
    case nori::resource::property_type::float32:
    {
        return 0.0f;
    }
    case nori::resource::property_type::string:
    {
        return std::string{};
    }
    default:
        break;
    }
    return std::monostate{};
}

void set_property_snapshot(nori::resource::handle target, const property_snapshot& value)
{
    std::visit([target]<typename T>(const T& current)
    {
        if constexpr (std::same_as<T, std::monostate>)
        {
            nori::resource::set(target, nori::resource::empty);
        }
        else
        {
            nori::resource::set(target, current);
        }
    }, value);
}

std::string display_property_snapshot(const property_snapshot& value)
{
    return std::visit([](const auto& current)
    {
        return display_value(current);
    }, value);
}

std::vector<nori::resource::handle> collect_subtree_handles(nori::resource::handle root)
{
    std::vector<nori::resource::handle> result;
    std::vector<nori::resource::handle> pending{root};
    while (!pending.empty())
    {
        const nori::resource::handle current{pending.back()};
        pending.pop_back();
        result.push_back(current);
        const std::vector<nori::resource::handle> children{nori::editor::children_of_loaded(current)};
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    return result;
}

template <typename T>
nori::resource::result<nori::resource::handle> create_child(nori::resource::handle parent, std::string_view name, const T& value)
{
    if (!nori::resource::is_valid_property_name(name))
    {
        return std::unexpected{nori::resource::error_code::invalid_path};
    }
    const nori::resource::handle created = [&]()
    {
        if constexpr (std::same_as<T, std::monostate>)
        {
            return nori::resource::create(parent, name, nori::resource::empty);
        }
        else
        {
            return nori::resource::create(parent, name, value);
        }
    }();
    if (!created)
    {
        return std::unexpected{nori::resource::error_code::name_conflict};
    }
    return created;
}

nori::resource::result<nori::resource::handle> materialize_subtree(temporary_storage& temporary, nori::resource::handle backup, nori::resource::handle parent, std::string_view name, std::optional<std::size_t> position = std::nullopt)
{
    if (!nori::resource::is_valid_property_name(name))
    {
        return std::unexpected{nori::resource::error_code::invalid_path};
    }
    const auto staging{temporary.store(backup)};
    if (!staging)
    {
        return std::unexpected{staging.error()};
    }
    if (nori::resource::name_of(*staging) != name)
    {
        if (const auto res{nori::resource::rename(*staging, name)}; !res)
        {
            temporary.remove(*staging);
            return std::unexpected{res.error()};
        }
    }
    if (position && *position > nori::editor::children_of_loaded(parent).size())
    {
        temporary.remove(*staging);
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    const nori::resource::handle staging_parent{nori::resource::parent_of(*staging)};
    const auto copied{nori::resource::move(*staging, parent)};
    nori::resource::remove(staging_parent);
    if (!copied)
    {
        return std::unexpected{copied.error()};
    }
    if (position)
    {
        if (const auto res{nori::resource::reorder(*copied, *position)}; !res)
        {
            nori::resource::remove(*copied);
            return std::unexpected{res.error()};
        }
    }
    return *copied;
}

template <typename T>
nori::resource::result<property_snapshot> copy_property_snapshot_as(nori::resource::handle target)
{
    const auto value{nori::resource::try_get<T>(target)};
    if (!value)
    {
        return std::unexpected{value.error()};
    }
    return property_snapshot{*value};
}
} // namespace

namespace nori::editor
{
bool is_primitive_type(nori::resource::property_type type)
{
    switch (type)
    {
    case nori::resource::property_type::boolean:
    case nori::resource::property_type::int32:
    case nori::resource::property_type::int64:
    case nori::resource::property_type::float32:
    case nori::resource::property_type::string:
    {
        return true;
    }
    default:
        break;
    }
    return false;
}

bool command::merge(const command&)
{
    return false;
}

bool command::net_noop() const
{
    return false;
}

nori::resource::result<nori::resource::handle> resolve_property(nori::resource::handle document_root, std::string_view subpath)
{
    if (subpath.empty())
    {
        return document_root;
    }
    return nori::resource::try_get(document_root, subpath);
}

nori::resource::result<std::string> relative_property_path(nori::resource::handle document_root, nori::resource::handle target)
{
    if (target == document_root)
    {
        return std::string{};
    }

    for (nori::resource::handle current{target}; current; current = nori::resource::parent_of(current))
    {
        if (current == document_root)
        {
            const nori::resource::path target_path{nori::resource::path_of(target)};
            nori::resource::subpath_iterator subpath{target_path};
            std::vector<std::string_view> segments;
            for (; subpath != nori::resource::subpath_iterator{}; ++subpath)
            {
                segments.push_back(*subpath);
            }
            if (segments.empty())
            {
                return std::string{};
            }

            nori::resource::path result{segments.front()};
            for (std::size_t i{1}; i < segments.size(); ++i)
            {
                result = result / segments[i];
            }
            return result.string();
        }
    }
    return std::unexpected{nori::resource::error_code::invalid_relationship};
}

nori::resource::result<property_snapshot> copy_property_snapshot(nori::resource::handle target)
{
    switch (nori::resource::type_of(target))
    {
    case nori::resource::property_type::empty:
    {
        return std::monostate{};
    }
    case nori::resource::property_type::boolean:
    {
        return copy_property_snapshot_as<bool>(target);
    }
    case nori::resource::property_type::int32:
    {
        return copy_property_snapshot_as<std::int32_t>(target);
    }
    case nori::resource::property_type::int64:
    {
        return copy_property_snapshot_as<std::int64_t>(target);
    }
    case nori::resource::property_type::float32:
    {
        return copy_property_snapshot_as<float>(target);
    }
    case nori::resource::property_type::string:
    {
        return copy_property_snapshot_as<std::string>(target);
    }
    case nori::resource::property_type::image:
    {
        return copy_property_snapshot_as<nori::resource::image>(target);
    }
    case nori::resource::property_type::binary:
    {
        return copy_property_snapshot_as<nori::resource::binary>(target);
    }
    default:
        return std::unexpected{nori::resource::error_code::type_mismatch};
    }
}

template <typename T>
create_command<T>::create_command(nori::resource::handle document_root, nori::resource::handle parent, std::string name, T initial_value) :
    document_root_{document_root},
    parent_{parent},
    parent_path_{},
    name_{std::move(name)},
    initial_value_{std::move(initial_value)}
{
}

template <typename T>
command_execution create_command<T>::execute()
{
    if (parent_)
    {
        const auto parent_path{relative_property_path(document_root_, parent_)};
        if (!parent_path)
        {
            return failure(parent_path.error());
        }
        parent_path_ = *parent_path;
        parent_ = {};
    }
    const auto parent{resolve_property(document_root_, parent_path_)};
    if (!parent)
    {
        return failure(parent.error());
    }
    const auto created{create_child(*parent, name_, initial_value_)};
    if (!created)
    {
        return failure(created.error());
    }
    return success({.created = {*created}}, true);
}

template <typename T>
command_execution create_command<T>::undo()
{
    const auto target{resolve_property(document_root_, append_property_path(parent_path_, name_))};
    if (!target)
    {
        return failure(target.error());
    }
    std::vector<nori::resource::handle> invalidated{collect_subtree_handles(*target)};
    nori::resource::remove(*target);
    return success({.invalidated = std::move(invalidated)}, true);
}

template <typename T>
std::string create_command<T>::description() const
{
    return std::format("추가: '{}' = {}", display_path(append_property_path(parent_path_, name_)), display_value(initial_value_));
}
} // namespace nori::editor

namespace
{
using nori::editor::property_snapshot;
using nori::editor::temporary_storage;

template <typename T>
nori::resource::result<nori::resource::handle> create_model_value(nori::resource::handle parent, const nori::resource::path& subpath, const T& value)
{
    if (subpath.has_file_path())
    {
        return std::unexpected{nori::resource::error_code::invalid_path};
    }

    nori::resource::subpath_iterator it{subpath};
    std::vector<std::string_view> segments;
    for (; it != nori::resource::subpath_iterator{}; ++it)
    {
        segments.push_back(*it);
    }
    if (segments.empty())
    {
        return std::unexpected{nori::resource::error_code::invalid_path};
    }

    nori::resource::handle current{parent};
    for (const std::string_view folder_name : std::span{segments.data(), segments.size() - 1})
    {
        const auto existing{nori::resource::try_get(current, folder_name)};
        if (existing)
        {
            current = *existing;
        }
        else
        {
            if (existing.error() != nori::resource::error_code::not_found)
            {
                return std::unexpected{existing.error()};
            }
            const auto folder{create_child(current, folder_name, std::monostate{})};
            if (!folder)
            {
                return std::unexpected{folder.error()};
            }
            current = *folder;
        }
    }
    return create_child(current, segments.back(), value);
}

nori::resource::result<nori::resource::handle> create_model_subtree(nori::resource::handle parent, std::string_view name, const nori::asset::cooked_model& model)
{
    const auto root{create_child(parent, name, model.model)};
    if (!root)
    {
        return std::unexpected{root.error()};
    }

    const auto create_binaries = [root_handle{*root}](std::string_view category, const std::vector<nori::asset::cooked_binary>& values) -> nori::resource::result<void>
    {
        for (const nori::asset::cooked_binary& value : values)
        {
            const auto created{create_model_value(
                root_handle,
                nori::resource::path{category} / value.name,
                value.binary
            )};
            if (!created)
            {
                return std::unexpected{created.error()};
            }
        }
        return {};
    };

    if (const auto res{create_binaries("meshes", model.meshes)}; !res)
    {
        nori::resource::remove(*root);
        return std::unexpected{res.error()};
    }
    if (const auto res{create_binaries("animations", model.animations)}; !res)
    {
        nori::resource::remove(*root);
        return std::unexpected{res.error()};
    }
    for (const nori::asset::cooked_texture& value : model.textures)
    {
        const auto created{create_model_value(
            *root,
            nori::resource::path{"textures"} / value.name,
            value.image
        )};
        if (!created)
        {
            nori::resource::remove(*root);
            return std::unexpected{created.error()};
        }
    }
    return *root;
}
} // namespace

namespace nori::editor
{
import_model_command::import_model_command(nori::resource::handle document_root, nori::resource::handle parent, std::string name, nori::asset::cooked_model model) :
    document_root_{document_root},
    parent_{parent},
    parent_path_{},
    name_{std::move(name)},
    model_{std::move(model)}
{
}

command_execution import_model_command::execute()
{
    if (parent_)
    {
        const auto parent_path{relative_property_path(document_root_, parent_)};
        if (!parent_path)
        {
            return failure(parent_path.error());
        }
        parent_path_ = *parent_path;
        parent_ = {};
    }
    const auto parent{resolve_property(document_root_, parent_path_)};
    if (!parent)
    {
        return failure(parent.error());
    }
    const auto existing{nori::resource::try_get(*parent, name_)};
    if (existing)
    {
        return failure(nori::resource::error_code::name_conflict);
    }
    if (existing.error() != nori::resource::error_code::not_found)
    {
        return failure(existing.error());
    }
    const auto created{create_model_subtree(*parent, name_, model_)};
    if (!created)
    {
        return failure(created.error());
    }
    return success({.created = {*created}}, true);
}

command_execution import_model_command::undo()
{
    const auto target{resolve_property(document_root_, append_property_path(parent_path_, name_))};
    if (!target)
    {
        return failure(target.error());
    }
    std::vector<nori::resource::handle> invalidated{collect_subtree_handles(*target)};
    nori::resource::remove(*target);
    return success({.invalidated = std::move(invalidated)}, true);
}

std::string import_model_command::description() const
{
    return std::format("가져오기: '{}'", display_path(append_property_path(parent_path_, name_)));
}

copy_command::copy_command(temporary_storage* temporary, nori::resource::handle document_root, std::vector<copy_source> sources) :
    temporary_{temporary},
    document_root_{document_root},
    sources_{std::move(sources)}
{
}

copy_command::~copy_command()
{
    for (const entry& value : entries_)
    {
        temporary_->remove(value.backup);
        if (value.overwritten_backup)
        {
            temporary_->remove(value.overwritten_backup);
        }
    }
}

command_execution copy_command::execute()
{
    if (entries_.empty())
    {
        const command_execution prepared{prepare()};
        if (!prepared.result)
        {
            return prepared;
        }
    }
    return materialize();
}

command_execution copy_command::undo()
{
    struct resolved_entry
    {
        const entry* value;
        nori::resource::handle target;
        nori::resource::handle parent;
        std::size_t index;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (std::size_t index{0}; index < entries_.size(); ++index)
    {
        const entry& value{entries_[index]};
        const auto parent{resolve_property(document_root_, value.parent_path)};
        const auto target{resolve_property(document_root_, append_property_path(value.parent_path, value.name))};
        if (!parent || !target)
        {
            return failure(!parent ? parent.error() : target.error());
        }
        resolved.push_back({&value, *target, *parent, index});
    }

    command_effect effect;
    std::vector<std::vector<nori::resource::handle>> invalidated_by_entry(entries_.size());
    std::vector<nori::resource::handle> created_by_entry(entries_.size());
    std::vector<const resolved_entry*> applied;
    for (const resolved_entry& current : resolved | std::views::reverse)
    {
        const std::vector<nori::resource::handle> invalidated{collect_subtree_handles(current.target)};
        invalidated_by_entry[current.index] = invalidated;
        nori::resource::remove(current.target);
        append_unique(effect.invalidated, invalidated);
        if (current.value->overwritten_backup)
        {
            const auto restored{materialize_subtree(
                *temporary_, current.value->overwritten_backup, current.parent, current.value->name, current.value->overwritten_position
            )};
            if (!restored)
            {
                const auto rollback{materialize_subtree(
                    *temporary_, current.value->backup, current.parent, current.value->name, current.value->position
                )};
                if (!rollback)
                {
                    nori::core::abort("failed to rollback copy command undo");
                }
                effect.created.push_back(*rollback);
                for (const resolved_entry* const previous : applied | std::views::reverse)
                {
                    if (previous->value->overwritten_backup)
                    {
                        const auto previous_restored{resolve_property(
                            document_root_,
                            append_property_path(previous->value->parent_path, previous->value->name)
                        )};
                        if (!previous_restored)
                        {
                            nori::core::abort("failed to resolve copy command undo rollback");
                        }
                        append_unique(effect.invalidated, collect_subtree_handles(*previous_restored));
                        nori::resource::remove(*previous_restored);
                    }
                    const auto previous_copy{materialize_subtree(
                        *temporary_,
                        previous->value->backup,
                        previous->parent,
                        previous->value->name,
                        previous->value->position
                    )};
                    if (!previous_copy)
                    {
                        nori::core::abort("failed to rollback earlier copy command undo entry");
                    }
                    effect.created.push_back(*previous_copy);
                }
                return failure(restored.error(), std::move(effect));
            }
            effect.created.push_back(*restored);
            created_by_entry[current.index] = *restored;
        }
        applied.push_back(&current);
    }
    command_effect ordered_effect;
    for (std::size_t index{0}; index < entries_.size(); ++index)
    {
        append_unique(ordered_effect.invalidated, invalidated_by_entry[index]);
        if (created_by_entry[index])
        {
            ordered_effect.created.push_back(created_by_entry[index]);
        }
    }
    return success(std::move(ordered_effect), true);
}

std::string copy_command::description() const
{
    const std::size_t count{entries_.empty() ? sources_.size() : entries_.size()};
    if (count == 1 && !entries_.empty())
    {
        return std::format("복사: '{}'", display_path(append_property_path(entries_.front().parent_path, entries_.front().name)));
    }
    return std::format("{}개 복사", count);
}

command_execution copy_command::prepare()
{
    std::unordered_set<std::string> destinations;
    std::vector<entry> prepared;
    prepared.reserve(sources_.size());
    for (const copy_source& source : sources_)
    {
        const auto parent_path{relative_property_path(document_root_, source.parent)};
        if (!parent_path)
        {
            for (const entry& value : prepared)
            {
                temporary_->remove(value.backup);
            }
            return failure(parent_path.error());
        }
        const std::string destination{append_property_path(*parent_path, source.name)};
        if (!destinations.insert(destination).second)
        {
            for (const entry& value : prepared)
            {
                temporary_->remove(value.backup);
            }
            return failure(nori::resource::error_code::name_conflict);
        }
        const auto backup{temporary_->store(source.source)};
        if (!backup)
        {
            for (const entry& value : prepared)
            {
                temporary_->remove(value.backup);
            }
            return failure(backup.error());
        }
        prepared.push_back({*backup, {}, *parent_path, source.name, source.position, {}});
    }

    for (entry& value : prepared)
    {
        const auto parent{resolve_property(document_root_, value.parent_path)};
        if (!parent)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
                if (cleanup.overwritten_backup)
                {
                    temporary_->remove(cleanup.overwritten_backup);
                }
            }
            return failure(parent.error());
        }
        const auto existing{nori::resource::try_get(*parent, value.name)};
        if (!existing)
        {
            if (existing.error() != nori::resource::error_code::not_found)
            {
                for (const entry& cleanup : prepared)
                {
                    temporary_->remove(cleanup.backup);
                    if (cleanup.overwritten_backup)
                    {
                        temporary_->remove(cleanup.overwritten_backup);
                    }
                }
                return failure(existing.error());
            }
            continue;
        }
        if (nori::resource::type_of(*existing) != nori::resource::type_of(value.backup))
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
                if (cleanup.overwritten_backup)
                {
                    temporary_->remove(cleanup.overwritten_backup);
                }
            }
            return failure(nori::resource::error_code::name_conflict);
        }
        const auto overwritten{temporary_->store(*existing)};
        if (!overwritten)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
                if (cleanup.overwritten_backup)
                {
                    temporary_->remove(cleanup.overwritten_backup);
                }
            }
            return failure(overwritten.error());
        }
        value.overwritten_backup = *overwritten;
        const auto overwritten_position{nori::resource::position_of(*existing)};
        if (!overwritten_position)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
                if (cleanup.overwritten_backup)
                {
                    temporary_->remove(cleanup.overwritten_backup);
                }
            }
            return failure(overwritten_position.error());
        }
        value.overwritten_position = *overwritten_position;
        if (!value.position)
        {
            value.position = *overwritten_position;
        }
    }

    entries_ = std::move(prepared);
    sources_.clear();
    return success({}, false);
}

command_execution copy_command::materialize()
{
    struct resolved_entry
    {
        const entry* value;
        nori::resource::handle parent;
        nori::resource::handle existing;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (const entry& value : entries_)
    {
        const auto parent{resolve_property(document_root_, value.parent_path)};
        if (!parent)
        {
            return failure(parent.error());
        }
        const auto existing{nori::resource::try_get(*parent, value.name)};
        if (value.overwritten_backup)
        {
            if (!existing || nori::resource::type_of(*existing) != nori::resource::type_of(value.overwritten_backup))
            {
                return failure(!existing ? existing.error() : nori::resource::error_code::name_conflict);
            }
            resolved.push_back({&value, *parent, *existing});
        }
        else
        {
            if (existing)
            {
                return failure(nori::resource::error_code::name_conflict);
            }
            if (existing.error() != nori::resource::error_code::not_found)
            {
                return failure(existing.error());
            }
            resolved.push_back({&value, *parent, {}});
        }
    }

    command_effect effect;
    std::vector<std::pair<const resolved_entry*, nori::resource::handle>> applied;
    for (const resolved_entry& current : resolved)
    {
        if (current.existing)
        {
            const std::vector<nori::resource::handle> invalidated{collect_subtree_handles(current.existing)};
            nori::resource::remove(current.existing);
            append_unique(effect.invalidated, invalidated);
        }
        const auto created{materialize_subtree(
            *temporary_, current.value->backup, current.parent, current.value->name, current.value->position
        )};
        if (!created)
        {
            if (current.value->overwritten_backup)
            {
                const auto restored{materialize_subtree(
                    *temporary_, current.value->overwritten_backup, current.parent, current.value->name, current.value->overwritten_position
                )};
                if (!restored)
                {
                    nori::core::abort("failed to rollback current copy command entry");
                }
                effect.created.push_back(*restored);
            }
            for (const auto& [previous, previous_created] : applied | std::views::reverse)
            {
                const std::vector<nori::resource::handle> invalidated{collect_subtree_handles(previous_created)};
                nori::resource::remove(previous_created);
                append_unique(effect.invalidated, invalidated);
                if (previous->value->overwritten_backup)
                {
                    const auto restored{materialize_subtree(
                        *temporary_, previous->value->overwritten_backup, previous->parent, previous->value->name, previous->value->overwritten_position
                    )};
                    if (!restored)
                    {
                        nori::core::abort("failed to rollback copy command");
                    }
                    effect.created.push_back(*restored);
                }
            }
            return failure(created.error(), std::move(effect));
        }
        effect.created.push_back(*created);
        applied.emplace_back(&current, *created);
    }
    return success(std::move(effect), true);
}

remove_command::remove_command(temporary_storage* temporary, nori::resource::handle document_root, std::vector<nori::resource::handle> targets) :
    temporary_{temporary},
    document_root_{document_root},
    targets_{std::move(targets)}
{
}

remove_command::~remove_command()
{
    for (const entry& value : entries_)
    {
        temporary_->remove(value.backup);
    }
}

command_execution remove_command::execute()
{
    if (entries_.empty())
    {
        const command_execution prepared{prepare()};
        if (!prepared.result)
        {
            return prepared;
        }
    }
    return remove();
}

command_execution remove_command::undo()
{
    std::vector<nori::resource::handle> parents;
    parents.reserve(entries_.size());
    for (const entry& value : entries_)
    {
        const auto parent{resolve_property(document_root_, value.parent_path)};
        if (!parent)
        {
            return failure(parent.error());
        }
        const auto existing{nori::resource::try_get(*parent, value.name)};
        if (existing)
        {
            return failure(nori::resource::error_code::name_conflict);
        }
        if (existing.error() != nori::resource::error_code::not_found)
        {
            return failure(existing.error());
        }
        parents.push_back(*parent);
    }

    std::vector<std::size_t> restore_order;
    restore_order.reserve(entries_.size());
    for (std::size_t index{0}; index < entries_.size(); ++index)
    {
        restore_order.push_back(index);
    }
    std::ranges::stable_sort(
        restore_order,
        [&parents, this](std::size_t lhs, std::size_t rhs)
    {
        const auto lhs_identity{std::tuple{parents[lhs].hier.index, parents[lhs].hier.generation, parents[lhs].node.index, parents[lhs].node.generation}};
        const auto rhs_identity{std::tuple{parents[rhs].hier.index, parents[rhs].hier.generation, parents[rhs].node.index, parents[rhs].node.generation}};
        if (lhs_identity != rhs_identity)
        {
            return lhs_identity < rhs_identity;
        }
        return entries_[lhs].position < entries_[rhs].position;
    }
    );

    command_effect effect;
    std::vector<nori::resource::handle> restored;
    std::vector<nori::resource::handle> created_by_entry(entries_.size());
    for (const std::size_t index : restore_order)
    {
        const entry& value{entries_[index]};
        const auto created{materialize_subtree(*temporary_, value.backup, parents[index], value.name, value.position)};
        if (!created)
        {
            for (const nori::resource::handle handle : restored | std::views::reverse)
            {
                const std::vector<nori::resource::handle> invalidated{collect_subtree_handles(handle)};
                nori::resource::remove(handle);
                append_unique(effect.invalidated, invalidated);
            }
            return failure(created.error(), std::move(effect));
        }
        restored.push_back(*created);
        created_by_entry[index] = *created;
    }
    for (const nori::resource::handle created : created_by_entry)
    {
        if (created)
        {
            effect.created.push_back(created);
        }
    }
    return success(std::move(effect), true);
}

std::string remove_command::description() const
{
    const std::size_t count{entries_.empty() ? targets_.size() : entries_.size()};
    if (count == 1 && !entries_.empty())
    {
        return std::format("제거: '{}'", display_path(append_property_path(entries_.front().parent_path, entries_.front().name)));
    }
    return std::format("{}개 제거", count);
}

command_execution remove_command::prepare()
{
    std::vector<entry> prepared;
    prepared.reserve(targets_.size());
    for (const nori::resource::handle target : targets_)
    {
        if (target == document_root_)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
            }
            return failure(nori::resource::error_code::invalid_relationship);
        }
        const auto path{relative_property_path(document_root_, target)};
        if (!path)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
            }
            return failure(path.error());
        }
        const auto backup{temporary_->store(target)};
        if (!backup)
        {
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
            }
            return failure(backup.error());
        }
        const auto parent_path{relative_property_path(document_root_, nori::resource::parent_of(target))};
        if (!parent_path)
        {
            temporary_->remove(*backup);
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
            }
            return failure(parent_path.error());
        }
        const auto position{nori::resource::position_of(target)};
        if (!position)
        {
            temporary_->remove(*backup);
            for (const entry& cleanup : prepared)
            {
                temporary_->remove(cleanup.backup);
            }
            return failure(position.error());
        }
        prepared.push_back({*backup, *parent_path, nori::resource::name_of(target), *position});
    }
    entries_ = std::move(prepared);
    targets_.clear();
    return success({}, false);
}

command_execution remove_command::remove()
{
    std::vector<nori::resource::handle> targets;
    targets.reserve(entries_.size());
    for (const entry& value : entries_)
    {
        const auto target{resolve_property(document_root_, append_property_path(value.parent_path, value.name))};
        if (!target)
        {
            return failure(target.error());
        }
        targets.push_back(*target);
    }

    command_effect effect;
    for (const nori::resource::handle target : targets)
    {
        const std::vector<nori::resource::handle> invalidated{collect_subtree_handles(target)};
        nori::resource::remove(target);
        append_unique(effect.invalidated, invalidated);
    }
    return success(std::move(effect), true);
}

move_command::move_command(nori::resource::handle document_root, std::vector<move_command::entry> entries) :
    document_root_{document_root},
    inputs_{std::move(entries)},
    entries_{}
{
}

command_execution move_command::execute()
{
    if (!inputs_.empty())
    {
        const command_execution prepared{prepare()};
        if (!prepared.result)
        {
            return prepared;
        }
    }
    return move(true);
}

command_execution move_command::undo()
{
    return move(false);
}

std::string move_command::description() const
{
    const std::size_t count{entries_.empty() ? inputs_.size() : entries_.size()};
    if (count == 1 && !entries_.empty())
    {
        return std::format("이동: '{}' -> '{}'", display_path(entries_.front().before_path), display_path(entries_.front().after_parent_path));
    }
    return std::format("{}개 이동", count);
}

command_execution move_command::prepare()
{
    std::vector<prepared_entry> prepared;
    prepared.reserve(inputs_.size());
    for (const entry& input : inputs_)
    {
        const nori::resource::handle before_parent{nori::resource::parent_of(input.source)};
        const auto before_path{relative_property_path(document_root_, input.source)};
        const auto before_parent_path{relative_property_path(document_root_, before_parent)};
        const auto after_parent_path{relative_property_path(document_root_, input.destination)};
        const auto before_position{nori::resource::position_of(input.source)};
        if (!before_path || !before_parent_path || !after_parent_path || !before_position)
        {
            return failure(!before_path ? before_path.error() : (!before_parent_path ? before_parent_path.error() : (!after_parent_path ? after_parent_path.error() : before_position.error())));
        }
        if (*before_parent_path == *after_parent_path && !input.position)
        {
            continue;
        }
        prepared.push_back({*before_path, append_property_path(*after_parent_path, nori::resource::name_of(input.source)), *before_parent_path, *after_parent_path, *before_position, input.position});
    }
    entries_ = std::move(prepared);
    inputs_.clear();
    return success({}, false);
}

command_execution move_command::move(bool forward)
{
    if (entries_.empty())
    {
        return success({}, false);
    }

    struct resolved_entry
    {
        nori::resource::handle source;
        nori::resource::handle destination;
        nori::resource::handle rollback_parent;
        std::size_t index;
    };

    const auto apply_positions = [this](std::span<const resolved_entry> values, bool use_after) -> nori::resource::result<void>
    {
        std::vector<const resolved_entry*> positioned;
        for (const resolved_entry& value : values)
        {
            if (!use_after || entries_[value.index].after_position)
            {
                positioned.push_back(&value);
            }
        }
        std::ranges::sort(positioned, {}, [](const resolved_entry* value)
        {
            return value->index;
        });
        for (const resolved_entry* const value : positioned)
        {
            const nori::resource::handle current_parent{nori::resource::parent_of(value->source)};
            const std::size_t last{nori::editor::children_of_loaded(current_parent).size() - 1};
            if (const auto res{nori::resource::reorder(value->source, last)}; !res)
            {
                return res;
            }
        }
        std::ranges::stable_sort(
            positioned,
            [this, use_after](const resolved_entry* lhs, const resolved_entry* rhs)
        {
            const nori::resource::handle lhs_parent{nori::resource::parent_of(lhs->source)};
            const nori::resource::handle rhs_parent{nori::resource::parent_of(rhs->source)};
            const auto lhs_identity{std::tuple{lhs_parent.hier.index, lhs_parent.hier.generation, lhs_parent.node.index, lhs_parent.node.generation}};
            const auto rhs_identity{std::tuple{rhs_parent.hier.index, rhs_parent.hier.generation, rhs_parent.node.index, rhs_parent.node.generation}};
            if (lhs_identity != rhs_identity)
            {
                return lhs_identity < rhs_identity;
            }
            const std::size_t lhs_position{use_after ? *entries_[lhs->index].after_position : entries_[lhs->index].before_position};
            const std::size_t rhs_position{use_after ? *entries_[rhs->index].after_position : entries_[rhs->index].before_position};
            return lhs_position < rhs_position;
        }
        );
        for (const resolved_entry* const value : positioned)
        {
            const std::size_t position{use_after ? *entries_[value->index].after_position : entries_[value->index].before_position};
            if (const auto res{nori::resource::reorder(value->source, position)}; !res)
            {
                return res;
            }
        }
        return {};
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (std::size_t offset{0}; offset < entries_.size(); ++offset)
    {
        const std::size_t index{forward ? offset : entries_.size() - 1 - offset};
        const prepared_entry& entry{entries_[index]};
        const auto source{resolve_property(document_root_, forward ? entry.before_path : entry.after_path)};
        const auto destination{resolve_property(document_root_, forward ? entry.after_parent_path : entry.before_parent_path)};
        const auto rollback{resolve_property(document_root_, forward ? entry.before_parent_path : entry.after_parent_path)};
        if (!source || !destination || !rollback)
        {
            return failure(!source ? source.error() : (!destination ? destination.error() : rollback.error()));
        }
        for (nori::resource::handle current{*destination}; current; current = nori::resource::parent_of(current))
        {
            if (current == *source)
            {
                return failure(nori::resource::error_code::invalid_relationship);
            }
        }
        const auto existing{nori::resource::try_get(*destination, nori::resource::name_of(*source))};
        if (existing && *existing != *source)
        {
            return failure(nori::resource::error_code::name_conflict);
        }
        if (!existing && existing.error() != nori::resource::error_code::not_found)
        {
            return failure(existing.error());
        }
        resolved.push_back({*source, *destination, *rollback, index});
    }

    std::size_t applied{};
    for (; applied < resolved.size(); ++applied)
    {
        const auto result{nori::resource::move(resolved[applied].source, resolved[applied].destination)};
        if (!result)
        {
            for (std::size_t index{applied}; index > 0; --index)
            {
                if (!nori::resource::move(resolved[index - 1].source, resolved[index - 1].rollback_parent))
                {
                    nori::core::abort("failed to rollback move command");
                }
            }
            const std::span restored{resolved.data(), applied};
            if (!apply_positions(restored, !forward))
            {
                nori::core::abort("failed to rollback move command positions");
            }
            return failure(result.error());
        }
    }

    if (forward)
    {
        for (const resolved_entry& value : resolved)
        {
            if (entries_[value.index].after_position)
            {
                const std::size_t sibling_count{nori::editor::children_of_loaded(nori::resource::parent_of(value.source)).size()};
                if (*entries_[value.index].after_position >= sibling_count)
                {
                    for (const resolved_entry& rollback : resolved | std::views::reverse)
                    {
                        if (!nori::resource::move(rollback.source, rollback.rollback_parent))
                        {
                            nori::core::abort("failed to rollback move command position validation");
                        }
                    }
                    if (!apply_positions(resolved, false))
                    {
                        nori::core::abort("failed to rollback move command positions");
                    }
                    return failure(nori::resource::error_code::invalid_relationship);
                }
            }
        }
    }

    const auto positioned{apply_positions(resolved, forward)};
    if (!positioned)
    {
        for (const resolved_entry& rollback : resolved | std::views::reverse)
        {
            if (!nori::resource::move(rollback.source, rollback.rollback_parent))
            {
                nori::core::abort("failed to rollback move command positioning");
            }
        }
        if (!apply_positions(resolved, !forward))
        {
            nori::core::abort("failed to rollback move command positions");
        }
        return failure(positioned.error());
    }

    if (forward)
    {
        for (const resolved_entry& value : resolved)
        {
            if (!entries_[value.index].after_position)
            {
                const auto position{nori::resource::position_of(value.source)};
                if (!position)
                {
                    nori::core::abort("failed to capture move command position");
                }
                entries_[value.index].after_position = *position;
            }
        }
    }

    bool changed{false};
    for (const prepared_entry& entry : entries_)
    {
        changed = changed || entry.before_parent_path != entry.after_parent_path || entry.before_position != *entry.after_position;
    }
    command_effect effect;
    for (std::size_t index{0}; index < entries_.size(); ++index)
    {
        const auto it{std::ranges::find(resolved, index, &resolved_entry::index)};
        assert(it != resolved.end());
        append_unique(effect.updated, std::span{&it->source, 1});
    }
    return success(std::move(effect), changed);
}

set_name_command::set_name_command(nori::resource::handle document_root, nori::resource::handle target, std::string new_name) :
    document_root_{document_root},
    target_{target},
    parent_path_{},
    old_name_{},
    new_name_{std::move(new_name)}
{
}

command_execution set_name_command::execute()
{
    if (target_)
    {
        const auto path{relative_property_path(document_root_, target_)};
        const auto parent_path{relative_property_path(document_root_, nori::resource::parent_of(target_))};
        if (!path || path->empty() || !parent_path)
        {
            return failure(!path ? path.error() : (!parent_path ? parent_path.error() : nori::resource::error_code::invalid_relationship));
        }
        parent_path_ = *parent_path;
        old_name_ = nori::resource::name_of(target_);
        target_ = {};
    }
    return set_name(old_name_, new_name_);
}

command_execution set_name_command::undo()
{
    return set_name(new_name_, old_name_);
}

std::string set_name_command::description() const
{
    if (old_name_.empty())
    {
        return "이름 변경";
    }
    return std::format("이름 변경: '{}' -> '{}'", display_path(append_property_path(parent_path_, old_name_)), new_name_);
}

command_execution set_name_command::set_name(std::string_view source, std::string_view destination)
{
    if (!nori::resource::is_valid_property_name(destination))
    {
        return failure(nori::resource::error_code::invalid_path);
    }
    if (source == destination)
    {
        return success({}, false);
    }
    const auto target{resolve_property(document_root_, append_property_path(parent_path_, source))};
    if (!target)
    {
        return failure(target.error());
    }
    const auto result{nori::resource::rename(*target, destination)};
    if (!result)
    {
        return failure(result.error());
    }
    return success({.updated = {*target}}, true);
}

set_type_command::set_type_command(nori::resource::handle document_root, std::vector<nori::resource::handle> targets, nori::resource::property_type after) :
    document_root_{document_root},
    targets_{std::move(targets)},
    entries_{},
    after_{default_property_snapshot(after)}
{
}

command_execution set_type_command::execute()
{
    if (!is_primitive_type(nori::resource::property_type{static_cast<std::uint8_t>(after_.index())}))
    {
        return failure(nori::resource::error_code::type_mismatch);
    }

    if (entries_.empty())
    {
        std::vector<entry> prepared;
        prepared.reserve(targets_.size());
        for (const nori::resource::handle target : targets_)
        {
            const auto path{relative_property_path(document_root_, target)};
            if (!path)
            {
                return failure(path.error());
            }
            const auto before{copy_property_snapshot(target)};
            if (!before)
            {
                return failure(before.error());
            }
            prepared.push_back({*path, *before});
        }
        entries_ = std::move(prepared);
        targets_.clear();
    }
    return apply_after();
}

command_execution set_type_command::undo()
{
    return apply_before();
}

std::string set_type_command::description() const
{
    if (entries_.size() == 1)
    {
        return std::format("타입 설정: '{}' = {}", display_path(entries_.front().path), display_property_snapshot(after_));
    }
    return std::format("{}개의 타입 설정", entries_.empty() ? targets_.size() : entries_.size());
}

command_execution set_type_command::apply_after()
{
    struct resolved_entry
    {
        nori::resource::handle target;
        property_snapshot current;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (const entry& item : entries_)
    {
        const auto target{resolve_property(document_root_, item.path)};
        if (!target)
        {
            return failure(target.error());
        }
        const auto current{copy_property_snapshot(*target)};
        if (!current)
        {
            return failure(current.error());
        }
        resolved.push_back({*target, *current});
    }

    const nori::resource::property_type after_type{static_cast<std::uint8_t>(after_.index())};
    command_effect effect;
    for (const resolved_entry& entry : resolved)
    {
        if (nori::resource::property_type{static_cast<std::uint8_t>(entry.current.index())} != after_type)
        {
            set_property_snapshot(entry.target, after_);
            append_unique(effect.updated, std::span{&entry.target, 1});
        }
    }
    const bool changed{!effect.updated.empty()};
    return success(std::move(effect), changed);
}

command_execution set_type_command::apply_before()
{
    struct resolved_entry
    {
        nori::resource::handle target;
        property_snapshot current;
        const property_snapshot* before;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (const entry& item : entries_)
    {
        const auto target{resolve_property(document_root_, item.path)};
        if (!target)
        {
            return failure(target.error());
        }
        const auto current{copy_property_snapshot(*target)};
        if (!current)
        {
            return failure(current.error());
        }
        resolved.push_back({*target, *current, &item.before});
    }

    command_effect effect;
    for (const resolved_entry& entry : resolved)
    {
        if (entry.current != *entry.before)
        {
            set_property_snapshot(entry.target, *entry.before);
            append_unique(effect.updated, std::span{&entry.target, 1});
        }
    }
    const bool changed{!effect.updated.empty()};
    return success(std::move(effect), changed);
}

template <typename T>
set_value_command<T>::set_value_command(nori::resource::handle document_root, std::vector<nori::resource::handle> targets, T after) :
    document_root_{document_root},
    targets_{std::move(targets)},
    after_{std::move(after)}
{
}

template <typename T>
command_execution set_value_command<T>::execute()
{
    if (entries_.empty())
    {
        std::vector<entry> prepared;
        prepared.reserve(targets_.size());
        for (const nori::resource::handle target : targets_)
        {
            const auto path{relative_property_path(document_root_, target)};
            if (!path)
            {
                return failure(path.error());
            }
            const auto before{nori::resource::try_get<T>(target)};
            if (!before)
            {
                return failure(before.error());
            }
            prepared.push_back({*path, *before});
        }
        entries_ = std::move(prepared);
        targets_.clear();
    }
    return apply_after();
}

template <typename T>
command_execution set_value_command<T>::undo()
{
    return apply_before();
}

template <typename T>
std::string set_value_command<T>::description() const
{
    if (entries_.size() == 1)
    {
        return std::format("값 설정: '{}' = {}", display_path(entries_.front().path), display_value(after_));
    }
    return std::format("{}개의 값 설정", entries_.empty() ? targets_.size() : entries_.size());
}

template <typename T>
bool set_value_command<T>::merge(const command& newer)
{
    if constexpr (
        !std::same_as<T, std::int32_t> &&
        !std::same_as<T, std::int64_t> &&
        !std::same_as<T, float> &&
        !std::same_as<T, std::string>
    )
    {
        return false;
    }
    else
    {
        const auto value{dynamic_cast<const set_value_command*>(&newer)};
        if (!value || entries_.size() != value->entries_.size())
        {
            return false;
        }
        for (std::size_t index{0}; index < entries_.size(); ++index)
        {
            if (entries_[index].path != value->entries_[index].path)
            {
                return false;
            }
        }
        after_ = value->after_;
        return true;
    }
}

template <typename T>
bool set_value_command<T>::net_noop() const
{
    return std::ranges::all_of(entries_, [this](const entry& value)
    {
        return value.before == after_;
    });
}

template <typename T>
command_execution set_value_command<T>::apply_after()
{
    struct resolved_entry
    {
        nori::resource::handle target;
        T current;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (const entry& item : entries_)
    {
        const auto target{resolve_property(document_root_, item.path)};
        if (!target)
        {
            return failure(target.error());
        }
        const auto current{nori::resource::try_get<T>(*target)};
        if (!current)
        {
            return failure(current.error());
        }
        resolved.push_back({*target, *current});
    }

    command_effect effect;
    for (const resolved_entry& entry : resolved)
    {
        if (entry.current != after_)
        {
            nori::resource::set(entry.target, after_);
            append_unique(effect.updated, std::span{&entry.target, 1});
        }
    }
    const bool changed{!effect.updated.empty()};
    return success(std::move(effect), changed);
}

template <typename T>
command_execution set_value_command<T>::apply_before()
{
    struct resolved_entry
    {
        nori::resource::handle target;
        T current;
        const T* before;
    };

    std::vector<resolved_entry> resolved;
    resolved.reserve(entries_.size());
    for (const entry& item : entries_)
    {
        const auto target{resolve_property(document_root_, item.path)};
        if (!target)
        {
            return failure(target.error());
        }
        const auto current{nori::resource::try_get<T>(*target)};
        if (!current)
        {
            return failure(current.error());
        }
        resolved.push_back({*target, *current, &item.before});
    }

    command_effect effect;
    for (const resolved_entry& entry : resolved)
    {
        if (entry.current != *entry.before)
        {
            nori::resource::set(entry.target, *entry.before);
            append_unique(effect.updated, std::span{&entry.target, 1});
        }
    }
    const bool changed{!effect.updated.empty()};
    return success(std::move(effect), changed);
}

command_manager::command_manager() :
    current_revision_{0},
    next_revision_{1},
    merge_allowed_{false}
{
}

command_result command_manager::execute(std::unique_ptr<command> new_command)
{
    command_execution execution{new_command->execute()};
    std::string description{new_command->description()};
    if (!execution.result || !execution.changed)
    {
        return {std::move(description), std::move(execution)};
    }

    redo_stack_.clear();
    if (merge_allowed_ && !undo_stack_.empty() && undo_stack_.back().command->merge(*new_command))
    {
        if (undo_stack_.back().command->net_noop())
        {
            current_revision_ = undo_stack_.back().before_revision;
            undo_stack_.pop_back();
            merge_allowed_ = false;
        }
        else
        {
            const revision next{next_revision_++};
            undo_stack_.back().after_revision = next;
            current_revision_ = next;
            merge_allowed_ = true;
        }
        return {std::move(description), std::move(execution)};
    }

    const revision next{next_revision_++};
    undo_stack_.push_back({std::move(new_command), current_revision_, next});
    if (undo_stack_.size() > max_size)
    {
        undo_stack_.pop_front();
    }
    current_revision_ = next;
    merge_allowed_ = true;
    return {std::move(description), std::move(execution)};
}

command_result command_manager::undo()
{
    if (undo_stack_.empty())
    {
        return {{}, failure(nori::resource::error_code::not_found)};
    }
    history_entry& entry{undo_stack_.back()};
    std::string description{entry.command->description()};
    command_execution execution{entry.command->undo()};
    if (!execution.result)
    {
        return {std::move(description), std::move(execution)};
    }
    if (!execution.changed)
    {
        nori::core::abort("successful history undo must change state");
    }
    current_revision_ = entry.before_revision;
    redo_stack_.push_back(std::move(entry));
    undo_stack_.pop_back();
    break_merge();
    return {std::move(description), std::move(execution)};
}

command_result command_manager::redo()
{
    if (redo_stack_.empty())
    {
        return {{}, failure(nori::resource::error_code::not_found)};
    }
    history_entry& entry{redo_stack_.back()};
    std::string description{entry.command->description()};
    command_execution execution{entry.command->execute()};
    if (!execution.result)
    {
        return {std::move(description), std::move(execution)};
    }
    if (!execution.changed)
    {
        nori::core::abort("successful history redo must change state");
    }
    current_revision_ = entry.after_revision;
    undo_stack_.push_back(std::move(entry));
    redo_stack_.pop_back();
    break_merge();
    return {std::move(description), std::move(execution)};
}

bool command_manager::can_undo() const
{
    return !undo_stack_.empty();
}

bool command_manager::can_redo() const
{
    return !redo_stack_.empty();
}

bool command_manager::dirty() const
{
    return !saved_revision_ || current_revision_ != *saved_revision_;
}

void command_manager::mark_saved()
{
    saved_revision_ = current_revision_;
    break_merge();
}

void command_manager::break_merge()
{
    merge_allowed_ = false;
}

void command_manager::clear()
{
    undo_stack_.clear();
    redo_stack_.clear();
    current_revision_ = 0;
    saved_revision_.reset();
    next_revision_ = 1;
    merge_allowed_ = false;
}

template class create_command<std::monostate>;
template class create_command<bool>;
template class create_command<std::int32_t>;
template class create_command<std::int64_t>;
template class create_command<float>;
template class create_command<std::string>;
template class create_command<nori::resource::image>;
template class create_command<nori::resource::binary>;

template class set_value_command<bool>;
template class set_value_command<std::int32_t>;
template class set_value_command<std::int64_t>;
template class set_value_command<float>;
template class set_value_command<std::string>;
template class set_value_command<nori::resource::image>;
template class set_value_command<nori::resource::binary>;
} // namespace nori::editor
