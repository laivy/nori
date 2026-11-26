#pragma once

namespace nori::gameplay
{
enum class update_group
{
    update,
    late_update,

    input,
    fixed_commands,
    pre_physics,
    physics,
    post_physics,
    post_fixed_update
};
} // namespace nori::gameplay
