#include <algorithm>
#include <atomic>
#include <functional>
#include <queue>
#include <utility>

#include <nori/core/abort.hpp>

#include <nori/gameplay/update/update_scheduler.hpp>

namespace
{
std::atomic<std::uint64_t> next_scheduler_id{1};

nori::gameplay::update_category category_of(nori::gameplay::update_group group)
{
    switch (group)
    {
    case nori::gameplay::update_group::update:
    case nori::gameplay::update_group::late_update:
        return nori::gameplay::update_category::variable;
    case nori::gameplay::update_group::input:
    case nori::gameplay::update_group::fixed_commands:
    case nori::gameplay::update_group::pre_physics:
    case nori::gameplay::update_group::physics:
    case nori::gameplay::update_group::post_physics:
    case nori::gameplay::update_group::post_fixed_update:
        return nori::gameplay::update_category::fixed;
    }
    nori::core::abort("invalid update group");
}
} // namespace

namespace nori::gameplay
{
update_scheduler::update_scheduler() :
    id_{next_scheduler_id.fetch_add(1, std::memory_order_relaxed)}
{
    if (id_ == 0)
    {
        core::abort("update scheduler identifier exhausted");
    }
}

update_scheduler::function_map& update_scheduler::edit_functions()
{
    if (!executing_)
    {
        return functions_;
    }
    if (!pending_functions_)
    {
        pending_functions_.emplace(functions_);
    }
    return *pending_functions_;
}

void update_scheduler::commit_changes()
{
    if (pending_functions_)
    {
        functions_.swap(*pending_functions_);
        pending_functions_.reset();
    }
}

update_handle update_scheduler::insert(update_function function)
{
    if (next_id_ == 0)
    {
        core::abort("update identifier exhausted");
    }
    static_cast<void>(category_of(function.group));
    const auto id{next_id_++};
    edit_functions().emplace(id, std::move(function));
    return {id_, id};
}

const update_scheduler::update_function* update_scheduler::find(update_handle handle) const
{
    if (handle.scheduler != id_)
    {
        return nullptr;
    }
    const auto it{functions_.find(handle.id)};
    return it == functions_.end() ? nullptr : &it->second;
}

update_scheduler::update_function* update_scheduler::find(update_handle handle)
{
    if (handle.scheduler != id_)
    {
        return nullptr;
    }
    auto& functions{edit_functions()};
    const auto it{functions.find(handle.id)};
    return it == functions.end() ? nullptr : &it->second;
}

bool update_scheduler::owns_update_for_owner(const void* owner, update_handle handle) const
{
    if (handle.scheduler != id_)
    {
        return false;
    }
    const auto matches = [&](const function_map& functions)
    {
        const auto it{functions.find(handle.id)};
        return it != functions.end() && it->second.owner == owner;
    };
    return matches(functions_) || (pending_functions_ && matches(*pending_functions_));
}

std::size_t update_scheduler::unregister_updates_for_owner(const void* owner)
{
    auto& functions{edit_functions()};
    std::vector<std::uint64_t> removed;
    for (auto it{functions.begin()}; it != functions.end();)
    {
        if (it->second.owner == owner)
        {
            removed.push_back(it->first);
            it = functions.erase(it);
        }
        else
        {
            ++it;
        }
    }
    if (!removed.empty())
    {
        for (auto& [id, function] : functions)
        {
            std::erase_if(function.prerequisites, [&](std::uint64_t prerequisite)
            {
                return std::ranges::binary_search(removed, prerequisite);
            });
        }
    }
    return removed.size();
}

void update_scheduler::execute_lifecycle(std::move_only_function<void()> function)
{
    ++lifecycle_depth_;

    struct lifecycle_scope
    {
        std::size_t& depth;

        ~lifecycle_scope()
        {
            --depth;
        }
    } scope{lifecycle_depth_};

    function();
}

bool update_scheduler::unregister_update(update_handle handle)
{
    if (!find(handle))
    {
        return false;
    }
    auto& functions{edit_functions()};
    functions.erase(handle.id);
    for (auto& [id, function] : functions)
    {
        std::erase(function.prerequisites, handle.id);
    }
    return true;
}

bool update_scheduler::set_update_enabled(update_handle handle, bool enabled)
{
    if (auto* function{find(handle)})
    {
        function->enabled = enabled;
        return true;
    }
    return false;
}

bool update_scheduler::is_update_enabled(update_handle handle) const
{
    const auto* function{find(handle)};
    return function && function->enabled;
}

bool update_scheduler::add_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* function{find(target)};
    const auto* dependency{find(prerequisite)};
    if (!function || !dependency || target == prerequisite || category_of(dependency->group) != category_of(function->group) || dependency->group > function->group || std::ranges::find(function->prerequisites, prerequisite.id) != function->prerequisites.end())
    {
        return false;
    }
    function->prerequisites.push_back(prerequisite.id);
    const auto& functions{edit_functions()};
    const auto category{category_of(function->group)};
    const auto count{std::ranges::count_if(functions, [category](const auto& entry)
    {
        return category_of(entry.second.group) == category;
    })};
    if (execution_order(functions, category).size() != static_cast<std::size_t>(count))
    {
        function->prerequisites.pop_back();
        return false;
    }
    return true;
}

bool update_scheduler::remove_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* function{find(target)};
    if (!function || !find(prerequisite))
    {
        return false;
    }
    return std::erase(function->prerequisites, prerequisite.id) != 0;
}

std::vector<std::uint64_t> update_scheduler::execution_order(const function_map& functions, update_category category)
{
    std::map<std::uint64_t, std::size_t> incoming;
    std::map<std::uint64_t, std::vector<std::uint64_t>> dependents;
    using ready_key = std::pair<update_group, std::uint64_t>;
    std::priority_queue<ready_key, std::vector<ready_key>, std::greater<ready_key>> ready;

    for (const auto& [id, function] : functions)
    {
        if (category_of(function.group) != category)
        {
            continue;
        }
        incoming.emplace(id, function.prerequisites.size());
        for (const auto prerequisite : function.prerequisites)
        {
            dependents[prerequisite].push_back(id);
        }
        if (function.prerequisites.empty())
        {
            ready.emplace(function.group, id);
        }
    }

    std::vector<std::uint64_t> order;
    order.reserve(incoming.size());
    while (!ready.empty())
    {
        const auto id{ready.top().second};
        ready.pop();
        order.push_back(id);
        for (const auto dependent : dependents[id])
        {
            if (--incoming.at(dependent) == 0)
            {
                ready.emplace(functions.at(dependent).group, dependent);
            }
        }
    }
    return order;
}

void update_scheduler::execute(update_category category, float delta_seconds)
{
    if (is_executing())
    {
        core::abort("recursive update or lifecycle execution");
    }
    if (category != update_category::variable && category != update_category::fixed)
    {
        core::abort("invalid update category");
    }
    const auto order{execution_order(functions_, category)};
    executing_ = true;

    struct execution_scope
    {
        update_scheduler& scheduler;

        ~execution_scope()
        {
            scheduler.executing_ = false;
            scheduler.commit_changes();
        }
    } scope{*this};

    for (const auto id : order)
    {
        const auto& function{functions_.at(id)};
        if (function.enabled)
        {
            (*function.callback)(delta_seconds);
        }
    }
}

bool update_scheduler::is_update_in_snapshot(update_handle handle) const
{
    return executing_ && find(handle) != nullptr;
}

bool update_scheduler::is_executing() const noexcept
{
    return executing_ || lifecycle_depth_ != 0;
}
} // namespace nori::gameplay
