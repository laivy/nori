#pragma once
#include <nori/core/input_code.hpp>

namespace nori::core::input
{
void update_key_down(key_code key, bool repeat) noexcept;
void update_key_up(key_code key) noexcept;
void update_mouse_button_down(mouse_button button, int x, int y) noexcept;
void update_mouse_button_up(mouse_button button, int x, int y) noexcept;
void update_mouse_move(int x, int y) noexcept;
void update_mouse_wheel(int x, int y, float delta) noexcept;

void reset_mouse_buttons() noexcept;

void clear_frame() noexcept;
void reset() noexcept;
} // namespace nori::core::input
