#include <array>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <nori/core/application.hpp>
#include <nori/core/input.hpp>

#if defined(_WIN32)
#include <nori/core/windows_application.hpp>
#endif

#include "application_test_helpers.hpp"

namespace
{
#if defined(_WIN32)
// 화면에 표시하지 않는 테스트용 창
// ShowWindow 요청의 SWP_SHOWWINDOW 플래그 제거
class hidden_windows_application : public nori::core::windows_application
{
public:
    using windows_application::windows_application;

protected:
    LRESULT on_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param) override
    {
        if (message == WM_WINDOWPOSCHANGING)
        {
            const auto position{reinterpret_cast<WINDOWPOS*>(l_param)};
            position->flags &= ~SWP_SHOWWINDOW;
        }
        else if (message == WM_SHOWWINDOW && w_param != 0)
        {
            // 위 veto가 동작하면 표시 요청은 여기에 도달하지 않는다. 회귀 감지용.
            EXPECT_FALSE(::IsWindowVisible(window));
        }
        return windows_application::on_window_message(window, message, w_param, l_param);
    }
};

class completed_windows_application final : public completed_frame_application<hidden_windows_application>
{
public:
    completed_windows_application() :
        completed_frame_application{{"completed frame", 64, 64, false, 100, 100, 3}},
        renders{0},
        begins{0},
        ends{0},
        caller_thread{std::this_thread::get_id()}
    {
    }

    int renders;
    int begins;
    int ends;
    std::thread::id caller_thread;
    std::thread::id frame_thread;

protected:
    void on_begin_frame() override
    {
        EXPECT_EQ(begins++, ends);
        EXPECT_EQ(renders, updates);
    }

    void on_start() override
    {
        EXPECT_NE(hwnd(), nullptr);
        frame_thread = std::this_thread::get_id();
        EXPECT_NE(frame_thread, caller_thread);
    }

    void on_fixed_update(float delta_seconds) override
    {
        EXPECT_EQ(std::this_thread::get_id(), frame_thread);
        completed_frame_application::on_fixed_update(delta_seconds);
    }

    void on_update(float delta_seconds) override
    {
        EXPECT_EQ(std::this_thread::get_id(), frame_thread);
        completed_frame_application::on_update(delta_seconds);
    }

    void on_render() override
    {
        EXPECT_EQ(std::this_thread::get_id(), frame_thread);
        EXPECT_NE(hwnd(), nullptr);
        EXPECT_EQ(++renders, updates);
        if (updates == 2)
        {
            EXPECT_TRUE(is_quitting());
            EXPECT_EQ(final_fixed_updates, 3);
        }
    }

    void on_end_frame() override
    {
        EXPECT_EQ(++ends, begins);
        EXPECT_EQ(renders, updates);
        EXPECT_GT(updates, 0);
    }

    void on_stop() override
    {
        EXPECT_EQ(std::this_thread::get_id(), frame_thread);
        EXPECT_NE(hwnd(), nullptr);
        EXPECT_EQ(renders, 2);
        completed_frame_application::on_stop();
    }
};

TEST(windows_application_test, final_frame)
{
    completed_windows_application app;
    expect_frame_completed_after_fixed_update_quit(app);
    EXPECT_EQ(app.renders, 2);
    EXPECT_EQ(app.begins, 2);
    EXPECT_EQ(app.ends, 2);
    EXPECT_EQ(app.hwnd(), nullptr);
}

TEST(windows_application_test, early_quit)
{
    hidden_windows_application app{{"early quit", 64, 64, false, 60, 60, 8}};
    app.quit(1);
    EXPECT_EQ(app.run(), 1);
    EXPECT_EQ(app.hwnd(), nullptr);
}

struct modifier_test_case
{
    nori::core::key_code left_key;
    nori::core::key_code right_key;
    WPARAM virtual_key;
    LPARAM left;
    LPARAM right;
    UINT down_message;
    UINT up_message;
};

