#pragma once
#ifndef IMGUI_DISABLE
#include <Windows.h>
#include <nori/core/types.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/resource/handle.hpp>

namespace nori::graphics::imgui
{
result<void> initialize(HWND hwnd);
result<void> render();

void image(
    resource::handle handle,
    core::float2 size,
    core::float2 uv0 = {0.0f, 0.0f},
    core::float2 uv1 = {1.0f, 1.0f}
);
void invalidate_image(resource::handle handle);
} // namespace nori::graphics::imgui
#endif
