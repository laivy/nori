#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
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
#include <nori/gameplay/world/world_events.hpp>
#include <nori/physics2d/body_handle.hpp>
#include <nori/physics2d/collider_handle.hpp>
#include <nori/physics2d/scene.hpp>

namespace nori::gameplay
{
class physics_body_component;
class game_instance;

class world
{
    friend class physics_body_component;
    friend class game_instance;
    friend class object;
    friend class component;

public:
    using trigger_callback = std::move_only_function<void(const trigger_changed&)>;

    world();
    ~world();

    world(const world&) = delete;
    world& operator=(const world&) = delete;

    template <core::rtti_type ObjectType, typename... Args>
    requires std::derived_from<ObjectType, object>
    object_handle create_object(Args&&... args)
    {
        if (clearing_)
        {
            core::abort("cannot create an object while clearing a world");
        }
        return insert(std::make_unique<ObjectType>(std::forward<Args>(args)...));
    }

    [[nodiscard]] object* find_object(object_handle handle);
    [[nodiscard]] const object* find_object(object_handle handle) const;
    [[nodiscard]] std::vector<object_handle> objects() const;
    [[nodiscard]] physics2d::scene& physics_scene();
    [[nodiscard]] const physics2d::scene& physics_scene() const;
    bool destroy_object(object_handle handle);

    [[nodiscard]] event_subscription subscribe_trigger_changed(trigger_callback callback);
    bool unsubscribe(event_subscription subscription);

    void fixed_update(float delta_seconds);
    void update(float delta_seconds);

    void end_frame();

private:
    struct update_target
    {
        object* value;
        std::size_t component_count;
    };

    struct body_owner
    {
        physics2d::body_handle body;
        object_handle object;
    };

    struct active_trigger
    {
        physics2d::collider_handle sensor;
        physics2d::collider_handle visitor;
        trigger_changed event;
    };

    object_handle insert(std::unique_ptr<object> obj);
    void clear();
    void dispatch_events();

    void register_body(physics2d::body_handle body, object_handle owner);
    void unregister_body(physics2d::body_handle body);
    void collect_trigger_events();

    [[nodiscard]] std::vector<update_target> update_targets();
    void dispatch_update(const std::vector<update_target>& targets, void (object::*object_callback)(float), void (component::*component_callback)(float), float delta_seconds);
    void execute_guarded(std::move_only_function<void()> callback);
    [[nodiscard]] bool is_executing() const noexcept;

    std::uint64_t id_;
    physics2d::scene physics_;
    std::deque<object_handle> pending_destructions_;
    detail::object_registry objects_;
    std::size_t execution_depth_{};
    bool flushing_{};
    bool clearing_{};
    core::multicast_delegate<void(const trigger_changed&)> trigger_changed_;
    std::vector<trigger_changed> events_;
    std::vector<body_owner> body_owners_;
    std::vector<active_trigger> active_triggers_;
};
} // namespace nori::gameplay
