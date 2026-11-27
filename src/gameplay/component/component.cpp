#include <nori/gameplay/component/component.hpp>
#include <nori/gameplay/object/object.hpp>

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
} // namespace nori::gameplay
