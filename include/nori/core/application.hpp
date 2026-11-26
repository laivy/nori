#pragma once
#include <chrono>
#include <cstdint>
#include <memory>

#include <nori/core/timer.hpp>

namespace nori::core
{
class application
{
public:
    struct specification
    {
        std::uint32_t target_frame_rate;
        std::uint32_t fixed_update_rate;
        std::uint32_t max_fixed_updates_per_frame;
    };

    explicit application(const specification& spec);
    virtual ~application();

    application(const application&) = delete;
    application(application&&) = delete;
    application& operator=(const application&) = delete;
    application& operator=(application&&) = delete;

    [[nodiscard]] virtual int run();

    void quit(int code) noexcept;

    [[nodiscard]] bool is_quitting() const noexcept;

protected:
    virtual void on_start();
    virtual void on_stop();

    virtual void on_begin_frame();
    virtual void on_end_frame();

    virtual void on_fixed_update(float delta_seconds);
    virtual void on_update(float delta_seconds);

    [[nodiscard]] int exit_code() const noexcept;

private:
    class control;

    virtual void on_platform_start();
    virtual void on_platform_begin_frame();
    virtual void on_platform_end_frame();

    void reset_frame_timing();
    void update_frame();
    void wait_for_next_frame();

    specification spec_;
    std::unique_ptr<control> control_;

    timer frame_timer_;
    std::chrono::steady_clock::time_point next_frame_time_;
    std::chrono::steady_clock::duration frame_interval_;
    std::chrono::duration<double> accumulator_;
};
} // namespace nori::core
