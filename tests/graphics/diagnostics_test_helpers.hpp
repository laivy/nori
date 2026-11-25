#pragma once

#include <wrl/client.h>

#include <directx/d3d12.h>

namespace nori::graphics::test_support
{
Microsoft::WRL::ComPtr<ID3D12Device> acquire_device();
} // namespace nori::graphics::test_support
