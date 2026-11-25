#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#ifdef _WIN32
#include <Windows.h>

#include <nori/core/encoding.hpp>
#else
#include <fcntl.h>
#include <unistd.h>

#include <csignal>

#include <sys/wait.h>
#endif

namespace
{
struct process_result
{
    int code;
    bool completed;
    std::string output;
};

process_result run_console_process(std::string_view mode, bool has_console = true)
{
    process_result result{-1, false, {}};
#ifdef _WIN32
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE read_pipe{nullptr};
    HANDLE write_pipe{nullptr};
    if (!::CreatePipe(&read_pipe, &write_pipe, &attributes, 0))
    {
        return result;
    }
    ::SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = nullptr;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    PROCESS_INFORMATION process{};
    std::wstring command{L"\"" + nori::core::utf8_to_utf16(NORI_CONSOLE_TEST_PROCESS) + L"\" " + nori::core::utf8_to_utf16(mode)};
    const BOOL created{::CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, has_console ? CREATE_NEW_CONSOLE : DETACHED_PROCESS, nullptr, nullptr, &startup, &process)};
    ::CloseHandle(write_pipe);
    if (!created)
    {
        ::CloseHandle(read_pipe);
        return result;
    }
    result.completed = ::WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0;
    if (!result.completed)
    {
        ::TerminateProcess(process.hProcess, 99);
        ::WaitForSingleObject(process.hProcess, INFINITE);
    }
    DWORD code{0};
    ::GetExitCodeProcess(process.hProcess, &code);
    result.code = static_cast<int>(code);
    std::array<char, 1024> buffer{};
    DWORD count{0};
    while (::ReadFile(read_pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count != 0)
    {
        result.output.append(buffer.data(), count);
    }
    ::CloseHandle(read_pipe);
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
#else
    static_cast<void>(has_console);
    int descriptors[2]{};
    if (::pipe(descriptors) != 0)
    {
        return result;
    }
    const std::string argument{mode};
    const pid_t child{::fork()};
    if (child == 0)
    {
        ::close(descriptors[0]);
        ::dup2(descriptors[1], STDOUT_FILENO);
        ::dup2(descriptors[1], STDERR_FILENO);
        ::close(descriptors[1]);
        const int input{::open("/dev/null", O_RDONLY)};
        ::dup2(input, STDIN_FILENO);
        ::close(input);
        ::execl(NORI_CONSOLE_TEST_PROCESS, NORI_CONSOLE_TEST_PROCESS, argument.c_str(), nullptr);
        ::_exit(127);
    }
    ::close(descriptors[1]);
    if (child < 0)
    {
        ::close(descriptors[0]);
        return result;
    }
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    int status{0};
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (::waitpid(child, &status, WNOHANG) == child)
        {
            result.completed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    if (!result.completed)
    {
        ::kill(child, SIGKILL);
        ::waitpid(child, &status, 0);
    }
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    std::array<char, 1024> buffer{};
    for (ssize_t count{::read(descriptors[0], buffer.data(), buffer.size())}; count > 0; count = ::read(descriptors[0], buffer.data(), buffer.size()))
    {
        result.output.append(buffer.data(), static_cast<std::size_t>(count));
    }
    ::close(descriptors[0]);
#endif
    return result;
}

TEST(console_application_test, interrupt_runs_cleanup_and_returns_success)
{
    const auto result{run_console_process("interrupt")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1"), std::string::npos);
    EXPECT_NE(result.output.find("stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("handler_called_during_run=0"), std::string::npos);
}

TEST(console_application_test, redirected_output_and_missing_input_preserve_lifecycle)
{
    const auto result{run_console_process("normal")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("콘솔 UTF-8 출력"), std::string::npos);
    EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
#ifdef _WIN32
    EXPECT_NE(result.output.find("utf8=1"), std::string::npos);
    EXPECT_NE(result.output.find("handler_restored=1"), std::string::npos);
#endif
}

TEST(console_application_test, error_exit_and_startup_failure_preserve_cleanup)
{
    for (const std::string_view mode : {"error", "startup-error", "cleanup-error"})
    {
        const auto result{run_console_process(mode)};
        SCOPED_TRACE(result.output);
        ASSERT_TRUE(result.completed);
        EXPECT_EQ(result.code, 1);
        EXPECT_NE(result.output.find("result=1 starts=1"), std::string::npos);
        EXPECT_NE(result.output.find(mode == "startup-error" ? "updates=0" : "updates=1"), std::string::npos);
        EXPECT_NE(result.output.find("stops=1 same_thread=1 quitting=1 repeated_result=1"), std::string::npos);
        EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    }
}

TEST(console_application_test, already_quitting_skips_preparation_and_lifecycle)
{
    const auto result{run_console_process("early-quit")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.output.find("result=1 starts=0 updates=0 stops=0 same_thread=1 quitting=1 repeated_result=1"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
#ifdef _WIN32
    EXPECT_NE(result.output.find("fault_triggered=0"), std::string::npos);
#endif
}

TEST(console_application_test, repeated_signal_during_cleanup_still_returns_success)
{
    const auto result{run_console_process("repeat")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("handler_called_during_run=0"), std::string::npos);
}

TEST(console_application_test, subsequent_instance_does_not_inherit_termination_request)
{
    const auto result{run_console_process("sequential")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("next_result=0 next_updates=1"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
}

TEST(console_application_test, interrupt_during_startup_reaches_cleanup)
{
    const auto result{run_console_process("startup-interrupt")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1"), std::string::npos);
    EXPECT_NE(result.output.find("stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("handler_called_during_run=0"), std::string::npos);
}

#ifndef _WIN32
TEST(console_application_test, sigterm_runs_cleanup_and_returns_success)
{
    const auto result{run_console_process("terminate")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
}
#endif

#ifdef _WIN32
TEST(console_application_test, missing_console_preserves_lifecycle)
{
    const auto result{run_console_process("normal", false)};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("fault_triggered=0"), std::string::npos);
    EXPECT_NE(result.output.find("utf8=0"), std::string::npos);
}

TEST(console_application_test, registration_failure_preserves_lifecycle_and_restores_output)
{
    const auto result{run_console_process("register-error")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("handler_restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
    EXPECT_NE(result.output.find("utf8=1"), std::string::npos);
}

TEST(console_application_test, frame_wait_preparation_failure_skips_lifecycle)
{
    for (const std::string_view mode : {"frame-event-error", "frame-timer-error"})
    {
        const auto result{run_console_process(mode)};
        SCOPED_TRACE(result.output);
        ASSERT_TRUE(result.completed);
        EXPECT_EQ(result.code, 1);
        EXPECT_NE(result.output.find("result=1 starts=0 updates=0 stops=0 same_thread=1 quitting=1 repeated_result=1"), std::string::npos);
        EXPECT_NE(result.output.find("restored=1"), std::string::npos);
        EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
    }
}

TEST(console_application_test, frame_wait_failure_stops_once_and_preserves_error_status)
{
    for (const std::string_view mode : {"frame-arm-error", "frame-wait-error", "frame-notify-error"})
    {
        const auto result{run_console_process(mode)};
        SCOPED_TRACE(result.output);
        ASSERT_TRUE(result.completed);
        EXPECT_EQ(result.code, 1);
        EXPECT_NE(result.output.find("result=1 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=1"), std::string::npos);
        EXPECT_NE(result.output.find("restored=1"), std::string::npos);
        EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
    }
}

TEST(console_application_test, frame_notification_failure_preserves_first_nonzero_exit_code)
{
    const auto result{run_console_process("custom-exit-notify-error")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 42);
    EXPECT_NE(result.output.find("result=42 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=42"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
}

TEST(console_application_test, code_page_preparation_failure_preserves_lifecycle)
{
    for (const std::string_view mode : {"query-error", "output-error"})
    {
        const auto result{run_console_process(mode)};
        SCOPED_TRACE(result.output);
        ASSERT_TRUE(result.completed);
        EXPECT_EQ(result.code, 0);
        EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
        EXPECT_NE(result.output.find("restored=1"), std::string::npos);
        EXPECT_NE(result.output.find("handler_restored=1"), std::string::npos);
        EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
        EXPECT_NE(result.output.find("utf8=0"), std::string::npos);
    }
}

TEST(console_application_test, handler_removal_failure_restores_code_page_and_preserves_exit_code)
{
    const auto result{run_console_process("handler-restore-error")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
}

TEST(console_application_test, code_page_restore_failure_removes_handler_and_preserves_exit_code)
{
    const auto result{run_console_process("output-restore-error")};
    SCOPED_TRACE(result.output);
    ASSERT_TRUE(result.completed);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("result=0 starts=1 updates=1 stops=1 same_thread=1 quitting=1 repeated_result=0"), std::string::npos);
    EXPECT_NE(result.output.find("restored=0"), std::string::npos);
    EXPECT_NE(result.output.find("handler_restored=1"), std::string::npos);
    EXPECT_NE(result.output.find("fault_triggered=1"), std::string::npos);
}
#endif
} // namespace
