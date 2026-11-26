#pragma once
#include <nori/core/transform.hpp>
#include <nori/gameplay/component/component.hpp>

namespace nori::gameplay
{
enum class attachment_rule
{
    keep_relative,
    keep_world,
    snap_to_target
};

struct attachment_rules
{
    attachment_rule position;
    attachment_rule rotation;
    attachment_rule scale;
};

inline constexpr attachment_rules keep_world_transform{attachment_rule::keep_world, attachment_rule::keep_world, attachment_rule::keep_world};
inline constexpr attachment_rules keep_relative_transform{attachment_rule::keep_relative, attachment_rule::keep_relative, attachment_rule::keep_relative};
inline constexpr attachment_rules snap_to_target_not_including_scale{attachment_rule::snap_to_target, attachment_rule::snap_to_target, attachment_rule::keep_world};

enum class detachment_rule
{
    keep_relative,
    keep_world
};

class scene_component : public core::rtti<scene_component, component>
{
public:
    explicit scene_component(object& owner, core::transform pose = core::transform::identity);

    [[nodiscard]] const core::transform& relative_transform() const;
    [[nodiscard]] core::transform world_transform() const;
    [[nodiscard]] const scene_component* parent() const;
    bool set_relative_transform(core::transform pose);
    bool attach_to_component(scene_component& parent, attachment_rules rules);
    bool detach_from_component(detachment_rule rule);

private:
    core::transform relative_;
    scene_component* parent_{};
};
} // namespace nori::gameplay
