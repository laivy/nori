#include <cmath>

#include <nori/core/abort.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object.hpp>

namespace
{
template <typename value_type>
value_type select_transform(nori::gameplay::attachment_rule rule, value_type relative, value_type world, value_type identity)
{
    switch (rule)
    {
    case nori::gameplay::attachment_rule::keep_relative:
    {
        return relative;
    }
    case nori::gameplay::attachment_rule::keep_world:
    {
        return world;
    }
    default:
        return identity;
    }
}
} // namespace

namespace nori::gameplay
{
scene_component::scene_component(object& owner, core::transform pose) :
    rtti{owner},
    relative_{core::transform::identity}
{
    if (!set_relative_transform(pose))
    {
        core::abort("invalid scene component transform");
    }
}

const core::transform& scene_component::relative_transform() const
{
    return relative_;
}

core::transform scene_component::world_transform() const
{
    return parent_ ? compose_transform(parent_->world_transform(), relative_) : relative_;
}

const scene_component* scene_component::parent() const
{
    return parent_;
}

bool scene_component::set_relative_transform(core::transform pose)
{
    const auto& scale{pose.scale};
    const auto& r{pose.rotation};
    const auto norm{r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w};
    if (!std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) || !std::isfinite(pose.position.z) || !std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z) || scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f || !(std::abs(norm - 1.0f) < 0.001f))
    {
        return false;
    }
    relative_ = pose;
    return true;
}

bool scene_component::attach_to_component(scene_component& parent, attachment_rules rules)
{
    if (&owner() != &parent.owner() || this == &owner().root_component() || !owner().contains_component(*this) || !owner().contains_component(parent))
    {
        return false;
    }
    for (const auto* ancestor{&parent}; ancestor; ancestor = ancestor->parent_)
    {
        if (ancestor == this)
        {
            return false;
        }
    }
    if (parent_ == &parent)
    {
        return true;
    }
    const auto relative_world{relative_to(world_transform(), parent.world_transform())};
    if (!set_relative_transform({select_transform(rules.position, relative_.position, relative_world.position, core::transform::identity.position), select_transform(rules.rotation, relative_.rotation, relative_world.rotation, core::transform::identity.rotation), select_transform(rules.scale, relative_.scale, relative_world.scale, core::transform::identity.scale)}))
    {
        return false;
    }
    parent_ = &parent;
    return true;
}

bool scene_component::detach_from_component(detachment_rule rule)
{
    if (rule == detachment_rule::keep_world && !set_relative_transform(world_transform()))
    {
        return false;
    }
    parent_ = nullptr;
    return true;
}
} // namespace nori::gameplay