class modifier_windows_application final : public hidden_windows_application
{
public:
    modifier_windows_application(const modifier_test_case& test_case, bool right_first) :
        hidden_windows_application{{"modifier input", 64, 64, false, 0, 100, 3}},
        test_case_{test_case},
        first_{right_first ? test_case.right : test_case.left},
        second_{right_first ? test_case.left : test_case.right},
        first_key_{right_first ? test_case.right_key : test_case.left_key},
        second_key_{right_first ? test_case.left_key : test_case.right_key},
        frame_{0},
        key_downs_{0},
        key_ups_{0},
        repeats_{0}
    {
    }

protected:
    void on_start() override
    {
        // 큐 진입 전 외부 키/포커스 메시지 차단
        original_window_proc_ = reinterpret_cast<WNDPROC>(::GetWindowLongPtrW(hwnd(), GWLP_WNDPROC));
        ::SetWindowLongPtrW(hwnd(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&isolated_window_proc));
        ::EnableWindow(hwnd(), FALSE);
        // 합성 입력 상태와 이벤트 횟수에 대한 외부 메시지 차단
        EXPECT_TRUE(::PostMessageW(hwnd(), WM_KEYDOWN, VK_SHIFT, 1 | (LPARAM{0x2A} << 16)));
        EXPECT_TRUE(::PostMessageW(hwnd(), WM_KEYUP, VK_SHIFT, 1 | (LPARAM{0x2A | KF_REPEAT | KF_UP} << 16)));
        EXPECT_TRUE(::PostMessageW(hwnd(), WM_KILLFOCUS, 0, 0));
    }

    void on_stop() override
    {
        ::SetWindowLongPtrW(hwnd(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_window_proc_));
    }

    LRESULT on_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param) override
    {
        // 합성 입력을 desktop focus와 시스템 Alt 처리로부터 분리
        if (message == WM_WINDOWPOSCHANGING)
        {
            const auto position{reinterpret_cast<WINDOWPOS*>(l_param)};
            position->flags &= ~SWP_SHOWWINDOW;
        }
        if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
        {
            return 0;
        }
        return hidden_windows_application::on_window_message(window, message, w_param, l_param);
    }

    void on_key_down(nori::core::key_code key, bool repeat) override
    {
        EXPECT_TRUE(key == first_key_ || key == second_key_) << std::to_underlying(key);
        EXPECT_TRUE(nori::core::input::is_key_down(key));
        ++key_downs_;
        if (repeat)
        {
            ++repeats_;
        }
    }

    void on_key_up(nori::core::key_code key) override
    {
        EXPECT_TRUE(key == first_key_ || key == second_key_) << std::to_underlying(key);
        EXPECT_FALSE(nori::core::input::is_key_down(key));
        ++key_ups_;
    }

    void on_window_focus(bool focused) override
    {
        if (!focused)
        {
            expect_state(first_key_, false, false, false);
            expect_state(second_key_, false, false, false);
        }
    }

    void on_update(float) override
    {
        SCOPED_TRACE(frame_);
        switch (frame_++)
        {
        case 0:
        {
            reset_focus();
            send_key(true, first_);
            break;
        }
        case 1:
        {
            expect_state(first_key_, true, true, false);
            expect_state(second_key_, false, false, false);
            EXPECT_EQ(key_downs_, 1);
            send_key(true, first_, true);
            break;
        }
        case 2:
        {
            expect_state(first_key_, true, false, false);
            expect_state(second_key_, false, false, false);
            EXPECT_EQ(repeats_, 1);
            send_key(true, second_);
            break;
        }
        case 3:
        {
            expect_state(first_key_, true, false, false);
            expect_state(second_key_, true, true, false);
            EXPECT_EQ(key_downs_, 3);
            send_key(false, first_);
            send_key(false, first_);
            break;
        }
        case 4:
        {
            expect_state(first_key_, false, false, true);
            expect_state(second_key_, true, false, false);
            EXPECT_EQ(key_ups_, 2);
            send_key(true, second_, true);
            break;
        }
        case 5:
        {
            expect_state(first_key_, false, false, false);
            expect_state(second_key_, true, false, false);
            EXPECT_EQ(repeats_, 2);
            send_key(false, second_);
            break;
        }
        case 6:
        {
            expect_state(first_key_, false, false, false);
            expect_state(second_key_, false, false, true);
            EXPECT_EQ(key_ups_, 3);
            send_key(true, first_);
            send_key(true, second_);
            reset_focus();
            send_key(true, second_);
            break;
        }
        case 7:
        {
            expect_state(first_key_, false, false, false);
            expect_state(second_key_, true, true, false);
            send_key(false, second_);
            break;
        }
        case 8:
        {
            expect_state(first_key_, false, false, false);
            expect_state(second_key_, false, false, true);
            EXPECT_EQ(key_ups_, 4);
            EXPECT_EQ(key_downs_, 7);
            quit(0);
            break;
        }
        default:
            ADD_FAILURE() << "Unexpected frame";
            quit(1);
            break;
        }
    }

