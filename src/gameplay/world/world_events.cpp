#include <algorithm>
#include <utility>

#include <nori/core/abort.hpp>

#include <nori/gameplay/world/world.hpp>

namespace
{
bool is_same_collider(nori::physics2d::collider_ref left, nori::physics2d::collider_ref right)
{
    return left.body == right.body && left.index == right.index;
}
} // namespace

namespace nori::gameplay
{
event_subscription world::subscribe_trigger_changed(trigger_callback callback)
{
    if (!callback)
    {
        core::abort("cannot subscribe a null event callback");
    }
    return {id_, trigger_changed_.add(std::move(callback))};
}

bool world::unsubscribe(event_subscription subscription)
{
    if (subscription.world_id != id_)
    {
        return false;
    }
    return trigger_changed_.remove(subscription.slot);
}

void world::dispatch_events()
{
    const auto pending{std::exchange(events_, {})};
    for (const auto& event : pending)
    {
        trigger_changed_.broadcast(event);
    }
}

void world::register_body(physics2d::body_handle body, object_handle owner)
{
    body_owners_.push_back({body, owner});
}

void world::unregister_body(physics2d::body_handle body)
{
    std::erase_if(body_owners_, [body](const body_owner& entry)
    {
        return entry.body == body;
    });
}

void world::collect_trigger_events()
{
    for (const auto& event : physics_.contact_events())
    {
        if (event.kind != physics2d::contact_kind::trigger)
        {
            continue;
        }
        if (event.phase == physics2d::contact_phase::end)
        {
            const auto entry{std::ranges::find_if(active_triggers_, [&](const active_trigger& trigger)
            {
                return is_same_collider(trigger.sensor, event.first) && is_same_collider(trigger.visitor, event.second);
            })};
            if (entry != active_triggers_.end())
            {
                auto ended{entry->event};
                ended.phase = trigger_phase::end;
                events_.emplace_back(ended);
                active_triggers_.erase(entry);
            }
            continue;
        }
        const auto sensor{std::ranges::find(body_owners_, event.first.body, &body_owner::body)};
        const auto visitor{std::ranges::find(body_owners_, event.second.body, &body_owner::body)};
        if (sensor != body_owners_.end() && visitor != body_owners_.end())
        {
            const trigger_changed began{sensor->object, event.first.index, visitor->object, event.second.index, trigger_phase::begin};
            active_triggers_.push_back({event.first, event.second, began});
            events_.emplace_back(began);
        }
    }
}
} // namespace nori::gameplay
