#pragma once
#include <Windows.h>

#include <imgui.h>

#include <nori/graphics/render_pass.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::imgui
{
bool initialize(HWND hwnd);
void shutdown() noexcept;

bool process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

bool begin_render_pass(const graphics::render_pass_desc& desc);
bool end_render_pass();

ImTextureID texture_id(texture_handle handle) noexcept;
} // namespace nori::graphics::imgui
