#include "context.hpp"
#include "diagnostics_test_helpers.hpp"

namespace nori::graphics::test_support
{
Microsoft::WRL::ComPtr<ID3D12Device> acquire_device()
{
    const auto graphics_context{context::create()};
    return graphics_context ? graphics_context->device() : nullptr;
}
} // namespace nori::graphics::test_support
