#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

#include <nori/core/abort.hpp>

#include <nori/gameplay/world/world.hpp>

namespace
{
std::atomic<std::uint64_t> next_world_id{1};
}

namespace nori::gameplay
{
world::world() :
    id_{next_world_id.fetch_add(1, std::memory_order_relaxed)}
{
    if (id_ == 0)
    {
        core::abort("gameplay world identifier exhausted");
    }
    scheduler_.register_update(*this, update_group::physics, &world::on_physics_update);
}

world::~world()
{
    clear();
}

object* world::find_object(object_handle handle)
{
    return const_cast<object*>(std::as_const(*this).find_object(handle));
}

const object* world::find_object(object_handle handle) const
{
    return objects_.find(handle);
}

std::vector<object_handle> world::objects() const
{
    return objects_.objects();
}

physics2d::scene& world::physics_scene()
{
    return physics_;
}

const physics2d::scene& world::physics_scene() const
{
    return physics_;
}

bool world::destroy_object(object_handle handle)
{
    auto* object{find_object(handle)};
    if (!object)
    {
        return false;
    }
    if (scheduler_.is_executing())
    {
        if (std::ranges::find(pending_destructions_, handle) != pending_destructions_.end())
        {
            return false;
        }
        pending_destructions_.push_back(handle);
        return true;
    }
    std::erase(pending_destructions_, handle);
    if (!objects_.destroy(handle))
    {
        return false;
    }
    object->detach_from_world();
    return true;
}

void world::update(float delta_seconds)
{
    scheduler_.execute(update_category::variable, delta_seconds);
    flush_pending_changes();
}

void world::fixed_update(float delta_seconds)
{
    scheduler_.execute(update_category::fixed, delta_seconds);
    if (!events_.empty())
    {
        scheduler_.execute_lifecycle([this]()
        {
            dispatch_events();
        });
    }
    flush_pending_changes();
}

update_scheduler& world::scheduler()
{
    return scheduler_;
}

void world::flush_pending_changes()
{
    if (scheduler_.is_executing())
    {
        core::abort("cannot flush objects during update execution");
    }
    while (!pending_destructions_.empty())
    {
        auto pending{std::exchange(pending_destructions_, {})};
        for (const auto handle : pending)
        {
            destroy_object(handle);
        }
    }
    objects_.flush_pending_changes();
}

object_handle world::insert(std::unique_ptr<object> value)
{
    const auto handle{objects_.insert(std::move(value))};
    objects_.find(handle)->attach_to_world(*this, handle);
    return handle;
}

void world::clear()
{
    if (scheduler_.is_executing())
    {
        core::abort("cannot clear a world during update execution");
    }
    for (const auto handle : objects())
    {
        destroy_object(handle);
    }
    flush_pending_changes();
    subscribers_.clear();
    events_.clear();
    body_owners_.clear();
    active_triggers_.clear();
}

void world::on_physics_update(float delta_seconds)
{
    if (std::isfinite(delta_seconds) && delta_seconds > 0.0f)
    {
        physics_.step(delta_seconds);
        collect_trigger_events();
    }
}
} // namespace nori::gameplay
