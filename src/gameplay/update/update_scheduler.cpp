#include <algorithm>
#include <atomic>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/gameplay/update/update_scheduler.hpp>

namespace
{
std::atomic<std::uint64_t> next_scheduler_id{1};

constexpr nori::gameplay::update_category category_of(nori::gameplay::update_group group)
{
    switch (group)
    {
    case nori::gameplay::update_group::update:
    case nori::gameplay::update_group::late_update:
    {
        return nori::gameplay::update_category::variable;
    }
    default:
        break;
    }
    return nori::gameplay::update_category::fixed;
}
} // namespace

namespace nori::gameplay
{
update_scheduler::update_scheduler() :
    id_{next_scheduler_id.fetch_add(1, std::memory_order_relaxed)},
    next_id_{1},
    functions_{},
    pending_functions_{},
    executing_{false},
    lifecycle_depth_{0}
{
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

update_handle update_scheduler::insert(update_function function)
{
    const auto id{next_id_++};
    edit_functions().emplace(id, std::move(function));
    return {id_, id};
}

const update_scheduler::function_map& update_scheduler::latest_functions() const
{
    return pending_functions_ ? *pending_functions_ : functions_;
}

const update_scheduler::update_function* update_scheduler::find(const function_map& functions, update_handle handle) const
{
    if (handle.scheduler != id_)
    {
        return nullptr;
    }
    const auto it{functions.find(handle.id)};
    return it == functions.end() ? nullptr : &it->second;
}

std::size_t update_scheduler::remove_all(const void* owner)
{
    std::vector<std::uint64_t> removed;
    for (const auto& [id, function] : latest_functions())
    {
        if (function.owner == owner)
        {
            removed.push_back(id);
        }
    }
    if (removed.empty())
    {
        return 0;
    }

    auto& functions{edit_functions()};
    for (const auto id : removed)
    {
        functions.erase(id);
    }
    for (auto& [_, function] : functions)
    {
        std::erase_if(function.prerequisites, [&](std::uint64_t prerequisite)
        {
            return std::ranges::binary_search(removed, prerequisite);
        });
    }
    return removed.size();
}

void update_scheduler::execute_lifecycle(std::move_only_function<void()> callback)
{
    ++lifecycle_depth_;

    struct lifecycle_scope
    {
        std::size_t& depth;

        ~lifecycle_scope()
        {
            --depth;
        }
    };

    lifecycle_scope scope{lifecycle_depth_};
    callback();
}

bool update_scheduler::remove(update_handle handle)
{
    if (!find(latest_functions(), handle))
    {
        return false;
    }
    auto& functions{edit_functions()};
    functions.erase(handle.id);
    for (auto& [_, function] : functions)
    {
        std::erase(function.prerequisites, handle.id);
    }
    return true;
}

bool update_scheduler::set_enabled(update_handle handle, bool enabled)
{
    const auto function{find(latest_functions(), handle)};
    if (!function)
    {
        return false;
    }
    if (function->enabled != enabled)
    {
        edit_functions().at(handle.id).enabled = enabled;
    }
    return true;
}

bool update_scheduler::is_enabled(update_handle handle) const
{
    const auto function{find(functions_, handle)};
    return function && function->enabled;
}

bool update_scheduler::add_prerequisite(update_handle target, update_handle prerequisite)
{
    const auto& latest{latest_functions()};
    const auto function{find(latest, target)};
    const auto dependency{find(latest, prerequisite)};
    if (!function || !dependency || target == prerequisite)
    {
        return false;
    }
    if (category_of(dependency->group) != category_of(function->group) || dependency->group > function->group)
    {
        return false;
    }
    if (std::ranges::contains(function->prerequisites, prerequisite.id))
    {
        return false;
    }
    const auto category{category_of(function->group)};
    auto& functions{edit_functions()};
    auto& prerequisites{functions.at(target.id).prerequisites};
    prerequisites.push_back(prerequisite.id);
    const auto count{std::ranges::count_if(functions, [category](const auto& entry)
    {
        return category_of(entry.second.group) == category;
    })};
    if (execution_order(functions, category).size() != static_cast<std::size_t>(count))
    {
        prerequisites.pop_back();
        return false;
    }
    return true;
}

bool update_scheduler::remove_prerequisite(update_handle target, update_handle prerequisite)
{
    const auto& latest{latest_functions()};
    const auto function{find(latest, target)};
    if (!function || !find(latest, prerequisite) || !std::ranges::contains(function->prerequisites, prerequisite.id))
    {
        return false;
    }
    std::erase(edit_functions().at(target.id).prerequisites, prerequisite.id);
    return true;
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
    const auto order{execution_order(functions_, category)};
    executing_ = true;

    struct execution_scope
    {
        update_scheduler& scheduler;

        ~execution_scope()
        {
            scheduler.executing_ = false;
            if (scheduler.pending_functions_)
            {
                scheduler.functions_.swap(*scheduler.pending_functions_);
                scheduler.pending_functions_.reset();
            }
        }
    } scope{*this};

    for (const auto id : order)
    {
        const auto& function{functions_.at(id)};
        if (function.enabled)
        {
            function.callback(delta_seconds);
        }
    }
}

bool update_scheduler::is_executing() const noexcept
{
    return executing_ || lifecycle_depth_ != 0;
}
} // namespace nori::gameplay
