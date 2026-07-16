#include <memory>
#include <utility>
#include <Windows.h>
#include <windowsx.h>
#include "input.hpp"
#include "timer.hpp"
#include "windows_application.hpp"

namespace nori::core
{
namespace
{
windows_application* get_app(HWND hwnd)
{
    return reinterpret_cast<windows_application*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

key_code to_key_code(WPARAM key)
{
    if ('A' <= key && key <= 'Z')
    {
        return static_cast<key_code>(static_cast<int>(key_code::a) + static_cast<int>(key - 'A'));
    }
    if ('0' <= key && key <= '9')
    {
        return static_cast<key_code>(static_cast<int>(key_code::num0) + static_cast<int>(key - '0'));
    }

    switch (key)
    {
    case VK_LEFT:
        return key_code::left;
    case VK_RIGHT:
        return key_code::right;
    case VK_UP:
        return key_code::up;
    case VK_DOWN:
        return key_code::down;
    case VK_ESCAPE:
        return key_code::escape;
    case VK_TAB:
        return key_code::tab;
    case VK_RETURN:
        return key_code::enter;
    case VK_SPACE:
        return key_code::space;
    case VK_BACK:
        return key_code::backspace;
    case VK_DELETE:
        return key_code::delete_key;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return key_code::shift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return key_code::control;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return key_code::alt;
    default:
        break;
    }
    return key_code::unknown;
}

POINT screen_to_client(HWND hwnd, LPARAM l_param)
{
    POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
    ::ScreenToClient(hwnd, &point);
    return point;
}
} // namespace

windows_application::windows_application(specification spec) :
    spec_{std::move(spec)},
    hwnd_{nullptr}
{
    initialize();
}

void windows_application::run()
{
    bool restart{false};
    do
    {
        timer frame_timer{};
        MSG msg{};
        while (true)
        {
            if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    break;
                }
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            else
            {
                tick(frame_timer.tick());
            }
        }

        const application::exit_code result_code{static_cast<application::exit_code>(msg.wParam)};
        switch (result_code)
        {
        case application::exit_code::success:
        {
            restart = false;
            break;
        }
        case application::exit_code::restart:
        {
            std::destroy_at(this);
            std::construct_at(this, spec_);
            restart = true;
            break;
        }
        case application::exit_code::error:
        {
            restart = false;
            break;
        }
        default:
            break;
        }
    } while (restart);
}

void windows_application::quit(application::exit_code exit_code)
{
    ::PostQuitMessage(static_cast<int>(exit_code));
}

HWND windows_application::hwnd() const
{
    return hwnd_;
}

LRESULT windows_application::on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        event e{window_destroy_event{}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        quit(application::exit_code::success);
        return 0;
    }
    case WM_SIZE:
    {
        event e{window_resize_event{LOWORD(l_param), HIWORD(l_param)}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_CLOSE:
    {
        event e{window_close_event{}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        constexpr LPARAM was_down_mask{1ll << 30};
        event e{key_down_event{to_key_code(w_param), (l_param & was_down_mask) != 0}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        event e{key_up_event{to_key_code(w_param)}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
    {
        event e{mouse_move_event{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
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
        event e{mouse_button_down_event{button, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
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
        event e{mouse_button_up_event{button, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    case WM_MOUSEWHEEL:
    {
        const POINT point{screen_to_client(hwnd, l_param)};
        const float delta{static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA)};
        event e{mouse_wheel_event{point.x, point.y, delta}};
        process_event(e);
        if (e.is_handled)
        {
            return 0;
        }
        break;
    }
    default:
        break;
    }
    return ::DefWindowProc(hwnd, message, w_param, l_param);
}

LRESULT windows_application::wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    auto app{get_app(hwnd)};
    if (app)
    {
        return app->on_window_message(hwnd, message, w_param, l_param);
    }
    return ::DefWindowProc(hwnd, message, w_param, l_param);
}

void windows_application::initialize()
{
    constexpr auto window_class_name{L"nori::core::windows_application"};

    WNDCLASSEX wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = wnd_proc;
    wcex.lpszClassName = window_class_name;
    ::RegisterClassEx(&wcex);

    RECT rect{0, 0, spec_.width, spec_.height};
    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    const HWND hwnd{::CreateWindowW(
        window_class_name,
        spec_.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    )};
    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ::ShowWindow(hwnd, SW_SHOW);

    hwnd_ = hwnd;
}

void windows_application::tick(float delta_seconds)
{
    on_tick(delta_seconds);
    clear_frame_input_state();
}
} // namespace nori::core
