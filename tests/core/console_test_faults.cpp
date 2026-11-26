#include <bit>
#include <string_view>

#include <Windows.h>

#include "console_test_faults.hpp"

namespace
{
std::string_view failure_mode;
bool triggered{false};

bool fail_operation(std::string_view mode)
{
    if (failure_mode != mode || triggered)
    {
        return false;
    }
    triggered = true;
    ::SetLastError(ERROR_ACCESS_DENIED);
    return true;
}

BOOL WINAPI set_control_handler(PHANDLER_ROUTINE routine, BOOL add)
{
    if (fail_operation(add ? "register-error" : "handler-restore-error"))
    {
        return FALSE;
    }
    static const auto native{std::bit_cast<decltype(&::SetConsoleCtrlHandler)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetConsoleCtrlHandler"))};
    return native(routine, add);
}

BOOL WINAPI set_code_page(UINT code_page)
{
    if (fail_operation(code_page == CP_UTF8 ? "output-error" : "output-restore-error"))
    {
        return FALSE;
    }
    static const auto native{std::bit_cast<decltype(&::SetConsoleOutputCP)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetConsoleOutputCP"))};
    return native(code_page);
}

UINT WINAPI get_code_page()
{
    if (fail_operation("query-error"))
    {
        return 0;
    }
    static const auto native{std::bit_cast<decltype(&::GetConsoleOutputCP)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "GetConsoleOutputCP"))};
    return native();
}

HANDLE WINAPI create_frame_event(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, BOOL initial_state, LPCWSTR name)
{
    if (fail_operation("frame-event-error"))
    {
        return nullptr;
    }
    static const auto native{std::bit_cast<decltype(&::CreateEventW)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "CreateEventW"))};
    return native(attributes, manual_reset, initial_state, name);
}

HANDLE WINAPI create_frame_timer(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name, DWORD flags, DWORD access)
{
    if (fail_operation("frame-timer-error"))
    {
        return nullptr;
    }
    static const auto native{std::bit_cast<decltype(&::CreateWaitableTimerExW)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "CreateWaitableTimerExW"))};
    return native(attributes, name, flags, access);
}

BOOL WINAPI arm_frame_timer(HANDLE timer, const LARGE_INTEGER* due_time, LONG period, PTIMERAPCROUTINE callback, LPVOID argument, PREASON_CONTEXT context, ULONG tolerance)
{
    if (fail_operation("frame-arm-error"))
    {
        return FALSE;
    }
    static const auto native{std::bit_cast<decltype(&::SetWaitableTimerEx)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetWaitableTimerEx"))};
    return native(timer, due_time, period, callback, argument, context, tolerance);
}

DWORD WINAPI wait_for_frame(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD timeout)
{
    if (fail_operation("frame-wait-error"))
    {
        return WAIT_FAILED;
    }
    static const auto native{std::bit_cast<decltype(&::WaitForMultipleObjects)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "WaitForMultipleObjects"))};
    return native(count, handles, wait_all, timeout);
}

BOOL WINAPI notify_frame_wait(HANDLE event)
{
    if (fail_operation("frame-notify-error"))
    {
        return FALSE;
    }
    static const auto native{std::bit_cast<decltype(&::SetEvent)>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetEvent"))};
    return native(event);
}
} // namespace

// 전용 테스트 프로세스의 OS import만 대체하여 실제 API를 유지한 채 단일 실패 주입
extern "C"
{
decltype(&::SetConsoleCtrlHandler) __imp_SetConsoleCtrlHandler{set_control_handler};
decltype(&::SetConsoleOutputCP) __imp_SetConsoleOutputCP{set_code_page};
decltype(&::GetConsoleOutputCP) __imp_GetConsoleOutputCP{get_code_page};
decltype(&::CreateEventW) __imp_CreateEventW{create_frame_event};
decltype(&::CreateWaitableTimerExW) __imp_CreateWaitableTimerExW{create_frame_timer};
decltype(&::WaitForMultipleObjects) __imp_WaitForMultipleObjects{wait_for_frame};
decltype(&::SetEvent) __imp_SetEvent{notify_frame_wait};

BOOL WINAPI SetWaitableTimerEx(HANDLE timer, const LARGE_INTEGER* due_time, LONG period, PTIMERAPCROUTINE callback, LPVOID argument, PREASON_CONTEXT context, ULONG tolerance)
{
    return arm_frame_timer(timer, due_time, period, callback, argument, context, tolerance);
}
}

void arm_console_fault(std::string_view mode)
{
    failure_mode = mode;
    triggered = false;
}

bool was_console_fault_triggered()
{
    return triggered;
}
