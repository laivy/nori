#include <array>
#include <cstddef>
#include "input.hpp"

namespace nori::core
{
namespace
{
constexpr std::size_t key_count{static_cast<std::size_t>(key_code::alt) + 1};
constexpr std::size_t mouse_button_count{static_cast<std::size_t>(mouse_button::middle) + 1};

std::array<bool, key_count> key_down{};
std::array<bool, key_count> key_pressed{};
std::array<bool, key_count> key_released{};
std::array<bool, mouse_button_count> mouse_button_down{};
std::array<bool, mouse_button_count> mouse_button_pressed{};
std::array<bool, mouse_button_count> mouse_button_released{};
mouse_position mouse_position_state{};
mouse_position mouse_delta_state{};
float scroll_delta{};
bool has_mouse_position{false};

std::size_t to_index(key_code key)
{
    return static_cast<std::size_t>(key);
}

std::size_t to_index(mouse_button button)
{
    return static_cast<std::size_t>(button);
}

bool is_valid(key_code key)
{
    if (key == key_code::unknown)
    {
        return false;
    }
    const std::size_t index{to_index(key)};
    if (index >= key_down.size())
    {
        return false;
    }
    return true;
}

bool is_valid(mouse_button button)
{
    if (button == mouse_button::unknown)
    {
        return false;
    }
    const std::size_t index{to_index(button)};
    if (index >= mouse_button_down.size())
    {
        return false;
    }
    return true;
}

void update_mouse_position(int x, int y)
{
    if (has_mouse_position)
    {
        mouse_delta_state.x += x - mouse_position_state.x;
        mouse_delta_state.y += y - mouse_position_state.y;
    }
    mouse_position_state = {x, y};
    has_mouse_position = true;
}
} // namespace

void update_input_state(const event& e)
{
    if (const auto data{std::get_if<key_down_event>(&e.data)})
    {
        if (!is_valid(data->key))
        {
            return;
        }
        const std::size_t index{to_index(data->key)};
        if (!key_down[index] && !data->repeat)
        {
            key_pressed[index] = true;
        }
        key_down[index] = true;
        return;
    }

    if (const auto data{std::get_if<key_up_event>(&e.data)})
    {
        if (!is_valid(data->key))
        {
            return;
        }
        const std::size_t index{to_index(data->key)};
        if (key_down[index])
        {
            key_released[index] = true;
        }
        key_down[index] = false;
        return;
    }

    if (const auto data{std::get_if<mouse_button_down_event>(&e.data)})
    {
        update_mouse_position(data->x, data->y);
        if (!is_valid(data->button))
        {
            return;
        }
        const std::size_t index{to_index(data->button)};
        if (!mouse_button_down[index])
        {
            mouse_button_pressed[index] = true;
        }
        mouse_button_down[index] = true;
        return;
    }

    if (const auto data{std::get_if<mouse_button_up_event>(&e.data)})
    {
        update_mouse_position(data->x, data->y);
        if (!is_valid(data->button))
        {
            return;
        }
        const std::size_t index{to_index(data->button)};
        if (mouse_button_down[index])
        {
            mouse_button_released[index] = true;
        }
        mouse_button_down[index] = false;
        return;
    }

    if (const auto data{std::get_if<mouse_move_event>(&e.data)})
    {
        update_mouse_position(data->x, data->y);
        return;
    }

    if (const auto data{std::get_if<mouse_wheel_event>(&e.data)})
    {
        update_mouse_position(data->x, data->y);
        scroll_delta += data->delta;
        return;
    }
}

void clear_frame_input_state()
{
    key_pressed.fill(false);
    key_released.fill(false);
    mouse_button_pressed.fill(false);
    mouse_button_released.fill(false);
    mouse_delta_state = {};
    scroll_delta = {};
}

bool is_key_down(key_code key)
{
    if (!is_valid(key))
    {
        return false;
    }
    return key_down[to_index(key)];
}

bool is_key_pressed(key_code key)
{
    if (!is_valid(key))
    {
        return false;
    }
    return key_pressed[to_index(key)];
}

bool is_key_released(key_code key)
{
    if (!is_valid(key))
    {
        return false;
    }
    return key_released[to_index(key)];
}

bool is_mouse_button_down(mouse_button button)
{
    if (!is_valid(button))
    {
        return false;
    }
    return mouse_button_down[to_index(button)];
}

bool is_mouse_button_pressed(mouse_button button)
{
    if (!is_valid(button))
    {
        return false;
    }
    return mouse_button_pressed[to_index(button)];
}

bool is_mouse_button_released(mouse_button button)
{
    if (!is_valid(button))
    {
        return false;
    }
    return mouse_button_released[to_index(button)];
}

mouse_position get_mouse_position()
{
    return mouse_position_state;
}

mouse_position get_mouse_delta()
{
    return mouse_delta_state;
}

float get_scroll_delta()
{
    return scroll_delta;
}
} // namespace nori::core
