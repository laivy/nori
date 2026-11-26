#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <box2d/box2d.h>

#include <nori/core/handle.hpp>
#include <nori/core/slot_map.hpp>
#include <nori/core/vector.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/scene.hpp>

namespace
{
std::atomic<std::uint64_t> next_scene_id{1};

bool is_finite(nori::core::vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool is_nonnegative(float value)
{
    return std::isfinite(value) && value >= 0.0f;
}
} // namespace

namespace nori::physics2d
{
class scene::impl
{
public:
    struct body_entry
    {
        b2BodyId id;
        std::optional<float> mass_override;
        std::vector<std::uint64_t> shapes;
    };

    using contact_key = std::tuple<contact_kind, std::uint64_t, std::uint64_t>;

    static contact_key make_contact_key(contact_kind kind, b2ShapeId first, b2ShapeId second)
    {
        auto first_id{b2StoreShapeId(first)};
        auto second_id{b2StoreShapeId(second)};
        if (kind == contact_kind::contact && first_id > second_id)
        {
            std::swap(first_id, second_id);
        }
        return {kind, first_id, second_id};
    }

    void begin_contact(const contact_key& key)
    {
        const auto& [kind, first_id, second_id]{key};
        const auto first{colliders.find(first_id)};
        const auto second{colliders.find(second_id)};
        if (first == colliders.end() || second == colliders.end())
        {
            return;
        }
        const contact_event event{first->second, second->second, kind, contact_phase::begin};
        if (active_contacts.emplace(key, event).second)
        {
            events.push_back(event);
        }
    }

    bool end_contact(const contact_key& key)
    {
        const auto active{active_contacts.find(key)};
        if (active == active_contacts.end())
        {
            return false;
        }
        auto event{active->second};
        event.phase = contact_phase::end;
        events.push_back(event);
        active_contacts.erase(active);
        return true;
    }

    void collect_contact_events()
    {
        events.clear();
        events.swap(removed_contacts);
        const auto contacts{b2World_GetContactEvents(world)};
        const auto sensors{b2World_GetSensorEvents(world)};
        std::vector<contact_key> unmatched_ends;
        const auto collect_end = [&](contact_key key)
        {
            if (!end_contact(key))
            {
                unmatched_ends.push_back(key);
            }
        };
        for (int i{}; i < contacts.endCount; ++i)
        {
            const auto& event{contacts.endEvents[i]};
            collect_end(make_contact_key(contact_kind::contact, event.shapeIdA, event.shapeIdB));
        }
        for (int i{}; i < sensors.endCount; ++i)
        {
            const auto& event{sensors.endEvents[i]};
            collect_end(make_contact_key(contact_kind::trigger, event.sensorShapeId, event.visitorShapeId));
        }
        for (int i{}; i < contacts.beginCount; ++i)
        {
            const auto& event{contacts.beginEvents[i]};
            begin_contact(make_contact_key(contact_kind::contact, event.shapeIdA, event.shapeIdB));
        }
        for (int i{}; i < sensors.beginCount; ++i)
        {
            const auto& event{sensors.beginEvents[i]};
            begin_contact(make_contact_key(contact_kind::trigger, event.sensorShapeId, event.visitorShapeId));
        }
        for (const auto& key : unmatched_ends)
        {
            end_contact(key);
        }
    }

    void remove_colliders(body_handle body, const body_entry& entry)
    {
        for (auto it{active_contacts.begin()}; it != active_contacts.end();)
        {
            auto event{it->second};
            if (event.first.body == body || event.second.body == body)
            {
                event.phase = contact_phase::end;
                removed_contacts.push_back(event);
                it = active_contacts.erase(it);
            }
            else
            {
                ++it;
            }
        }
        for (const auto shape : entry.shapes)
        {
            colliders.erase(shape);
        }
    }

    static void apply_dynamic_mass(const body_entry& entry)
    {
        if (b2Body_GetType(entry.id) != b2_dynamicBody)
        {
            return;
        }
        auto mass{b2Body_GetMassData(entry.id)};
        if (mass.mass == 0.0f)
        {
            mass.mass = entry.mass_override.value_or(1.0f);
        }
        else if (entry.mass_override)
        {
            mass.rotationalInertia *= *entry.mass_override / mass.mass;
            mass.mass = *entry.mass_override;
        }
        else
        {
            return;
        }
        b2Body_SetMassData(entry.id, mass);
    }

