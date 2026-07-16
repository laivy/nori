#pragma once
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include "descriptor_allocator.hpp"

namespace nori::graphics
{
struct texture
{
    enum class state
    {
        unavailable,
        available
    };

    state availability;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    descriptor_handle srv_handle;
};
} // namespace nori::graphics
