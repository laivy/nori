#include <atomic>
#include <tuple>

#ifdef _WIN32
#include <Windows.h>
#else
#include <csignal>
#endif

#include <nori/core/console_application.hpp>

namespace
{
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> termination_requested{false};

#ifdef _WIN32
BOOL WINAPI handle_termination(DWORD event)
{
    if (event != CTRL_C_EVENT)
    {
        return FALSE;
    }
    termination_requested.store(true, std::memory_order_relaxed);
    return TRUE;
}
#else
void handle_termination(int)
{
    termination_requested.store(true, std::memory_order_relaxed);
}
#endif

class console_environment
{
public:
    console_environment();
    ~console_environment();

    console_environment(const console_environment&) = delete;
    console_environment& operator=(const console_environment&) = delete;

private:
#ifdef _WIN32
    UINT output_code_page_;
    bool changed_code_page_;
    bool registered_handler_;
#else
    struct sigaction interrupt_action_;
    struct sigaction terminate_action_;
    bool registered_interrupt_;
    bool registered_terminate_;
#endif
};

console_environment::console_environment() :
#ifdef _WIN32
    output_code_page_{0},
    changed_code_page_{false},
    registered_handler_{false}
#else
    interrupt_action_{},
    terminate_action_{},
    registered_interrupt_{false},
    registered_terminate_{false}
#endif
{
    termination_requested.store(false, std::memory_order_relaxed);
#ifdef _WIN32
    output_code_page_ = ::GetConsoleOutputCP();
    if (output_code_page_ == 0)
    {
        return;
    }
    if (!::SetConsoleOutputCP(CP_UTF8))
    {
        return;
    }
    changed_code_page_ = true;
    if (!::SetConsoleCtrlHandler(handle_termination, TRUE))
    {
        return;
    }
    registered_handler_ = true;
#else
    struct sigaction action{};
    action.sa_handler = handle_termination;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (::sigaction(SIGINT, &action, &interrupt_action_) != 0)
    {
        return;
    }
    registered_interrupt_ = true;
    if (::sigaction(SIGTERM, &action, &terminate_action_) != 0)
    {
        return;
    }
    registered_terminate_ = true;
#endif
}

console_environment::~console_environment()
{
#ifdef _WIN32
    if (registered_handler_)
    {
        std::ignore = ::SetConsoleCtrlHandler(handle_termination, FALSE);
    }
    if (changed_code_page_)
    {
        std::ignore = ::SetConsoleOutputCP(output_code_page_);
    }
#else
    if (registered_terminate_)
    {
        std::ignore = ::sigaction(SIGTERM, &terminate_action_, nullptr);
    }
    if (registered_interrupt_)
    {
        std::ignore = ::sigaction(SIGINT, &interrupt_action_, nullptr);
    }
#endif
}
} // namespace

namespace nori::core
{
int console_application::run()
{
    if (is_quitting())
    {
        return exit_code();
    }

    const console_environment env;
    return application::run();
}

void console_application::on_platform_begin_frame()
{
    if (termination_requested.load(std::memory_order_relaxed))
    {
        quit(0);
    }
}
} // namespace nori::core