private:
    static LRESULT CALLBACK isolated_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
    {
        const auto instance{reinterpret_cast<modifier_windows_application*>(::GetWindowLongPtrW(window, GWLP_USERDATA))};
        if (message == synthetic_message)
        {
            const auto input{reinterpret_cast<const MSG*>(l_param)};
            return ::CallWindowProcW(instance->original_window_proc_, window, input->message, input->wParam, input->lParam);
        }
        if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SETFOCUS || message == WM_KILLFOCUS)
        {
            return 0;
        }
        return ::CallWindowProcW(instance->original_window_proc_, window, message, w_param, l_param);
    }

    void send_message(UINT message, WPARAM w_param, LPARAM l_param) const
    {
        const MSG input{hwnd(), message, w_param, l_param};
        ::SendMessageW(hwnd(), synthetic_message, 0, reinterpret_cast<LPARAM>(&input));
    }

    void expect_state(nori::core::key_code key, bool down, bool pressed, bool released) const
    {
        EXPECT_EQ(nori::core::input::is_key_down(key), down);
        EXPECT_EQ(nori::core::input::is_key_pressed(key), pressed);
        EXPECT_EQ(nori::core::input::is_key_released(key), released);
    }

    void send_key(bool down, LPARAM key_data, bool repeat = false) const
    {
        if (repeat || !down)
        {
            key_data |= LPARAM{KF_REPEAT} << 16;
        }
        if (!down)
        {
            key_data |= LPARAM{KF_UP} << 16;
        }
        send_message(down ? test_case_.down_message : test_case_.up_message, test_case_.virtual_key, key_data);
    }

    void reset_focus() const
    {
        send_message(WM_KILLFOCUS, 0, 0);
        send_message(WM_SETFOCUS, 0, 0);
    }

    static constexpr UINT synthetic_message{WM_APP + 1};

    WNDPROC original_window_proc_{};
    modifier_test_case test_case_;
    LPARAM first_;
    LPARAM second_;
    nori::core::key_code first_key_;
    nori::core::key_code second_key_;
    int frame_;
    int key_downs_;
    int key_ups_;
    int repeats_;
};

TEST(windows_application_test, modifier_keys)
{
    constexpr std::array<modifier_test_case, 3> cases{
        modifier_test_case{nori::core::key_code::left_shift, nori::core::key_code::right_shift, VK_SHIFT, 1 | (LPARAM{0x2A} << 16), 1 | (LPARAM{0x36} << 16), WM_KEYDOWN, WM_KEYUP},
        modifier_test_case{nori::core::key_code::left_control, nori::core::key_code::right_control, VK_CONTROL, 1 | (LPARAM{0x1D} << 16), 1 | (LPARAM{0x1D | KF_EXTENDED} << 16), WM_KEYDOWN, WM_KEYUP},
        modifier_test_case{nori::core::key_code::left_alt, nori::core::key_code::right_alt, VK_MENU, 1 | (LPARAM{0x38 | KF_ALTDOWN} << 16), 1 | (LPARAM{0x38 | KF_EXTENDED | KF_ALTDOWN} << 16), WM_SYSKEYDOWN, WM_SYSKEYUP}
    };
    for (const auto& test_case : cases)
    {
        SCOPED_TRACE(std::to_underlying(test_case.left_key));
        for (const bool right_first : {false, true})
        {
            SCOPED_TRACE(right_first);
            modifier_windows_application app{test_case, right_first};
            EXPECT_EQ(app.run(), 0);
            EXPECT_FALSE(nori::core::input::is_key_down(test_case.left_key));
            EXPECT_FALSE(nori::core::input::is_key_down(test_case.right_key));
        }
    }
}

