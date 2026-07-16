#pragma once
#include <nori/core/event.hpp>
#include <nori/core/input_code.hpp>

namespace nori::core
{
struct mouse_position
{
    int x;
    int y;
};

void update_input_state(const event& e);
void clear_frame_input_state();

bool is_key_down(key_code key);
bool is_key_pressed(key_code key);
bool is_key_released(key_code key);
bool is_mouse_button_down(mouse_button button);
bool is_mouse_button_pressed(mouse_button button);
bool is_mouse_button_released(mouse_button button);
mouse_position get_mouse_position();
mouse_position get_mouse_delta();
float get_scroll_delta();
} // namespace nori::core
