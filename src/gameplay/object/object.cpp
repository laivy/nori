#include <algorithm>
#include <cassert>

#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace nori::gameplay
{
object::object(transform pose) :
    root_{&add_component<scene_component>(pose)}
{
}

::nori::gameplay::world& object::world()
{
    assert(world_);
    return *world_;
}

const ::nori::gameplay::world& object::world() const
{
    assert(world_);
    return *world_;
}

object_handle object::handle() const
{
    return handle_;
}

update_scheduler& object::world_scheduler()
{
    return world().scheduler();
}

bool object::is_registered() const noexcept
{
    return world_ && !unregistering_components_;
}

update_scheduler* object::active_scheduler() const
{
    return is_registered() ? &world_->scheduler() : nullptr;
}

bool object::unregister_update(update_handle handle)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->unregister_update(handle);
}

bool object::set_update_enabled(update_handle handle, bool enabled)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->set_update_enabled(handle, enabled);
}

bool object::is_update_enabled(update_handle handle) const
{
    const auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->is_update_enabled(handle);
}

bool object::add_update_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, target) && scheduler->add_prerequisite(target, prerequisite);
}

bool object::remove_update_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, target) && scheduler->remove_prerequisite(target, prerequisite);
}

update_scheduler& object::registration_scheduler()
{
    auto* scheduler{active_scheduler()};
    if (!scheduler)
    {
        core::abort("update registration requires an active owner");
    }
    return *scheduler;
}

void object::attach_to_world(::nori::gameplay::world& active, object_handle handle)
{
    if (world_ || active.find_object(handle) != this)
    {
        core::abort("object registration requires an owning world");
    }
    world_ = &active;
    handle_ = handle;
    const auto initial{find_components<component>()};
    for (auto* value : initial)
    {
        if (!world_)
        {
            break;
        }
        if (contains_component(*value) && !value->is_registered())
        {
            value->register_component();
        }
    }
    world_scheduler().execute_lifecycle([this]
    {
        on_register();
    });
}

void object::detach_from_world()
{
    if (!world_ || unregistering_components_)
    {
        return;
    }
    auto& scheduler{world_scheduler()};
    if (scheduler.is_executing() || world_->find_object(handle_) == this)
    {
        core::abort("object unregistration requires removal at a lifecycle boundary");
    }
    unregistering_components_ = true;
    scheduler.unregister_updates(*this);
    scheduler.execute_lifecycle([this]
    {
        on_unregister();
    });
    for (auto* value : find_components<component>())
    {
        value->unregister_component();
    }
    world_ = nullptr;
    handle_ = {};
    unregistering_components_ = false;
}

scene_component& object::root_component()
{
    return *root_;
}

const scene_component& object::root_component() const
{
    return *root_;
}

bool object::remove_component(component& value)
{
    if (notifying_component_removal_ || &value == root_ || !contains_component(value) || std::ranges::find(pending_component_removals_, &value) != pending_component_removals_.end())
    {
        return false;
    }
    if (world_ && world_scheduler().is_executing())
    {
        pending_component_removals_.push_back(&value);
        return true;
    }
    remove_component_now(value);
    return true;
}

bool object::contains_component(const component& value) const
{
    return std::ranges::find(components_, &value, &std::unique_ptr<component>::get) != components_.end();
}

void object::finish_update()
{
    if (notifying_component_removal_ || (world_ && world_scheduler().is_executing()))
    {
        core::abort("cannot finish object update during execution");
    }
    while (!pending_component_removals_.empty())
    {
        const auto value{pending_component_removals_.front()};
        pending_component_removals_.pop_front();
        remove_component_now(*value);
    }
    retired_components_.clear();
}

void object::remove_component_now(component& value)
{
    if (const auto parent{core::cast<scene_component>(&value)})
    {
        for (auto* child : find_components<scene_component>())
        {
            if (child->parent() == parent && !child->attach_to_component(*root_, keep_world_transform))
            {
                core::abort("cannot reparent scene component");
            }
        }
    }
    const auto entry{std::ranges::find(components_, &value, &std::unique_ptr<component>::get)};
    assert(entry != components_.end());
    auto retired{std::move(*entry)};
    components_.erase(entry);
    notify_component_removed(*retired);
    retired->unregister_component();
    retired_components_.push_back(std::move(retired));
}

void object::notify_component_removed(const component& value)
{
    const auto notify = [this, &value]() noexcept
    {
        notifying_component_removal_ = true;
        for (const auto& entry : components_)
        {
            entry->on_component_removed(value);
        }
        notifying_component_removal_ = false;
    };
    if (world_)
    {
        world_scheduler().execute_lifecycle(notify);
    }
    else
    {
        notify();
    }
}
} // namespace nori::gameplay