class coalescing_windows_application final : public hidden_windows_application
{
public:
    coalescing_windows_application() :
        hidden_windows_application{{"motion coalescing", 64, 64, false, 0, 100, 0}},
        updates_{0},
        moves_{0},
        resizes_{0},
        wheels_{0},
        key_downs_{0},
        last_x_{0},
        last_y_{0},
        last_w_{0},
        last_h_{0},
        wheel_delta_{0.0f},
        base_x_{0.0f},
        base_y_{0.0f}
    {
    }

    int updates_;
    int moves_;
    int resizes_;
    int wheels_;
    int key_downs_;
    int last_x_;
    int last_y_;
    int last_w_;
    int last_h_;
    float wheel_delta_;
    float base_x_;
    float base_y_;

protected:
    void on_mouse_move(int x, int y) override
    {
        ++moves_;
        last_x_ = x;
        last_y_ = y;
    }

    void on_window_resize(int width, int height) override
    {
        ++resizes_;
        last_w_ = width;
        last_h_ = height;
    }

    void on_mouse_wheel(int, int, float delta) override
    {
        ++wheels_;
        wheel_delta_ += delta;
    }

    void on_key_down(nori::core::key_code key, bool) override
    {
        if (key == nori::core::key_code::a)
        {
            ++key_downs_;
        }
    }

    void on_update(float) override
    {
        if (updates_ == 0)
        {
            // 시작 시점 메시지와 함께 기준 좌표 확정, 다음 프레임부터 델타 완전 텔레스코핑
            ::SendMessageW(hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(5, 5));
        }
        else if (updates_ == 1)
        {
            moves_ = 0;
            resizes_ = 0;
            wheels_ = 0;
            key_downs_ = 0;
            wheel_delta_ = 0.0f;
            base_x_ = nori::core::input::mouse_position().x;
            base_y_ = nori::core::input::mouse_position().y;
            for (int i{0}; i < 10; ++i)
            {
                ::SendMessageW(hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(10 + i, 20 + 2 * i));
            }
            ::SendMessageW(hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(100, 80));
            ::SendMessageW(hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(120, 90));
            ::SendMessageW(hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(140, 100));
            ::SendMessageW(hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0, 120), MAKELPARAM(200, 200));
            ::SendMessageW(hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0, 240), MAKELPARAM(200, 200));
            ::SendMessageW(hwnd(), WM_KEYDOWN, 'A', 1 | (LPARAM{0x1E} << 16));
        }
        else if (updates_ == 2)
        {
            EXPECT_EQ(moves_, 1);
            EXPECT_EQ(last_x_, 19);
            EXPECT_EQ(last_y_, 38);
            EXPECT_EQ(resizes_, 1);
            EXPECT_EQ(last_w_, 140);
            EXPECT_EQ(last_h_, 100);
            EXPECT_EQ(wheels_, 1);
            EXPECT_FLOAT_EQ(wheel_delta_, 3.0f);
            EXPECT_EQ(key_downs_, 1);
            EXPECT_TRUE(nori::core::input::is_key_down(nori::core::key_code::a));
            const auto pos{nori::core::input::mouse_position()};
            const auto delta{nori::core::input::mouse_delta()};
            EXPECT_EQ(delta.x, pos.x - base_x_);
            EXPECT_EQ(delta.y, pos.y - base_y_);
            EXPECT_FLOAT_EQ(nori::core::input::scroll_delta(), 3.0f);
        }
        else
        {
            EXPECT_EQ(moves_, 1);
            EXPECT_EQ(resizes_, 1);
            EXPECT_EQ(wheels_, 1);
            quit(0);
        }
        ++updates_;
    }
};

TEST(windows_application_test, motion_coalescing)
{
    coalescing_windows_application app;
    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.moves_, 1);
    EXPECT_EQ(app.resizes_, 1);
    EXPECT_EQ(app.wheels_, 1);
    EXPECT_EQ(app.key_downs_, 1);
}

#endif
} // namespace
