#include <cmath>

#include <nori/core/transform.hpp>
#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object.hpp>

namespace
{
template <typename ValueType>
ValueType select_transform(nori::gameplay::attachment_rule rule, ValueType relative, ValueType world, ValueType identity)
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

bool is_valid_transform(const nori::core::transform& pose)
{
    const nori::core::vec3& position{pose.position};
    const nori::core::vec3& scale{pose.scale};
    const nori::core::vec4& rotation{pose.rotation};
    const float norm{rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w};
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
    {
        return false;
    }
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z))
    {
        return false;
    }
    if (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f)
    {
        return false;
    }
    return std::abs(norm - 1.0f) < 0.001f;
}
} // namespace

namespace nori::gameplay
{
scene_component::scene_component(object& owner, core::transform pose) :
    rtti{owner},
    relative_{pose},
    parent_{nullptr}
{
}

const core::transform& scene_component::relative_transform() const
{
    return relative_;
}

core::transform scene_component::world_transform() const
{
    return parent_ ? core::compose_transform(parent_->world_transform(), relative_) : relative_;
}

const scene_component* scene_component::parent() const
{
    return parent_;
}

void scene_component::set_relative_transform(core::transform pose)
{
    relative_ = pose;
}

bool scene_component::attach_to_component(scene_component& parent, attachment_rules rules)
{
    const object& obj{owner()};
    if (&obj != &parent.owner())
    {
        return false;
    }
    if (this == &obj.root_component())
    {
        return false;
    }
    if (!obj.contains_component(*this) || !obj.contains_component(parent))
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
    const auto pose{compute_attachment_transform(parent, rules)};
    if (!pose)
    {
        return false;
    }
    set_relative_transform(*pose);
    parent_ = &parent;
    return true;
}

bool scene_component::detach_from_component(detachment_rule rule)
{
    if (rule == detachment_rule::keep_world)
    {
        const auto pose{world_transform()};
        if (!is_valid_transform(pose))
        {
            return false;
        }
        set_relative_transform(pose);
    }
    parent_ = nullptr;
    return true;
}

std::optional<core::transform> scene_component::compute_attachment_transform(const scene_component& parent, attachment_rules rules) const
{
    const auto relative_world{core::relative_to(world_transform(), parent.world_transform())};
    const core::transform pose{
        select_transform(rules.position, relative_.position, relative_world.position, core::transform::identity.position),
        select_transform(rules.rotation, relative_.rotation, relative_world.rotation, core::transform::identity.rotation),
        select_transform(rules.scale, relative_.scale, relative_world.scale, core::transform::identity.scale)
    };
    if (!is_valid_transform(pose))
    {
        return std::nullopt;
    }
    return pose;
}
} // namespace nori::gameplay
