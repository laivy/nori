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
    id_{next_world_id.fetch_add(1, std::memory_order_relaxed)},
    objects_{id_}
{
    if (id_ == 0)
    {
        core::abort("gameplay world identifier exhausted");
    }
    scheduler_.add(*this, update_group::physics, &world::on_physics_update);
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
    if (!objects_.contains(handle) || std::ranges::contains(pending_destructions_, handle))
    {
        return false;
    }
    pending_destructions_.push_back(handle);
    return true;
}

void world::update(float delta_seconds)
{
    scheduler_.execute(update_category::variable, delta_seconds);
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
}

update_scheduler& world::scheduler()
{
    return scheduler_;
}

void world::end_frame()
{
    if (scheduler_.is_executing() || flushing_)
    {
        core::abort("cannot flush pending changes during execution");
    }
    flushing_ = true;
    bool changed{};
    do
    {
        while (!pending_destructions_.empty())
        {
            const auto handle{pending_destructions_.front()};
            pending_destructions_.pop_front();
            const auto obj{objects_.extract(handle)};
            obj->unregister_object();
        }
        changed = false;
        for (const auto handle : objects())
        {
            if (const auto obj{find_object(handle)})
            {
                if (obj->flush_pending_changes())
                {
                    changed = true;
                }
            }
        }
    } while (changed || !pending_destructions_.empty());
    flushing_ = false;
}

object_handle world::insert(std::unique_ptr<object> obj)
{
    const auto handle{objects_.insert(std::move(obj))};
    objects_.find(handle)->register_object(*this, handle);
    return handle;
}

void world::clear()
{
    if (scheduler_.is_executing() || flushing_ || clearing_)
    {
        core::abort("cannot clear a world during execution");
    }
    clearing_ = true;
    for (const auto handle : objects())
    {
        destroy_object(handle);
    }
    end_frame();
    trigger_changed_ = {};
    events_.clear();
    body_owners_.clear();
    active_triggers_.clear();
    clearing_ = false;
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
