#pragma once
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/core/multicast_delegate.hpp>
#include <nori/core/rtti.hpp>
#include <nori/gameplay/detail/object_registry.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/object/object_handle.hpp>
#include <nori/gameplay/update/update_scheduler.hpp>
#include <nori/gameplay/world/world_events.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/scene.hpp>

namespace nori::gameplay
{
class game_instance;
class physics_body_component;

class world
{
    friend class game_instance;
    friend class physics_body_component;

public:
    using trigger_callback = std::move_only_function<void(const trigger_changed&)>;

    world();
    ~world();

    world(const world&) = delete;
    world& operator=(const world&) = delete;

    template <core::rtti_type object_type, typename... arguments>
    requires std::derived_from<object_type, object>
    object_handle create_object(arguments&&... args)
    {
        if (clearing_)
        {
            core::abort("cannot create an object while clearing a world");
        }
        return insert(std::make_unique<object_type>(std::forward<arguments>(args)...));
    }

    [[nodiscard]] object* find_object(object_handle handle);
    [[nodiscard]] const object* find_object(object_handle handle) const;
    [[nodiscard]] std::vector<object_handle> objects() const;
    [[nodiscard]] physics2d::scene& physics_scene();
    [[nodiscard]] const physics2d::scene& physics_scene() const;
    bool destroy_object(object_handle handle);

    [[nodiscard]] event_subscription subscribe_trigger_changed(trigger_callback callback);
    bool unsubscribe(event_subscription subscription);

    void update(float delta_seconds);
    void fixed_update(float delta_seconds);
    [[nodiscard]] update_scheduler& scheduler();

    // Processes pending removals, including requests made by lifecycle callbacks.
    // Must not be called during updates, lifecycle callbacks or another flush.
    void flush_pending_changes();

private:
    struct body_owner
    {
        physics2d::body_handle body;
        object_handle object;
    };

    struct active_trigger
    {
        physics2d::collider_ref sensor;
        physics2d::collider_ref visitor;
        trigger_changed event;
    };

    object_handle insert(std::unique_ptr<object> value);
    void clear();
    void dispatch_events();

    void register_body(physics2d::body_handle body, object_handle owner);
    void unregister_body(physics2d::body_handle body);
    void on_physics_update(float delta_seconds);
    void collect_trigger_events();

    std::uint64_t id_;
    physics2d::scene physics_;
    update_scheduler scheduler_;
    std::vector<object_handle> pending_destructions_;
    detail::object_registry objects_;
    bool flushing_{};
    bool clearing_{};
    core::multicast_delegate<void(const trigger_changed&)> trigger_changed_;
    std::vector<trigger_changed> events_;
    std::vector<body_owner> body_owners_;
    std::vector<active_trigger> active_triggers_;
};
} // namespace nori::gameplay
