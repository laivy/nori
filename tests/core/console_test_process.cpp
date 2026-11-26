#include <atomic>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <Windows.h>

#include "console_test_faults.hpp"
#else
#include <csignal>
#endif

#include <nori/core/console_application.hpp>

namespace
{
std::atomic<bool> previous_handler_called{false};
static_assert(std::atomic<bool>::is_always_lock_free);

#ifdef _WIN32
BOOL WINAPI previous_handler(DWORD event)
{
    if (event != CTRL_C_EVENT)
    {
        return FALSE;
    }
    previous_handler_called.store(true);
    return TRUE;
}
#else
void previous_handler(int)
{
    previous_handler_called.store(true);
}
#endif

void request_termination(std::string_view mode)
{
#ifdef _WIN32
    if (!::GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0))
    {
        std::puts("signal_failed");
    }
#else
    ::raise(mode == "terminate" ? SIGTERM : SIGINT);
#endif
    static_cast<void>(mode);
}

class console_test_application final : public nori::core::console_application
{
public:
    explicit console_test_application(std::string_view mode);

    int starts{0};
    int updates{0};
    int stops{0};
    bool same_thread{true};

private:
    void on_start() override;
    void on_update(float) override;
    void on_stop() override;

    std::string_view mode_;
    std::thread::id execution_thread_;
};

console_test_application::console_test_application(std::string_view mode) :
    console_application{{100, 100, 8}},
    mode_{mode},
    execution_thread_{std::this_thread::get_id()}
{
}

void console_test_application::on_start()
{
    ++starts;
    same_thread = same_thread && std::this_thread::get_id() == execution_thread_;
#ifdef _WIN32
    const UINT code_page{::GetConsoleOutputCP()};
    std::printf("utf8=%d\n", code_page == CP_UTF8);
#endif
    std::puts("콘솔 UTF-8 출력");
    if (mode_ == "startup-error")
    {
        quit(1);
    }
    else if (mode_ == "startup-interrupt")
    {
        request_termination(mode_);
#ifdef _WIN32
        std::this_thread::sleep_for(std::chrono::milliseconds{30});
#endif
    }
}

void console_test_application::on_update(float)
{
    ++updates;
    same_thread = same_thread && std::this_thread::get_id() == execution_thread_;
    if (mode_ == "interrupt" || mode_ == "terminate" || mode_ == "repeat" || mode_ == "sequential" || mode_ == "startup-interrupt")
    {
        if (updates == 1)
        {
            if (mode_ != "startup-interrupt")
            {
                request_termination(mode_);
            }
        }
    }
    else if ((mode_ == "frame-arm-error" || mode_ == "frame-wait-error") && updates == 1)
    {
        return;
    }
    else if (mode_ == "custom-exit-notify-error")
    {
        quit(42);
    }
    else
    {
        quit(mode_ == "error" ? 1 : 0);
    }
}

void console_test_application::on_stop()
{
    ++stops;
    same_thread = same_thread && std::this_thread::get_id() == execution_thread_;
    if (mode_ == "repeat")
    {
        request_termination(mode_);
#ifdef _WIN32
        std::this_thread::sleep_for(std::chrono::milliseconds{30});
#endif
    }
    if (mode_ == "cleanup-error")
    {
        quit(1);
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        return 2;
    }

#ifdef _WIN32
    ::SetConsoleCtrlHandler(nullptr, FALSE);
    const UINT initial_code_page{::GetConsoleOutputCP()};
    if (initial_code_page != 0 && !::SetConsoleOutputCP(1252))
    {
        return 3;
    }
    const UINT saved_code_page{::GetConsoleOutputCP()};
    if (!::SetConsoleCtrlHandler(previous_handler, TRUE))
    {
        return 4;
    }
#else
    struct sigaction action{};
    action.sa_handler = previous_handler;
    ::sigemptyset(&action.sa_mask);
    struct sigaction saved_interrupt{};
    struct sigaction saved_terminate{};
    if (::sigaction(SIGINT, &action, &saved_interrupt) != 0 || ::sigaction(SIGTERM, &action, &saved_terminate) != 0)
    {
        return 4;
    }
#endif

    const std::string_view mode{argv[1]};
    console_test_application app{mode};
    if (mode == "early-quit")
    {
        app.quit(1);
    }

#ifdef _WIN32
    if (mode == "custom-exit-notify-error")
    {
        arm_console_fault("frame-notify-error");
    }
    else
    {
        arm_console_fault(mode == "early-quit" ? "query-error" : mode);
    }
#endif

    const auto result{app.run()};
    const auto repeated_result{app.run()};
    std::printf("result=%d starts=%d updates=%d stops=%d same_thread=%d quitting=%d repeated_result=%d\n", result, app.starts, app.updates, app.stops, app.same_thread, app.is_quitting(), repeated_result);
    std::printf("handler_called_during_run=%d\n", previous_handler_called.load());
    if (mode == "sequential")
    {
        console_test_application next{"normal"};
        const auto next_result{next.run()};
        std::printf("next_result=%d next_updates=%d\n", next_result, next.updates);
    }

#ifdef _WIN32
    std::printf("fault_triggered=%d\n", was_console_fault_triggered());
    arm_console_fault({});
    std::printf("restored=%d\n", ::GetConsoleOutputCP() == saved_code_page);
    if (saved_code_page != 0 && mode != "handler-restore-error")
    {
        request_termination("interrupt");
        for (int i{0}; i < 100 && !previous_handler_called.load(); ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        std::printf("handler_restored=%d\n", previous_handler_called.load());
        ::SetConsoleOutputCP(initial_code_page);
    }
    ::SetConsoleCtrlHandler(previous_handler, FALSE);
#else
    ::raise(SIGINT);
    const bool interrupt_restored{previous_handler_called.exchange(false)};
    ::raise(SIGTERM);
    std::printf("restored=%d\n", interrupt_restored && previous_handler_called.load());
    ::sigaction(SIGINT, &saved_interrupt, nullptr);
    ::sigaction(SIGTERM, &saved_terminate, nullptr);
#endif

    return result;
}
