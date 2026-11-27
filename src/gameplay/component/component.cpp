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

void component::on_fixed_update(float)
{
}

void component::on_update(float)
{
}

void component::on_post_update(float)
{
}

void component::on_component_removed(const component&) noexcept
{
}

void component::register_component()
{
    if (registered_ || !owner_.is_registered() || !owner_.contains_component(*this))
    {
        core::abort("component registration requires an attached owner and owned component");
    }

    auto& owner_world{*owner_.world()};
    registered_ = true;
    owner_world.execute_guarded([this]()
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

    auto& owner_world{*owner_.world()};
    if (owner_world.is_executing() || (owner_.is_registered() && owner_.contains_component(*this)))
    {
        core::abort("component unregistration requires removal at a lifecycle boundary");
    }

    registered_ = false;
    owner_world.execute_guarded([this]()
    {
        owner_.component_lifecycle_active_ = true;
        on_unregister();
        owner_.component_lifecycle_active_ = false;
    });
}
} // namespace nori::gameplay
