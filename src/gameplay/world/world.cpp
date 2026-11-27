#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/gameplay/component/physics_body_component.hpp>
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

void world::fixed_update(float delta_seconds)
{
    if (is_executing() || flushing_ || clearing_)
    {
        core::abort("recursive world update");
    }
    execute_guarded([this, delta_seconds]()
    {
        dispatch_update(update_targets(), &object::on_fixed_update, &component::on_fixed_update, delta_seconds);

        const auto handles{objects()};
        for (const auto handle : handles)
        {
            if (const auto body{find_object(handle)->find_component<physics_body_component>()})
            {
                body->sync_to_physics();
            }
        }
        if (std::isfinite(delta_seconds) && delta_seconds > 0.0f)
        {
            physics_.step(delta_seconds);
            collect_trigger_events();
        }
        for (const auto handle : handles)
        {
            if (const auto body{find_object(handle)->find_component<physics_body_component>()})
            {
                body->sync_from_physics();
            }
        }
        dispatch_events();
    });
}

void world::update(float delta_seconds)
{
    if (is_executing() || flushing_ || clearing_)
    {
        core::abort("recursive world update");
    }
    execute_guarded([this, delta_seconds]()
    {
        const auto targets{update_targets()};
        dispatch_update(targets, &object::on_update, &component::on_update, delta_seconds);
        dispatch_update(targets, &object::on_post_update, &component::on_post_update, delta_seconds);
    });
}

void world::end_frame()
{
    if (is_executing() || flushing_)
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
    if (is_executing() || flushing_ || clearing_)
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

std::vector<world::update_target> world::update_targets()
{
    const auto handles{objects()};
    std::vector<update_target> targets;
    targets.reserve(handles.size());
    for (const auto handle : handles)
    {
        const auto value{find_object(handle)};
        targets.push_back({value, value->components_.size()});
    }
    return targets;
}

void world::dispatch_update(const std::vector<update_target>& targets, void (object::*object_callback)(float), void (component::*component_callback)(float), float delta_seconds)
{
    for (const auto& [value, component_count] : targets)
    {
        (value->*object_callback)(delta_seconds);
        for (std::size_t index{}; index < component_count; ++index)
        {
            (value->components_[index].get()->*component_callback)(delta_seconds);
        }
    }
}

void world::execute_guarded(std::move_only_function<void()> callback)
{
    ++execution_depth_;
    struct execution_scope
    {
        std::size_t& depth;

        ~execution_scope()
        {
            --depth;
        }
    } scope{execution_depth_};
    callback();
}

bool world::is_executing() const noexcept
{
    return execution_depth_ != 0;
}
} // namespace nori::gameplay
