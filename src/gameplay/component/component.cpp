#include <nori/core/abort.hpp>
#include <nori/gameplay/component/component.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace nori::gameplay
{
component::component(object& owner) :
    owner_{owner}
{
}

object& component::owner()
{
    return owner_;
}

const object& component::owner() const
{
    return owner_;
}

bool component::is_registered() const noexcept
{
    return registered_;
}

void component::on_register()
{
}

void component::on_unregister()
{
}

void component::register_component()
{
    if (registered_ || !owner_.is_registered() || !owner_.contains_component(*this))
    {
        core::abort("component registration requires an attached owner and owned component");
    }

    update_scheduler& scheduler{owner_.world()->scheduler()};
    registered_ = true;
    scheduler.execute_lifecycle([this]
    {
        owner_.component_lifecycle_active_ = true;
        on_register();
        owner_.component_lifecycle_active_ = false;
    });
}

void component::unregister_component()
{
    if (!registered_)
    {
        return;
    }

    update_scheduler& scheduler{owner_.world()->scheduler()};
    if (scheduler.is_executing() || (owner_.is_registered() && owner_.contains_component(*this)))
    {
        core::abort("component unregistration requires removal at a lifecycle boundary");
    }

    registered_ = false;
    scheduler.remove_all(*this);
    scheduler.execute_lifecycle([this]
    {
        owner_.component_lifecycle_active_ = true;
        on_unregister();
        owner_.component_lifecycle_active_ = false;
    });
}

update_scheduler* component::active_scheduler() const
{
    return registered_ && owner_.is_registered() ? &owner_.world()->scheduler() : nullptr;
}

void component::on_component_removed(const component&) noexcept
{
}

bool component::unregister_update(update_handle handle)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->remove(handle);
}

bool component::set_update_enabled(update_handle handle, bool enabled)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->set_update_enabled(handle, enabled);
}

bool component::is_update_enabled(update_handle handle) const
{
    const auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, handle) && scheduler->is_update_enabled(handle);
}

bool component::add_update_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, target) && scheduler->add_prerequisite(target, prerequisite);
}

bool component::remove_update_prerequisite(update_handle target, update_handle prerequisite)
{
    auto* scheduler{active_scheduler()};
    return scheduler && scheduler->owns_update(*this, target) && scheduler->remove_prerequisite(target, prerequisite);
}

update_scheduler& component::registration_scheduler()
{
    auto* scheduler{active_scheduler()};
    if (!scheduler)
    {
        core::abort("update registration requires an active owner");
    }
    return *scheduler;
}
} // namespace nori::gameplay
