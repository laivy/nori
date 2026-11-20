#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

#include <nori/core/input.hpp>

#include "input_internal.hpp"

namespace
{
class input_state
{
public:
    input_state() noexcept;

    void update_key_down(nori::core::key_code key, bool repeat) noexcept;
    void update_key_up(nori::core::key_code key) noexcept;
    void update_mouse_button_down(nori::core::mouse_button button, int x, int y) noexcept;
    void update_mouse_button_up(nori::core::mouse_button button, int x, int y) noexcept;
    void update_mouse_move(int x, int y) noexcept;
    void update_mouse_wheel(int x, int y, float delta) noexcept;

    void reset_mouse_buttons() noexcept;
    void clear_frame() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool is_key_down(nori::core::key_code key) const noexcept;
    [[nodiscard]] bool is_key_pressed(nori::core::key_code key) const noexcept;
    [[nodiscard]] bool is_key_released(nori::core::key_code key) const noexcept;

    [[nodiscard]] bool is_mouse_button_down(nori::core::mouse_button button) const noexcept;
    [[nodiscard]] bool is_mouse_button_pressed(nori::core::mouse_button button) const noexcept;
    [[nodiscard]] bool is_mouse_button_released(nori::core::mouse_button button) const noexcept;

    [[nodiscard]] nori::core::vec2 get_mouse_position() const noexcept;
    [[nodiscard]] nori::core::vec2 get_mouse_delta() const noexcept;
    [[nodiscard]] float get_scroll_delta() const noexcept;

private:
    [[nodiscard]] static constexpr std::size_t to_index(nori::core::key_code key) noexcept;
    [[nodiscard]] static constexpr std::size_t to_index(nori::core::mouse_button button) noexcept;
    [[nodiscard]] static constexpr bool is_valid(nori::core::key_code key) noexcept;
    [[nodiscard]] static constexpr bool is_valid(nori::core::mouse_button button) noexcept;
    void update_mouse_position(int x, int y) noexcept;

    static constexpr std::size_t key_count{static_cast<std::size_t>(nori::core::key_code::count)};
    static constexpr std::size_t mouse_button_count{static_cast<std::size_t>(nori::core::mouse_button::count)};

    std::array<bool, key_count> key_down_;
    std::array<bool, key_count> key_pressed_;
    std::array<bool, key_count> key_released_;

    std::array<bool, mouse_button_count> mouse_button_down_;
    std::array<bool, mouse_button_count> mouse_button_pressed_;
    std::array<bool, mouse_button_count> mouse_button_released_;

