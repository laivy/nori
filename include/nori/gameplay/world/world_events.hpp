#pragma once

#include <cstddef>
#include <cstdint>

#include <nori/gameplay/object/object_handle.hpp>

namespace nori::gameplay
{
enum class trigger_phase
{
    begin,
    end
};

struct trigger_changed
{
    object_handle sensor;
    std::size_t sensor_collider;
    object_handle visitor;
    std::size_t visitor_collider;
    trigger_phase phase;
};

struct event_subscription
{
    std::uint64_t world_id;
    std::uint64_t sequence;
};
} // namespace nori::gameplay
