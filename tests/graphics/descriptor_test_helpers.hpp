#pragma once

#include <vector>

#include "descriptor_allocator.hpp"

namespace
{
std::size_t available_descriptor_count(nori::graphics::descriptor_allocator& descriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    std::vector<nori::graphics::descriptor_handle> available;
    while (true)
    {
        const auto handle{descriptors.allocate(type)};
        if (handle.cpu.ptr == 0)
        {
            break;
        }
        available.push_back(handle);
    }
    for (auto it{available.rbegin()}; it != available.rend(); ++it)
    {
        descriptors.free(*it);
    }
    return available.size();
}
} // namespace
