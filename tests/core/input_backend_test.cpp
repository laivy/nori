#include <gtest/gtest.h>

#include <nori/core/input.hpp>

#include "input_internal.hpp"

namespace
{
class input_backend_test : public testing::Test
{
protected:
    void SetUp() override
    {
        nori::core::input::reset();
    }

    void TearDown() override
    {
        nori::core::input::reset();
    }
};

TEST_F(input_backend_test, key_transitions)
{
    nori::core::input::update_key_down(nori::core::key_code::a, false);
    EXPECT_TRUE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_TRUE(nori::core::input::is_key_pressed(nori::core::key_code::a));

    nori::core::input::clear_frame();
    nori::core::input::update_key_down(nori::core::key_code::a, true);
    EXPECT_TRUE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_key_pressed(nori::core::key_code::a));

    nori::core::input::update_key_up(nori::core::key_code::a);
    EXPECT_FALSE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_TRUE(nori::core::input::is_key_released(nori::core::key_code::a));
}

TEST_F(input_backend_test, frame_clear)
{
    nori::core::input::update_key_down(nori::core::key_code::a, false);
    nori::core::input::update_mouse_button_down(nori::core::mouse_button::left, 10, 20);
    nori::core::input::update_mouse_wheel(10, 20, 1.5f);
    nori::core::input::clear_frame();

    EXPECT_TRUE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_key_pressed(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_key_released(nori::core::key_code::a));
    EXPECT_TRUE(nori::core::input::is_mouse_button_down(nori::core::mouse_button::left));
    EXPECT_FALSE(nori::core::input::is_mouse_button_pressed(nori::core::mouse_button::left));
    EXPECT_EQ(nori::core::input::mouse_position().x, 10);
    EXPECT_EQ(nori::core::input::mouse_position().y, 20);
    EXPECT_EQ(nori::core::input::mouse_delta().x, 0);
    EXPECT_EQ(nori::core::input::mouse_delta().y, 0);
    EXPECT_FLOAT_EQ(nori::core::input::scroll_delta(), 0.0f);
}

TEST_F(input_backend_test, mouse_state)
{
    nori::core::input::update_mouse_move(10, 20);
    nori::core::input::update_mouse_move(13, 25);
    nori::core::input::update_mouse_button_down(nori::core::mouse_button::left, 13, 25);
    nori::core::input::update_mouse_wheel(13, 25, 1.5f);

    EXPECT_EQ(nori::core::input::mouse_position().x, 13);
    EXPECT_EQ(nori::core::input::mouse_position().y, 25);
    EXPECT_EQ(nori::core::input::mouse_delta().x, 3);
    EXPECT_EQ(nori::core::input::mouse_delta().y, 5);
    EXPECT_TRUE(nori::core::input::is_mouse_button_down(nori::core::mouse_button::left));
    EXPECT_TRUE(nori::core::input::is_mouse_button_pressed(nori::core::mouse_button::left));
    EXPECT_FLOAT_EQ(nori::core::input::scroll_delta(), 1.5f);

    nori::core::input::update_mouse_button_up(nori::core::mouse_button::left, 14, 27);
    EXPECT_FALSE(nori::core::input::is_mouse_button_down(nori::core::mouse_button::left));
    EXPECT_TRUE(nori::core::input::is_mouse_button_released(nori::core::mouse_button::left));
    EXPECT_EQ(nori::core::input::mouse_delta().x, 4);
    EXPECT_EQ(nori::core::input::mouse_delta().y, 7);
}

TEST_F(input_backend_test, mouse_vectors)
{
    nori::core::input::update_mouse_move(-12, 18);
    nori::core::input::update_mouse_move(-15, 25);

    const nori::core::vec2 position{nori::core::input::mouse_position()};
    const nori::core::vec2 delta{nori::core::input::mouse_delta()};
    EXPECT_EQ(position, (nori::core::vec2{-15.0f, 25.0f}));
    EXPECT_EQ(delta, (nori::core::vec2{-3.0f, 7.0f}));
    EXPECT_EQ(delta * 0.5f, (nori::core::vec2{-1.5f, 3.5f}));
}

TEST_F(input_backend_test, mouse_reset)
{
    nori::core::input::update_key_down(nori::core::key_code::a, false);
    nori::core::input::update_mouse_move(10, 20);
    nori::core::input::update_mouse_button_down(nori::core::mouse_button::left, 10, 20);
    nori::core::input::update_mouse_button_down(nori::core::mouse_button::right, 10, 20);
    nori::core::input::update_mouse_button_up(nori::core::mouse_button::right, 10, 20);

    nori::core::input::reset_mouse_buttons();

    EXPECT_TRUE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_TRUE(nori::core::input::is_key_pressed(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_mouse_button_down(nori::core::mouse_button::left));
    EXPECT_FALSE(nori::core::input::is_mouse_button_pressed(nori::core::mouse_button::left));
    EXPECT_FALSE(nori::core::input::is_mouse_button_released(nori::core::mouse_button::right));
    EXPECT_EQ(nori::core::input::mouse_position().x, 10);
    EXPECT_EQ(nori::core::input::mouse_position().y, 20);
}

TEST_F(input_backend_test, reset)
{
    nori::core::input::update_key_down(nori::core::key_code::a, false);
    nori::core::input::update_mouse_button_down(nori::core::mouse_button::left, 10, 20);
    nori::core::input::update_mouse_wheel(12, 23, 1.0f);
    nori::core::input::reset();

    EXPECT_FALSE(nori::core::input::is_key_down(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_key_pressed(nori::core::key_code::a));
    EXPECT_FALSE(nori::core::input::is_mouse_button_down(nori::core::mouse_button::left));
    EXPECT_EQ(nori::core::input::mouse_position().x, 0);
    EXPECT_EQ(nori::core::input::mouse_position().y, 0);
    EXPECT_EQ(nori::core::input::mouse_delta().x, 0);
    EXPECT_EQ(nori::core::input::mouse_delta().y, 0);
    EXPECT_FLOAT_EQ(nori::core::input::scroll_delta(), 0.0f);
}

TEST_F(input_backend_test, initial_mouse_delta)
{
    nori::core::input::update_mouse_move(10, 20);
    nori::core::input::update_mouse_move(13, 25);
    nori::core::input::reset();
    nori::core::input::update_mouse_move(100, 200);

    EXPECT_EQ(nori::core::input::mouse_position().x, 100);
    EXPECT_EQ(nori::core::input::mouse_position().y, 200);
    EXPECT_EQ(nori::core::input::mouse_delta().x, 0);
    EXPECT_EQ(nori::core::input::mouse_delta().y, 0);

    nori::core::input::update_mouse_move(101, 198);
    EXPECT_EQ(nori::core::input::mouse_delta().x, 1);
    EXPECT_EQ(nori::core::input::mouse_delta().y, -2);
}
} // namespace
