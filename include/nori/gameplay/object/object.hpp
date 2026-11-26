#pragma once
#include <concepts>
#include <deque>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/component.hpp>
#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object_handle.hpp>
#include <nori/gameplay/update/update_group.hpp>
#include <nori/gameplay/update/update_handle.hpp>
#include <nori/gameplay/update/update_scheduler.hpp>

namespace nori::gameplay
{
class world;

class object : public core::rtti<object>
{
    friend class world;
    friend class component;

public:
    explicit object(core::transform pose = core::transform::identity);
    virtual ~object() = default;

    object(const object&) = delete;
    object& operator=(const object&) = delete;
    object(object&&) = delete;
    object& operator=(object&&) = delete;

    [[nodiscard]] object_handle handle() const;

    [[nodiscard]] auto world(this auto& self) noexcept
    {
        using world_type = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const gameplay::world, gameplay::world>;
        return static_cast<world_type*>(self.world_);
    }

    [[nodiscard]] decltype(auto) root_component(this auto& self) noexcept
    {
        return std::forward_like<decltype(self)>(*self.root_);
    }

    template <core::rtti_type Component, typename... Args>
    requires std::derived_from<Component, component>
    Component* add_component(Args&&... args)
    {
        if (component_lifecycle_active_)
        {
            core::abort("cannot add a component during removal notification");
        }

        if constexpr (Component::is_unique)
        {
            if (find_component<Component>())
            {
                return nullptr;
            }
        }

        auto comp{std::make_unique<Component>(*this, std::forward<Args>(args)...)};
        const auto result{comp.get()};
        components_.push_back(std::move(comp));
        if (world_ && !unregistering_components_)
        {
            static_cast<component&>(*result).register_component();
        }
        return result;
    }

    template <core::rtti_type Component, typename Self>
    requires std::derived_from<Component, component>
    [[nodiscard]] auto find_component(this Self& self)
    {
        using source_type = std::conditional_t<std::is_const_v<Self>, const component, component>;
        using result_type = std::conditional_t<std::is_const_v<Self>, const Component, Component>;

        for (const auto& comp : self.components_)
        {
            if (const auto result{core::cast<Component>(static_cast<source_type*>(comp.get()))})
            {
                return result;
            }
        }
        return static_cast<result_type*>(nullptr);
    }

    template <core::rtti_type Component, typename Self>
    requires std::derived_from<Component, component>
    [[nodiscard]] auto find_components(this Self& self)
    {
        using source_type = std::conditional_t<std::is_const_v<Self>, const component, component>;
        using result_type = std::conditional_t<std::is_const_v<Self>, const Component, Component>;

        std::vector<result_type*> result;
        for (const auto& comp : self.components_)
        {
            if (const auto match{core::cast<Component>(static_cast<source_type*>(comp.get()))})
            {
                result.push_back(match);
            }
        }
        return result;
    }

    bool remove_component(component& comp);

    [[nodiscard]] bool contains_component(const component& comp) const;
    [[nodiscard]] bool is_registered() const noexcept;

protected:
    virtual void on_register();
    virtual void on_unregister();

    template <typename Self, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Self&, float>)
    update_handle register_update(this Self& receiver, update_group group, Function function)
    {
        auto& owner{static_cast<object&>(receiver)};
        return owner.registration_scheduler().add(owner, receiver, group, function);
    }

    bool unregister_update(update_handle handle);
    bool set_update_enabled(update_handle handle, bool enabled);
    [[nodiscard]] bool is_update_enabled(update_handle handle) const;
    bool add_update_prerequisite(update_handle target, update_handle prerequisite);
    bool remove_update_prerequisite(update_handle target, update_handle prerequisite);

private:
    struct child_attachment
    {
        scene_component* child;
        core::transform pose;
    };

    void register_object(gameplay::world& world, object_handle handle);
    void unregister_object();
    bool flush_pending_changes();

    [[nodiscard]] std::optional<std::vector<child_attachment>> plan_reparenting(const component& comp);
    bool remove_component_now(component& comp);
    void notify_component_removed(const component& removed);
    update_scheduler& world_scheduler();
    [[nodiscard]] update_scheduler* active_scheduler() const;
    update_scheduler& registration_scheduler();

    gameplay::world* world_;
    object_handle handle_;
    bool unregistering_components_;
    bool component_lifecycle_active_;

    std::vector<std::unique_ptr<component>> components_;
    std::deque<component*> pending_component_removals_;
    scene_component* root_;
};
} // namespace nori::gameplay
