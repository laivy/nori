#include <cmath>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/physics_body_component.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace
{
nori::physics2d::body_transform body_pose(const nori::core::transform& pose)
{
    if (pose.scale.x != 1.0f || pose.scale.y != 1.0f || pose.rotation.x != 0.0f || pose.rotation.y != 0.0f)
    {
        nori::core::abort("physics body requires a planar root pose with unit XY scale");
    }
    return {{pose.position.x, pose.position.y}, 2.0f * std::atan2(pose.rotation.z, pose.rotation.w)};
}
} // namespace

namespace nori::gameplay
{
physics_body_component::physics_body_component(object& owner, physics_body_config config) :
    rtti{owner},
    config_{std::move(config)}
{
}

physics2d::body_handle physics_body_component::body_handle() const
{
    return body_;
}

update_handle physics_body_component::pre_physics_update() const
{
    return pre_physics_update_;
}

std::optional<physics2d::body_velocity> physics_body_component::velocity() const
{
    return is_registered() ? owner().world().physics_scene().get_velocity(body_) : std::nullopt;
}

bool physics_body_component::set_velocity(physics2d::body_velocity value)
{
    return is_registered() && owner().world().physics_scene().set_velocity(body_, value);
}

std::vector<physics2d::contact_point> physics_body_component::get_contacts() const
{
    return is_registered() && !teleported_ && !pending_teleport_ ? owner().world().physics_scene().get_contacts(body_) : std::vector<physics2d::contact_point>{};
}

bool physics_body_component::teleport(physics2d::body_transform pose, bool preserve_velocity)
{
    if (!is_registered() || !std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) || !std::isfinite(pose.angle_radians))
    {
        return false;
    }
    pending_teleport_ = teleport_request{pose, preserve_velocity};
    return true;
}

void physics_body_component::on_register()
{
    for (const auto other : owner().find_components<physics_body_component>())
    {
        if (other != this && other->is_registered())
        {
            core::abort("only one physics body can drive an object's root");
        }
    }
    auto& active{owner().world()};
    const auto pose{body_pose(owner().root_component().relative_transform())};
    body_ = active.physics_scene().create_body({config_.type, pose, config_.velocity,
                                                   config_.lock_rotation, config_.mass_override},
        config_.colliders);
    if (!active.physics_scene().contains(body_))
    {
        core::abort("failed to create physics body");
    }
    active.register_body(body_, owner().handle());
    if (config_.type == physics2d::body_type::static_body)
    {
        static_pose_ = pose;
    }
    pre_physics_update_ = register_update(update_group::pre_physics, &physics_body_component::on_pre_physics_update);
    register_update(update_group::post_physics, &physics_body_component::on_post_physics_update);
}

void physics_body_component::on_unregister()
{
    auto& active{owner().world()};
    active.physics_scene().destroy_body(body_);
    active.unregister_body(body_);
    body_ = {};
    pre_physics_update_ = {};
    pending_teleport_.reset();
    teleported_ = false;
}

void physics_body_component::on_pre_physics_update(float)
{
    const auto pose{body_pose(owner().root_component().relative_transform())};
    if (pending_teleport_)
    {
        apply_pending_teleport();
    }
    else if (config_.type == physics2d::body_type::static_body && !teleported_ && (static_pose_.position.x != pose.position.x || static_pose_.position.y != pose.position.y || static_pose_.angle_radians != pose.angle_radians))
    {
        auto& physics_scene{owner().world().physics_scene()};
        if (physics_scene.set_transform(body_, pose))
        {
            static_pose_ = pose;
        }
    }
}

void physics_body_component::on_post_physics_update(float)
{
    if (config_.type != physics2d::body_type::static_body || teleported_)
    {
        auto& root{owner().root_component()};
        const auto body_transform{config_.type == physics2d::body_type::static_body ? std::optional{static_pose_} : owner().world().physics_scene().get_transform(body_)};
        if (body_transform)
        {
            auto pose{root.relative_transform()};
            pose.position.x = body_transform->position.x;
            pose.position.y = body_transform->position.y;
            pose.rotation = core::axis_rotation({0.0f, 0.0f, 1.0f}, body_transform->angle_radians);
            root.set_relative_transform(pose);
            if (config_.type == physics2d::body_type::static_body)
            {
                static_pose_ = body_pose(pose);
            }
        }
    }
    teleported_ = false;
}

void physics_body_component::apply_pending_teleport()
{
    if (!pending_teleport_)
    {
        return;
    }
    const auto request{*pending_teleport_};
    pending_teleport_.reset();
    auto& physics_scene{owner().world().physics_scene()};
    teleported_ = physics_scene.set_transform(body_, request.pose);
    if (teleported_ && config_.type == physics2d::body_type::static_body)
    {
        static_pose_ = request.pose;
    }
    else if (teleported_ && !request.preserve_velocity)
    {
        physics_scene.set_velocity(body_, {});
    }
}
} // namespace nori::gameplay
