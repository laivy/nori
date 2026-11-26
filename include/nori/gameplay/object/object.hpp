#pragma once

#include <concepts>
#include <deque>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/core/rtti.hpp>

#include <nori/gameplay/component/scene_component.hpp>
#include <nori/gameplay/object/object_handle.hpp>

namespace nori::gameplay
{
class world;

class object : public core::rtti<object>
{
public:
    explicit object(transform pose = transform::identity);
    virtual ~object() = default;

    object(const object&) = delete;
    object& operator=(const object&) = delete;
    object(object&&) = delete;
    object& operator=(object&&) = delete;

    [[nodiscard]] ::nori::gameplay::world& world();
    [[nodiscard]] const ::nori::gameplay::world& world() const;
    [[nodiscard]] object_handle handle() const;

    [[nodiscard]] scene_component& root_component();
    [[nodiscard]] const scene_component& root_component() const;

    template <core::rtti_type component_type, typename... arguments>
    requires std::derived_from<component_type, component>
    component_type& add_component(arguments&&... args)
    {
        if (notifying_component_removal_)
        {
            core::abort("cannot add a component during removal notification");
        }
        auto value{std::make_unique<component_type>(*this, std::forward<arguments>(args)...)};
        auto& result{*value};
        components_.push_back(std::move(value));
        if (world_ && !unregistering_components_)
        {
            static_cast<component&>(result).register_component();
        }
        return result;
    }

    template <core::rtti_type component_type>
    requires std::derived_from<component_type, component>
    [[nodiscard]] component_type* find_component()
    {
        return const_cast<component_type*>(std::as_const(*this).find_component<component_type>());
    }

    template <core::rtti_type component_type>
    requires std::derived_from<component_type, component>
    [[nodiscard]] const component_type* find_component() const
    {
        for (const auto& entry : components_)
        {
            if (const auto result{core::cast<component_type>(static_cast<const component*>(entry.get()))})
            {
                return result;
            }
        }
        return nullptr;
    }

    template <core::rtti_type component_type>
    requires std::derived_from<component_type, component>
    [[nodiscard]] std::vector<component_type*> find_components()
    {
        return collect_components<component_type>(*this);
    }

    template <core::rtti_type component_type>
    requires std::derived_from<component_type, component>
    [[nodiscard]] std::vector<const component_type*> find_components() const
    {
        return collect_components<component_type>(*this);
    }

    bool remove_component(component& value);
    [[nodiscard]] bool contains_component(const component& value) const;
    [[nodiscard]] bool is_registered() const noexcept;

    void attach_to_world(::nori::gameplay::world& active, object_handle handle);
    void detach_from_world();
    void finish_update();

protected:
    virtual void on_register()
    {
    }

    virtual void on_unregister()
    {
    }

    template <typename Self, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Self&, float>)
    update_handle register_update(this Self& instance, update_group group, Function function)
    {
        auto& owner{static_cast<object&>(instance)};
        return owner.registration_scheduler().register_update(owner, instance, group, function);
    }

    bool unregister_update(update_handle handle);
    bool set_update_enabled(update_handle handle, bool enabled);
    [[nodiscard]] bool is_update_enabled(update_handle handle) const;
    bool add_update_prerequisite(update_handle target, update_handle prerequisite);
    bool remove_update_prerequisite(update_handle target, update_handle prerequisite);

private:
    template <typename component_type, typename object_type>
    static auto collect_components(object_type& object)
    {
        using source_type = std::conditional_t<std::is_const_v<object_type>, const component, component>;
        using result_type = std::conditional_t<std::is_const_v<object_type>, const component_type, component_type>;
        std::vector<result_type*> result;
        for (const auto& entry : object.components_)
        {
            if (const auto match{core::cast<component_type>(static_cast<source_type*>(entry.get()))})
            {
                result.push_back(match);
            }
        }
        return result;
    }

    void remove_component_now(component& value);
    void notify_component_removed(const component& value);
    update_scheduler& world_scheduler();
    [[nodiscard]] update_scheduler* active_scheduler() const;
    update_scheduler& registration_scheduler();

    ::nori::gameplay::world* world_{};
    object_handle handle_{};
    bool unregistering_components_{};
    bool notifying_component_removal_{};

    std::vector<std::unique_ptr<component>> components_;
    std::vector<std::unique_ptr<component>> retired_components_;
    std::deque<component*> pending_component_removals_;
    scene_component* root_;
};
} // namespace nori::gameplay