    std::optional<nori::core::vec2> mouse_position_;
    nori::core::vec2 mouse_delta_;
    float scroll_delta_;
};

input_state::input_state() noexcept :
    key_down_{},
    key_pressed_{},
    key_released_{},
    mouse_button_down_{},
    mouse_button_pressed_{},
    mouse_button_released_{},
    mouse_position_{std::nullopt},
    mouse_delta_{},
    scroll_delta_{0.0f}
{
}

void input_state::update_key_down(nori::core::key_code key, bool repeat) noexcept
{
    if (!is_valid(key))
    {
        return;
    }
    const std::size_t index{to_index(key)};
    if (!key_down_[index] && !repeat)
    {
        key_pressed_[index] = true;
    }
    key_down_[index] = true;
}

void input_state::update_key_up(nori::core::key_code key) noexcept
{
    if (!is_valid(key))
    {
        return;
    }
    const std::size_t index{to_index(key)};
    if (key_down_[index])
    {
        key_released_[index] = true;
    }
    key_down_[index] = false;
}

void input_state::update_mouse_button_down(nori::core::mouse_button button, int x, int y) noexcept
{
    update_mouse_position(x, y);
    if (!is_valid(button))
    {
        return;
    }
    const std::size_t index{to_index(button)};
    if (!mouse_button_down_[index])
    {
        mouse_button_pressed_[index] = true;
    }
    mouse_button_down_[index] = true;
}

void input_state::update_mouse_button_up(nori::core::mouse_button button, int x, int y) noexcept
{
    update_mouse_position(x, y);
    if (!is_valid(button))
    {
        return;
    }
    const std::size_t index{to_index(button)};
    if (mouse_button_down_[index])
    {
        mouse_button_released_[index] = true;
    }
    mouse_button_down_[index] = false;
}

void input_state::update_mouse_move(int x, int y) noexcept
{
    update_mouse_position(x, y);
}

void input_state::update_mouse_wheel(int x, int y, float delta) noexcept
{
    update_mouse_position(x, y);
    scroll_delta_ += delta;
}

void input_state::reset_mouse_buttons() noexcept
{
    mouse_button_down_.fill(false);
    mouse_button_pressed_.fill(false);
    mouse_button_released_.fill(false);
}

void input_state::clear_frame() noexcept
{
    key_pressed_.fill(false);
    key_released_.fill(false);
    mouse_button_pressed_.fill(false);
    mouse_button_released_.fill(false);
    mouse_delta_ = {};
    scroll_delta_ = 0.0f;
}

void input_state::reset() noexcept
{
    key_down_.fill(false);
    mouse_button_down_.fill(false);
    clear_frame();
    mouse_position_.reset();
}

bool input_state::is_key_down(nori::core::key_code key) const noexcept
{
    return is_valid(key) && key_down_[to_index(key)];
}

bool input_state::is_key_pressed(nori::core::key_code key) const noexcept
{
    return is_valid(key) && key_pressed_[to_index(key)];
}

bool input_state::is_key_released(nori::core::key_code key) const noexcept
{
    return is_valid(key) && key_released_[to_index(key)];
}

bool input_state::is_mouse_button_down(nori::core::mouse_button button) const noexcept
{
    return is_valid(button) && mouse_button_down_[to_index(button)];
}

bool input_state::is_mouse_button_pressed(nori::core::mouse_button button) const noexcept
{
    return is_valid(button) && mouse_button_pressed_[to_index(button)];
}

bool input_state::is_mouse_button_released(nori::core::mouse_button button) const noexcept
{
    return is_valid(button) && mouse_button_released_[to_index(button)];
}

nori::core::vec2 input_state::get_mouse_position() const noexcept
{
    return mouse_position_.value_or(nori::core::vec2{});
}

nori::core::vec2 input_state::get_mouse_delta() const noexcept
{
    return mouse_delta_;
}

float input_state::get_scroll_delta() const noexcept
{
    return scroll_delta_;
}

constexpr std::size_t input_state::to_index(nori::core::key_code key) noexcept
{
    return static_cast<std::size_t>(key);
}

constexpr std::size_t input_state::to_index(nori::core::mouse_button button) noexcept
{
    return static_cast<std::size_t>(button);
}

constexpr bool input_state::is_valid(nori::core::key_code key) noexcept
{
    return key > nori::core::key_code::unknown && key < nori::core::key_code::count;
}

constexpr bool input_state::is_valid(nori::core::mouse_button button) noexcept
{
    return button > nori::core::mouse_button::unknown && button < nori::core::mouse_button::count;
}

void input_state::update_mouse_position(int x, int y) noexcept
{
    const nori::core::vec2 position{static_cast<float>(x), static_cast<float>(y)};
    if (mouse_position_)
    {
        mouse_delta_ += position - *mouse_position_;
    }
    mouse_position_ = position;
}

input_state& state() noexcept
{
    static input_state value;
    return value;
}
} // namespace

namespace nori::core::input
{
bool is_key_down(key_code key) noexcept
{
    return state().is_key_down(key);
}

bool is_key_pressed(key_code key) noexcept
{
    return state().is_key_pressed(key);
}

bool is_key_released(key_code key) noexcept
{
    return state().is_key_released(key);
}

bool is_mouse_button_down(mouse_button button) noexcept
{
    return state().is_mouse_button_down(button);
}

bool is_mouse_button_pressed(mouse_button button) noexcept
{
    return state().is_mouse_button_pressed(button);
}

bool is_mouse_button_released(mouse_button button) noexcept
{
    return state().is_mouse_button_released(button);
}

vec2 mouse_position() noexcept
{
    return state().get_mouse_position();
}

vec2 mouse_delta() noexcept
{
    return state().get_mouse_delta();
}

float scroll_delta() noexcept
{
    return state().get_scroll_delta();
}

void update_key_down(key_code key, bool repeat) noexcept
{
    state().update_key_down(key, repeat);
}

void update_key_up(key_code key) noexcept
{
    state().update_key_up(key);
}

void update_mouse_button_down(mouse_button button, int x, int y) noexcept
{
    state().update_mouse_button_down(button, x, y);
}

void update_mouse_button_up(mouse_button button, int x, int y) noexcept
{
    state().update_mouse_button_up(button, x, y);
}

void update_mouse_move(int x, int y) noexcept
{
    state().update_mouse_move(x, y);
}

void update_mouse_wheel(int x, int y, float delta) noexcept
{
    state().update_mouse_wheel(x, y, delta);
}

void reset_mouse_buttons() noexcept
{
    state().reset_mouse_buttons();
}

void clear_frame() noexcept
{
    state().clear_frame();
}

void reset() noexcept
{
    state().reset();
}
} // namespace nori::core::input
