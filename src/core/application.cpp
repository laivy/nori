#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <nori/core/application.hpp>

namespace nori::core
{
class application::control
{
public:
    control();
    ~control();

    [[nodiscard]] bool initialize();
    void quit(int code) noexcept;
    [[nodiscard]] bool is_quitting() const noexcept;
    [[nodiscard]] int exit_code() const noexcept;
    void wait_until(std::chrono::steady_clock::time_point deadline);

private:
    mutable std::shared_mutex mutex_;
    std::optional<int> exit_code_;
#ifdef _WIN32
    HANDLE wait_timer_;
    HANDLE quit_event_;
#else
    std::condition_variable_any condition_;
#endif
};

application::control::control()
#ifdef _WIN32
    :
    wait_timer_{nullptr},
    quit_event_{nullptr}
#endif
{
}

application::control::~control()
{
#ifdef _WIN32
    if (wait_timer_)
    {
        ::CloseHandle(wait_timer_);
    }
    if (quit_event_)
    {
        ::CloseHandle(quit_event_);
    }
#endif
}

bool application::control::initialize()
{
#ifdef _WIN32
    std::unique_lock lock{mutex_};
    quit_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!quit_event_)
    {
        lock.unlock();
        quit(1);
        return false;
    }

    wait_timer_ = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!wait_timer_)
    {
        lock.unlock();
        quit(1);
        return false;
    }

    if (exit_code_ && !::SetEvent(quit_event_))
    {
        lock.unlock();
        quit(1);
        return false;
    }
#endif
    return true;
}

void application::control::quit(int code) noexcept
{
    if (const std::unique_lock lock{mutex_})
    {
        if (!exit_code_ || *exit_code_ == 0)
        {
            exit_code_ = code;
        }
#ifdef _WIN32
        if (quit_event_ && !::SetEvent(quit_event_) && *exit_code_ == 0)
        {
            exit_code_ = 1;
        }
#endif
    }
#ifndef _WIN32
    condition_.notify_all();
#endif
}

bool application::control::is_quitting() const noexcept
{
    const std::shared_lock lock{mutex_};
    return exit_code_.has_value();
}

int application::control::exit_code() const noexcept
{
    const std::shared_lock lock{mutex_};
    return *exit_code_;
}

void application::control::wait_until(std::chrono::steady_clock::time_point deadline)
{
#ifdef _WIN32
    const std::array handles{quit_event_, wait_timer_};
    while (!is_quitting())
    {
        const auto remaining{deadline - std::chrono::steady_clock::now()};
        if (remaining <= std::chrono::steady_clock::duration::zero())
        {
            return;
        }

        using timer_duration = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;
        LARGE_INTEGER due_time{};
        due_time.QuadPart = -std::chrono::ceil<timer_duration>(remaining).count();
        if (!::SetWaitableTimerEx(wait_timer_, &due_time, 0, nullptr, nullptr, nullptr, 0))
        {
            quit(1);
            return;
        }

        const DWORD result{::WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE)};
        if (result == WAIT_OBJECT_0)
        {
            return;
        }
        if (result != WAIT_OBJECT_0 + 1)
        {
            quit(1);
            return;
        }
    }
#else
    std::unique_lock lock{mutex_};
    condition_.wait_until(lock, deadline, [this]()
    {
        return exit_code_.has_value();
    });
#endif
}

application::application(const specification& spec) :
    spec_{spec},
    control_{std::make_unique<control>()}
{
}

application::~application() = default;

int application::run()
{
    if (is_quitting())
    {
        return exit_code();
    }

    if (spec_.target_frame_rate != 0 && !control_->initialize())
    {
        return exit_code();
    }

    on_start();
    if (!is_quitting())
    {
        on_platform_start();
    }

    reset_frame_timing();
    while (!is_quitting())
    {
        wait_for_next_frame();
        if (is_quitting())
        {
            break;
        }
        on_begin_frame();
        on_platform_begin_frame();
        update_frame();
        on_platform_end_frame();
        on_end_frame();
    }
    on_stop();

    return exit_code();
}

void application::quit(int code) noexcept
{
    control_->quit(code);
}

bool application::is_quitting() const noexcept
{
    return control_->is_quitting();
}

void application::on_start()
{
}

void application::on_stop()
{
}

void application::on_begin_frame()
{
}

void application::on_end_frame()
{
}

void application::on_fixed_update(float)
{
}

void application::on_update(float)
{
}

int application::exit_code() const noexcept
{
    return control_->exit_code();
}

void application::on_platform_start()
{
}

void application::on_platform_begin_frame()
{
}

void application::on_platform_end_frame()
{
}

void application::reset_frame_timing()
{
    frame_timer_.reset();
    next_frame_time_ = std::chrono::steady_clock::now();
    if (spec_.target_frame_rate != 0)
    {
        const std::chrono::duration<double> period{1.0 / spec_.target_frame_rate};
        frame_interval_ = std::max(std::chrono::steady_clock::duration{1}, std::chrono::duration_cast<std::chrono::steady_clock::duration>(period));
    }
    else
    {
        frame_interval_ = std::chrono::steady_clock::duration::zero();
    }
    accumulator_ = std::chrono::duration<double>::zero();
}

void application::update_frame()
{
    const std::chrono::duration<double> step{1.0 / spec_.fixed_update_rate};
    const std::chrono::duration<double> elapsed{frame_timer_.tick()};
    accumulator_ += elapsed;

    for (std::uint32_t count{0}; count < spec_.max_fixed_updates_per_frame; ++count)
    {
        if (accumulator_ < step)
        {
            break;
        }
        on_fixed_update(static_cast<float>(step.count()));
        accumulator_ -= step;
    }
    on_update(static_cast<float>(elapsed.count()));
}

void application::wait_for_next_frame()
{
    if (spec_.target_frame_rate == 0 || is_quitting())
    {
        return;
    }

    control_->wait_until(next_frame_time_);
    const auto current{std::chrono::steady_clock::now()};
    if (current > next_frame_time_)
    {
        next_frame_time_ += ((current - next_frame_time_) / frame_interval_) * frame_interval_;
    }
    next_frame_time_ += frame_interval_;
}
} // namespace nori::core
