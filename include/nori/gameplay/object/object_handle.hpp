#pragma once

#include <cstddef>
#include <cstdint>

namespace nori::gameplay
{
struct object_handle
{
    std::uint64_t world_id;
    std::size_t slot;
    std::uint64_t generation;

    bool operator==(const object_handle&) const = default;
};

} // namespace nori::gameplay
