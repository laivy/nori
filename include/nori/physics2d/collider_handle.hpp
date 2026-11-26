#pragma once

#include <cstddef>

#include <nori/physics2d/body_handle.hpp>

namespace nori::physics2d
{
struct collider_handle
{
    [[nodiscard]] bool operator==(const collider_handle&) const noexcept = default;

    body_handle body;
    std::size_t index;
};
} // namespace nori::physics2d