    std::uint64_t id;
    core::slot_map<core::basic_handle<detail::body_tag>, body_entry> bodies;
    b2WorldId world;
    std::unordered_map<std::uint64_t, collider_ref> colliders;
    std::map<contact_key, contact_event> active_contacts;
    std::vector<contact_event> events;
    std::vector<contact_event> removed_contacts;
};

scene::scene() :
    impl_{std::make_unique<impl>()}
{
    impl_->id = next_scene_id.fetch_add(1, std::memory_order_relaxed);
    b2WorldDef settings{b2DefaultWorldDef()};
    settings.workerCount = 1;
    impl_->world = b2CreateWorld(&settings);
}

scene::~scene()
{
    if (B2_IS_NON_NULL(impl_->world))
    {
        b2DestroyWorld(impl_->world);
    }
}

void scene::step(float delta_seconds)
{
    if (B2_IS_NON_NULL(impl_->world) && std::isfinite(delta_seconds) && delta_seconds > 0.0f)
    {
        constexpr int substep_count{4};
        b2World_Step(impl_->world, delta_seconds, substep_count);
        impl_->collect_contact_events();
    }
}

bool scene::set_gravity(core::vec2 gravity)
{
    if (B2_IS_NULL(impl_->world) || !is_finite(gravity))
    {
        return false;
    }
    b2World_SetGravity(impl_->world, {gravity.x, gravity.y});
    return true;
}

body_handle scene::create_body(const body_desc& desc, std::span<const box_collider_config> colliders)
{
    if (B2_IS_NULL(impl_->world) || !is_finite(desc.transform.position) || !std::isfinite(desc.transform.angle_radians) || !is_finite(desc.velocity.linear) || !std::isfinite(desc.velocity.angular_radians_per_second))
    {
        return {};
    }
    if (desc.mass_override && (desc.type != body_type::dynamic_body || !std::isfinite(*desc.mass_override) || *desc.mass_override <= 0.0f))
    {
        return {};
    }

    b2BodyDef settings{b2DefaultBodyDef()};
    switch (desc.type)
    {
    case body_type::static_body:
    {
        settings.type = b2_staticBody;
        break;
    }
    case body_type::kinematic_body:
    {
        settings.type = b2_kinematicBody;
        break;
    }
    case body_type::dynamic_body:
    {
        settings.type = b2_dynamicBody;
        break;
    }
    default:
        return {};
    }

    settings.motionLocks.angularZ = desc.lock_rotation;
    settings.position = {desc.transform.position.x, desc.transform.position.y};
    settings.rotation = b2MakeRot(desc.transform.angle_radians);
    if (settings.type != b2_staticBody)
    {
        settings.linearVelocity = {desc.velocity.linear.x, desc.velocity.linear.y};
        settings.angularVelocity = desc.velocity.angular_radians_per_second;
    }

    const auto slot{impl_->bodies.emplace(impl::body_entry{})};
    const b2BodyId id{b2CreateBody(impl_->world, &settings)};
    if (B2_IS_NULL(id))
    {
        impl_->bodies.erase(slot);
        return {};
    }

    auto& entry{impl_->bodies[slot]};
    entry.id = id;
    entry.mass_override = desc.mass_override;
    impl::apply_dynamic_mass(entry);
    const body_handle body{impl_->id, slot};
    for (const auto& collider : colliders)
    {
        if (!add_box_collider({body, collider.half_extent, collider.center, collider.settings}))
        {
            destroy_body(body);
            return {};
        }
    }
    return body;
}

bool scene::destroy_body(body_handle body)
{
    if (!contains(body))
    {
        return false;
    }

    const auto& entry{impl_->bodies[body.slot]};
    impl_->remove_colliders(body, entry);
    b2DestroyBody(entry.id);
    impl_->bodies.erase(body.slot);
    return true;
}

bool scene::contains(body_handle body) const noexcept
{
    return body.scene_id == impl_->id && impl_->bodies.contains(body.slot);
}

bool scene::add_box_collider(const box_collider_desc& desc)
{
    const auto config{desc.settings.value_or(default_box_collider_settings)};
    if (!contains(desc.body) || !is_finite(desc.half_extent) || desc.half_extent.x <= 0.0f || desc.half_extent.y <= 0.0f || !is_finite(desc.center) || !is_nonnegative(config.density) || !is_nonnegative(config.friction) || !is_nonnegative(config.restitution))
    {
        return false;
    }

    b2ShapeDef settings{b2DefaultShapeDef()};
    settings.density = config.is_sensor ? 0.0f : config.density;
    settings.material.friction = config.friction;
    settings.material.restitution = config.restitution;
    settings.isSensor = config.is_sensor;
    settings.filter.categoryBits = config.filter.category_bits;
    settings.filter.maskBits = config.filter.mask_bits;
    settings.enableContactEvents = !config.is_sensor;
    settings.enableSensorEvents = true;
    const b2Polygon box{b2MakeOffsetBox(desc.half_extent.x, desc.half_extent.y, {desc.center.x, desc.center.y}, b2Rot_identity)};
    auto& entry{impl_->bodies[desc.body.slot]};
    const b2ShapeId shape{b2CreatePolygonShape(entry.id, &settings, &box)};
    if (B2_IS_NULL(shape))
    {
        return false;
    }
    const auto key{b2StoreShapeId(shape)};
    impl_->colliders.emplace(key, collider_ref{desc.body, entry.shapes.size()});
    entry.shapes.push_back(key);
    impl::apply_dynamic_mass(entry);
    return true;
}

std::vector<contact_point> scene::contacts_of(body_handle body) const
{
    std::vector<contact_point> result;
    if (!contains(body))
    {
        return result;
    }
    const auto id{impl_->bodies[body.slot].id};
    std::vector<b2ContactData> contacts(static_cast<std::size_t>(b2Body_GetContactCapacity(id)));
    const int count{b2Body_GetContactData(id, contacts.data(), static_cast<int>(contacts.size()))};
    for (int i{}; i < count; ++i)
    {
        const auto& contact{contacts[static_cast<std::size_t>(i)]};
        const auto first{b2Shape_GetBody(contact.shapeIdA)};
        const auto second{b2Shape_GetBody(contact.shapeIdB)};
        const bool is_first{B2_ID_EQUALS(first, id)};
        const auto normal{is_first ? b2Neg(contact.manifold.normal) : contact.manifold.normal};
        const auto self{impl_->colliders.find(b2StoreShapeId(is_first ? contact.shapeIdA : contact.shapeIdB))};
        const auto other{impl_->colliders.find(b2StoreShapeId(is_first ? contact.shapeIdB : contact.shapeIdA))};
        if (self == impl_->colliders.end() || other == impl_->colliders.end())
        {
            continue;
        }
        for (int j{}; j < contact.manifold.pointCount; ++j)
        {
            const auto& point{contact.manifold.points[j]};
            const auto velocity_a{b2Body_GetWorldPointVelocity(first, b2OffsetPos(b2Body_GetWorldCenter(first), point.anchorA))};
            const auto velocity_b{b2Body_GetWorldPointVelocity(second, b2OffsetPos(b2Body_GetWorldCenter(second), point.anchorB))};
            const auto relative_velocity{is_first ? b2Sub(velocity_a, velocity_b) : b2Sub(velocity_b, velocity_a)};
            result.push_back({self->second, other->second, {normal.x, normal.y}, {relative_velocity.x, relative_velocity.y}, point.separation, point.totalNormalImpulse});
        }
    }
    return result;
}

std::optional<ray_hit> scene::cast_ray(core::vec2 origin, core::vec2 translation, collision_filter filter) const
{
    if (B2_IS_NULL(impl_->world) || !is_finite(origin) || !is_finite(translation) || (translation.x == 0.0f && translation.y == 0.0f))
    {
        return std::nullopt;
    }
    const auto result{b2World_CastRayClosest(impl_->world, {origin.x, origin.y}, {translation.x, translation.y}, {filter.category_bits, filter.mask_bits})};
    if (!result.hit)
    {
        return std::nullopt;
    }
    return ray_hit{impl_->colliders.at(b2StoreShapeId(result.shapeId)), {static_cast<float>(result.point.x), static_cast<float>(result.point.y)}, {result.normal.x, result.normal.y}, result.fraction};
}

std::span<const contact_event> scene::contact_events() const
{
    return impl_->events;
}

std::optional<body_transform> scene::transform_of(body_handle body) const
{
    if (!contains(body))
    {
        return std::nullopt;
    }

    const auto id{impl_->bodies[body.slot].id};
    const b2Vec2 position{b2Body_GetPosition(id)};
    return body_transform{{static_cast<float>(position.x), static_cast<float>(position.y)}, b2Rot_GetAngle(b2Body_GetRotation(id))};
}

bool scene::set_transform(body_handle body, body_transform transform)
{
    if (!contains(body) || !is_finite(transform.position) || !std::isfinite(transform.angle_radians))
    {
        return false;
    }

    b2Body_SetTransform(impl_->bodies[body.slot].id, {transform.position.x, transform.position.y}, b2MakeRot(transform.angle_radians));
    return true;
}

std::optional<body_velocity> scene::velocity_of(body_handle body) const
{
    if (!contains(body))
    {
        return std::nullopt;
    }

    const auto id{impl_->bodies[body.slot].id};
    const b2Vec2 linear{b2Body_GetLinearVelocity(id)};
    return body_velocity{{linear.x, linear.y}, b2Body_GetAngularVelocity(id)};
}

bool scene::set_velocity(body_handle body, body_velocity velocity)
{
    if (!contains(body) || !is_finite(velocity.linear) || !std::isfinite(velocity.angular_radians_per_second))
    {
        return false;
    }

    const auto id{impl_->bodies[body.slot].id};
    if (b2Body_GetType(id) == b2_staticBody)
    {
        return false;
    }

    b2Body_SetLinearVelocity(id, {velocity.linear.x, velocity.linear.y});
    b2Body_SetAngularVelocity(id, velocity.angular_radians_per_second);
    return true;
}

bool scene::apply_force(body_handle body, core::vec2 force)
{
    if (!contains(body) || !is_finite(force))
    {
        return false;
    }

    const auto id{impl_->bodies[body.slot].id};
    if (b2Body_GetType(id) != b2_dynamicBody)
    {
        return false;
    }

    b2Body_ApplyForceToCenter(id, {force.x, force.y}, true);
    return true;
}

bool scene::apply_linear_impulse(body_handle body, core::vec2 impulse)
{
    if (!contains(body) || !is_finite(impulse))
    {
        return false;
    }

    const auto id{impl_->bodies[body.slot].id};
    if (b2Body_GetType(id) != b2_dynamicBody)
    {
        return false;
    }

    b2Body_ApplyLinearImpulseToCenter(id, {impulse.x, impulse.y}, true);
    return true;
}
} // namespace nori::physics2d
