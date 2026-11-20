#pragma once
#include <nori/core/input_code.hpp>
#include <nori/core/vector.hpp>

namespace nori::core::input
{
[[nodiscard]] bool is_key_down(key_code key) noexcept;
[[nodiscard]] bool is_key_pressed(key_code key) noexcept;
[[nodiscard]] bool is_key_released(key_code key) noexcept;

[[nodiscard]] bool is_mouse_button_down(mouse_button button) noexcept;
[[nodiscard]] bool is_mouse_button_pressed(mouse_button button) noexcept;
[[nodiscard]] bool is_mouse_button_released(mouse_button button) noexcept;

[[nodiscard]] vec2 mouse_position() noexcept;
[[nodiscard]] vec2 mouse_delta() noexcept;
[[nodiscard]] float scroll_delta() noexcept;
} // namespace nori::core::input
