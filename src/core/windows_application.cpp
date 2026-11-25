#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <Windows.h>
#include <windowsx.h>

#include <nori/core/abort.hpp>
#include <nori/core/encoding.hpp>
#include <nori/core/windows_application.hpp>

#include "input_internal.hpp"

namespace
{
constexpr char32_t replacement_character{U'\uFFFD'};

nori::core::key_code to_key_code(LPARAM l_param) noexcept
{
    const WORD key_data{HIWORD(l_param)};
    const BYTE scan_code{LOBYTE(key_data)};

    // 키보드 상단 숫자 키(1 ~ 0)
    if (0x02 <= scan_code && scan_code <= 0x0A)
    {
        return nori::core::key_code{std::to_underlying(nori::core::key_code::num1) + static_cast<int>(scan_code - 0x02)};
    }

    if (const bool is_extended_key{(key_data & KF_EXTENDED) != 0})
    {
        switch (scan_code)
        {
        case 0x48:
        {
            return nori::core::key_code::up;
        }
        case 0x4B:
        {
            return nori::core::key_code::left;
        }
        case 0x4D:
        {
            return nori::core::key_code::right;
        }
        case 0x50:
        {
            return nori::core::key_code::down;
        }
        case 0x53:
        {
            return nori::core::key_code::delete_key;
        }
        default:
            break;
        }
    }

    switch (scan_code)
    {
    case 0x01:
    {
        return nori::core::key_code::escape;
    }
    case 0x0B:
    {
        return nori::core::key_code::num0;
    }
    case 0x0E:
    {
        return nori::core::key_code::backspace;
    }
    case 0x0F:
    {
        return nori::core::key_code::tab;
    }
    case 0x10:
    {
        return nori::core::key_code::q;
    }
    case 0x11:
    {
        return nori::core::key_code::w;
    }
    case 0x12:
    {
        return nori::core::key_code::e;
    }
    case 0x13:
    {
        return nori::core::key_code::r;
    }
    case 0x14:
    {
        return nori::core::key_code::t;
    }
    case 0x15:
    {
        return nori::core::key_code::y;
    }
    case 0x16:
    {
        return nori::core::key_code::u;
    }
    case 0x17:
    {
        return nori::core::key_code::i;
    }
    case 0x18:
    {
        return nori::core::key_code::o;
    }
    case 0x19:
    {
        return nori::core::key_code::p;
    }
    case 0x1C:
    {
        return nori::core::key_code::enter;
    }
    case 0x1D:
    {
        return (key_data & KF_EXTENDED) != 0 ? nori::core::key_code::right_control : nori::core::key_code::left_control;
    }
    case 0x1E:
    {
        return nori::core::key_code::a;
    }
    case 0x1F:
    {
        return nori::core::key_code::s;
    }
    case 0x20:
    {
        return nori::core::key_code::d;
    }
    case 0x21:
    {
        return nori::core::key_code::f;
    }
    case 0x22:
    {
        return nori::core::key_code::g;
    }
    case 0x23:
    {
        return nori::core::key_code::h;
    }
    case 0x24:
    {
        return nori::core::key_code::j;
    }
    case 0x25:
    {
        return nori::core::key_code::k;
    }
    case 0x26:
    {
        return nori::core::key_code::l;
    }
    case 0x2A:
    {
        return nori::core::key_code::left_shift;
    }
    case 0x36:
    {
        return nori::core::key_code::right_shift;
    }
    case 0x2C:
    {
        return nori::core::key_code::z;
    }
    case 0x2D:
    {
        return nori::core::key_code::x;
    }
    case 0x2E:
    {
        return nori::core::key_code::c;
    }
    case 0x2F:
    {
        return nori::core::key_code::v;
    }
    case 0x30:
    {
        return nori::core::key_code::b;
    }
    case 0x31:
    {
        return nori::core::key_code::n;
    }
    case 0x32:
    {
        return nori::core::key_code::m;
    }
    case 0x38:
    {
        return (key_data & KF_EXTENDED) != 0 ? nori::core::key_code::right_alt : nori::core::key_code::left_alt;
    }
    case 0x39:
    {
        return nori::core::key_code::space;
    }
    default:
        break;
    }
    return nori::core::key_code::unknown;
}

nori::core::windows_application* app(HWND hwnd)
{
    return reinterpret_cast<nori::core::windows_application*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

POINT screen_to_client(HWND hwnd, LPARAM l_param)
{
    POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
    ::ScreenToClient(hwnd, &point);
    return point;
}

constexpr char32_t combine_surrogates(char16_t high, char16_t low) noexcept
{
    return 0x10000u + ((static_cast<char32_t>(high) - 0xD800u) << 10u) + (static_cast<char32_t>(low) - 0xDC00u);
}
} // namespace

namespace nori::core
{
windows_application::windows_application(const specification& spec) :
    application{{spec.target_frame_rate, spec.fixed_update_rate, spec.max_fixed_updates_per_frame}},
    spec_{spec},
    hwnd_{nullptr},
    finished_{false},
    minimized_{false},
    mouse_buttons_{},
    pending_high_surrogate_{0}
{
}

int windows_application::run()
{
    if (is_quitting())
    {
        return exit_code();
    }
    if (!initialize())
    {
        quit(1);
        return exit_code();
    }

    input::reset();
    std::jthread frame_thread{[this]()
    {
        std::ignore = application::run();
        finished_.store(true);
        ::PostMessageW(hwnd(), WM_NULL, 0, 0);
    }};

    MSG message{};
    while (!finished_.load())
    {
        const BOOL received{::GetMessageW(&message, nullptr, 0, 0)};
        if (received <= 0)
        {
            quit(received < 0 ? 1 : static_cast<int>(message.wParam));
            break;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    frame_thread.join();
    input::reset();
    shutdown();
    return exit_code();
}

HWND windows_application::hwnd() const noexcept
{
    return hwnd_;
}

void windows_application::on_render()
{
}

LRESULT windows_application::on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

void windows_application::on_window_resize(int, int)
{
}

void windows_application::on_window_focus(bool)
{
}

void windows_application::on_window_close()
{
}

void windows_application::on_key_down(key_code, bool)
{
}

void windows_application::on_key_up(key_code)
{
}

void windows_application::on_text_input(char32_t)
{
}

void windows_application::on_mouse_move(int, int)
{
}

void windows_application::on_mouse_button_down(mouse_button, int, int)
{
}

void windows_application::on_mouse_button_up(mouse_button, int, int)
{
}

void windows_application::on_mouse_wheel(int, int, float)
{
}

LRESULT windows_application::wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (windows_application* const instance{app(hwnd)})
    {
        return instance->process_window_message(hwnd, message, w_param, l_param);
    }
    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT windows_application::process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_SYSCOMMAND:
    {
        if (!spec_.resizable && ((w_param & 0xFFF0) == SC_SIZE || (w_param & 0xFFF0) == SC_MAXIMIZE))
        {
            return 0;
        }
        break;
    }
    case WM_SETFOCUS:
    {
        enqueue_event([this]()
        {
            on_window_focus(true);
        });
        break;
    }
    case WM_KILLFOCUS:
    {
        mouse_buttons_.fill(false);
        if (::GetCapture() == hwnd)
        {
            ::ReleaseCapture();
        }
        pending_high_surrogate_ = 0;
        enqueue_event([this]()
        {
            input::reset();
            on_window_focus(false);
        });
        break;
    }
    case WM_DPICHANGED:
    {
        const RECT suggested{*reinterpret_cast<const RECT*>(l_param)};
        ::SetWindowPos(hwnd, nullptr, suggested.left, suggested.top, suggested.right - suggested.left, suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SIZE:
    {
        // 프레임당 1회로 합침, 마지막 크기만 사용
        const bool minimized{w_param == SIZE_MINIMIZED};
        const int width{static_cast<int>(LOWORD(l_param))};
        const int height{static_cast<int>(HIWORD(l_param))};
        const std::lock_guard lock{mutex_};
        size_ = size_motion{width, height, minimized};
        break;
    }
    case WM_CLOSE:
    {
        enqueue_event([this]()
        {
            on_window_close();
        });
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        const bool is_repeat{(HIWORD(l_param) & KF_REPEAT) != 0};
        const key_code key{to_key_code(l_param)};
        enqueue_event([this, key, is_repeat]()
        {
            input::update_key_down(key, is_repeat);
            on_key_down(key, is_repeat);
        });
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        const key_code key{to_key_code(l_param)};
        enqueue_event([this, key]()
        {
            input::update_key_up(key);
            on_key_up(key);
        });
        break;
    }
    case WM_CHAR:
    {
        const char16_t character{static_cast<char16_t>(w_param)};
        const char16_t high{std::exchange(pending_high_surrogate_, char16_t{})};
        if (high != 0)
        {
            if (IS_LOW_SURROGATE(character))
            {
                dispatch_text_input(combine_surrogates(high, character));
                break;
            }
            dispatch_text_input(replacement_character);
        }

        if (IS_HIGH_SURROGATE(character))
        {
            pending_high_surrogate_ = character;
        }
        else
        {
            dispatch_text_input(IS_LOW_SURROGATE(character) ? replacement_character : character);
        }
        break;
    }
    case WM_UNICHAR:
    {
        if (w_param == UNICODE_NOCHAR)
        {
            return TRUE;
        }
        dispatch_text_input(static_cast<char32_t>(w_param));
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        // 프레임당 1회로 합침, 최종 좌표만 사용
        const int x{GET_X_LPARAM(l_param)};
        const int y{GET_Y_LPARAM(l_param)};
        const std::lock_guard lock{mutex_};
        mouse_ = mouse_motion{x, y};
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        if (::GetCapture() != hwnd)
        {
            ::SetCapture(hwnd);
        }

        mouse_button button{mouse_button::unknown};
        if (message == WM_LBUTTONDOWN)
        {
            button = mouse_button::left;
        }
        else if (message == WM_RBUTTONDOWN)
        {
            button = mouse_button::right;
        }
        else if (message == WM_MBUTTONDOWN)
        {
            button = mouse_button::middle;
        }
        mouse_buttons_[static_cast<std::size_t>(button) - 1] = true;
        const int x{GET_X_LPARAM(l_param)};
        const int y{GET_Y_LPARAM(l_param)};
        enqueue_event([this, button, x, y]()
        {
            input::update_mouse_button_down(button, x, y);
            on_mouse_button_down(button, x, y);
        });
        break;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        mouse_button button{mouse_button::unknown};
        if (message == WM_LBUTTONUP)
        {
            button = mouse_button::left;
        }
        else if (message == WM_RBUTTONUP)
        {
            button = mouse_button::right;
        }
        else if (message == WM_MBUTTONUP)
        {
            button = mouse_button::middle;
        }
        mouse_buttons_[static_cast<std::size_t>(button) - 1] = false;
        const int x{GET_X_LPARAM(l_param)};
        const int y{GET_Y_LPARAM(l_param)};
        enqueue_event([this, button, x, y]()
        {
            input::update_mouse_button_up(button, x, y);
            on_mouse_button_up(button, x, y);
        });
        if (!std::ranges::contains(mouse_buttons_, true) && ::GetCapture() == hwnd)
        {
            ::ReleaseCapture();
        }
        break;
    }
    case WM_CAPTURECHANGED:
    {
        if (std::ranges::contains(mouse_buttons_, true))
        {
            mouse_buttons_.fill(false);
            enqueue_event([]()
            {
                input::reset_mouse_buttons();
            });
        }
        break;
    }
    case WM_MOUSEWHEEL:
    {
        // 위치는 최신값, 델타는 누적으로 합침
        const POINT point{screen_to_client(hwnd, l_param)};
        const float delta{static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA)};
        const std::lock_guard lock{mutex_};
        if (wheel_)
        {
            wheel_->x = point.x;
            wheel_->y = point.y;
            wheel_->delta += delta;
        }
        else
        {
            wheel_ = wheel_motion{point.x, point.y, delta};
        }
        break;
    }
    default:
        break;
    }
    return on_window_message(hwnd, message, w_param, l_param);
}

bool windows_application::initialize()
{
    constexpr auto window_class_name{L"nori::core::windows_application"};

    const auto title{utf8_to_utf16(spec_.title)};
    if (!spec_.title.empty() && title.empty())
    {
        return false;
    }
    const HINSTANCE instance{::GetModuleHandleW(nullptr)};
    if (!instance)
    {
        return false;
    }

    const WNDCLASSEXW wcex{
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = wnd_proc,
        .hInstance = instance,
        .lpszClassName = window_class_name,
    };
    if (!::RegisterClassExW(&wcex) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    const DWORD style{static_cast<DWORD>(spec_.resizable ? WS_OVERLAPPEDWINDOW : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)};
    RECT rect{0, 0, spec_.width, spec_.height};
    if (!::AdjustWindowRect(&rect, style, FALSE))
    {
        return false;
    }

    const HWND hwnd{::CreateWindowW(
        window_class_name,
        title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr
    )};
    if (!hwnd)
    {
        return false;
    }

    ::SetLastError(ERROR_SUCCESS);
    if (!::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)) && ::GetLastError() != ERROR_SUCCESS)
    {
        ::DestroyWindow(hwnd);
        return false;
    }

    hwnd_ = hwnd;
    return true;
}

void windows_application::shutdown()
{
    if (const HWND window{hwnd()})
    {
        ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        ::DestroyWindow(window);
        hwnd_ = nullptr;
    }
}

void windows_application::on_platform_start()
{
    ::ShowWindowAsync(hwnd(), SW_SHOW);
}

void windows_application::on_platform_begin_frame()
{
    std::vector<std::move_only_function<void()>> events;
    std::optional<mouse_motion> mouse;
    std::optional<size_motion> size;
    std::optional<wheel_motion> wheel;
    {
        const std::lock_guard lock{mutex_};
        events.reserve(events_.size());
        while (!events_.empty())
        {
            events.push_back(std::move(events_.front()));
            events_.pop();
        }
        mouse = std::exchange(mouse_, std::nullopt);
        size = std::exchange(size_, std::nullopt);
        wheel = std::exchange(wheel_, std::nullopt);
    }

    for (auto& event : events)
    {
        event();
    }
    if (size)
    {
        minimized_ = size->minimized;
        if (!size->minimized)
        {
            on_window_resize(size->width, size->height);
        }
    }
    if (mouse)
    {
        input::update_mouse_move(mouse->x, mouse->y);
        on_mouse_move(mouse->x, mouse->y);
    }
    if (wheel)
    {
        input::update_mouse_wheel(wheel->x, wheel->y, wheel->delta);
        on_mouse_wheel(wheel->x, wheel->y, wheel->delta);
    }
}

void windows_application::on_platform_end_frame()
{
    if (!minimized_)
    {
        on_render();
    }
    input::clear_frame();
}

void windows_application::enqueue_event(std::move_only_function<void()> callback)
{
    const std::lock_guard lock{mutex_};
    if (events_.size() >= max_events_)
    {
        core::abort("windows_application event queue exhausted");
    }
    events_.push(std::move(callback));
}

void windows_application::dispatch_text_input(char32_t character)
{
    enqueue_event([this, character]()
    {
        on_text_input(character);
    });
}
} // namespace nori::core
