#include <algorithm>
#include <cassert>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace nori::gameplay
{
object::object(core::transform pose) :
    world_{nullptr},
    handle_{},
    unregistering_components_{false},
    component_lifecycle_active_{false},
    root_{add_component<scene_component>(pose)}
{
}

object_handle object::handle() const
{
    return handle_;
}

bool object::is_registered() const noexcept
{
    return world_ && !unregistering_components_;
}

void object::on_register()
{
}

void object::on_unregister()
{
}

void object::on_fixed_update(float)
{
}

void object::on_update(float)
{
}

void object::on_post_update(float)
{
}

void object::register_object(gameplay::world& world, object_handle handle)
{
    if (world_ || world.find(handle) != this)
    {
        core::abort("object registration requires an owning world");
    }
    world_ = &world;
    handle_ = handle;
    for (const auto& comp : components_)
    {
        comp->register_component();
    }
    world_->execute_guarded([this]()
    {
        component_lifecycle_active_ = true;
        on_register();
        component_lifecycle_active_ = false;
    });
}

void object::unregister_object()
{
    if (!world_ || unregistering_components_)
    {
        return;
    }
    if (world_->is_executing() || world_->find(handle_) == this)
    {
        core::abort("object unregistration requires removal at a lifecycle boundary");
    }
    unregistering_components_ = true;
    world_->execute_guarded([this]()
    {
        component_lifecycle_active_ = true;
        on_unregister();
        component_lifecycle_active_ = false;
    });
    for (const auto& comp : components_)
    {
        comp->unregister_component();
    }
    world_ = nullptr;
    handle_ = {};
    unregistering_components_ = false;
}

bool object::remove_component(component& comp)
{
    if (component_lifecycle_active_ || &comp == root_ || !contains_component(comp) || std::ranges::contains(pending_component_removals_, &comp))
    {
        return false;
    }
    if (world_)
    {
        if (!plan_reparenting(comp))
        {
            return false;
        }
        pending_component_removals_.push_back(&comp);
        return true;
    }
    return remove_component_now(comp);
}

bool object::contains_component(const component& comp) const
{
    return std::ranges::contains(components_, &comp, &std::unique_ptr<component>::get);
}

bool object::flush_pending_changes()
{
    if (component_lifecycle_active_ || (world_ && world_->is_executing()))
    {
        core::abort("cannot flush pending changes during execution");
    }

    bool changed{false};
    const auto count{pending_component_removals_.size()};
    for (std::size_t index{}; index < count; ++index)
    {
        const auto comp{pending_component_removals_.front()};
        pending_component_removals_.pop_front();
        if (remove_component_now(*comp))
        {
            changed = true;
        }
        else
        {
            pending_component_removals_.push_back(comp);
        }
    }
    return changed;
}

std::optional<std::vector<object::child_attachment>> object::plan_reparenting(const component& comp)
{
    std::vector<child_attachment> attachments;
    if (const auto parent{core::cast<scene_component>(&comp)})
    {
        for (const auto child : find_components<scene_component>())
        {
            if (child->parent() == parent)
            {
                const auto pose{child->compute_attachment_transform(*root_, keep_world_transform)};
                if (!pose)
                {
                    return std::nullopt;
                }
                attachments.push_back({child, *pose});
            }
        }
    }
    return attachments;
}

bool object::remove_component_now(component& comp)
{
    const auto attachments{plan_reparenting(comp)};
    if (!attachments)
    {
        return false;
    }
    for (const auto& [child, pose] : *attachments)
    {
        child->set_relative_transform(pose);
        child->parent_ = root_;
    }
    const auto it{std::ranges::find(components_, &comp, &std::unique_ptr<component>::get)};
    assert(it != components_.end());
    auto retired{std::move(*it)};
    components_.erase(it);
    notify_component_removed(*retired);
    retired->unregister_component();
    return true;
}

void object::notify_component_removed(const component& removed)
{
    const auto notify = [this, &removed]() noexcept
    {
        component_lifecycle_active_ = true;
        for (const auto& comp : components_)
        {
            comp->on_component_removed(removed);
        }
        component_lifecycle_active_ = false;
    };
    if (world_)
    {
        world_->execute_guarded(notify);
    }
    else
    {
        notify();
    }
}
} // namespace nori::gameplay
