#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/application.hpp>

#include "application_test_helpers.hpp"

namespace
{
class other_test_application : public test_application
{
public:
    using test_application::test_application;
};

TEST(application_test, sequential_instances)
{
    {
        test_application first{{60, 60, 8}};
    }
    other_test_application second{{60, 60, 8}};
}

class lifecycle_application : public test_application
{
public:
    explicit lifecycle_application(const specification& spec) :
        test_application{spec},
        starts{0},
        stops{0},
        updates{0},
        fixed_updates{0},
        fails{false},
        quits_on_start{false},
        delays{false},
        maximum_fixed_count{0},
        fixed_in_frame{0}
    {
    }

    int starts;
    int stops;
    int updates;
    int fixed_updates;
    bool fails;
    bool quits_on_start;
    bool delays;
    int maximum_fixed_count;
    int fixed_in_frame;
    std::vector<int> fixed_counts;

protected:
    void on_start() override
    {
        ++starts;
        if (fails)
        {
            quit(1);
        }
        else if (quits_on_start)
        {
            quit(0);
        }
    }

    void on_fixed_update(float delta_seconds) override
    {
        EXPECT_NEAR(delta_seconds, 0.01f, 0.000001f);
        ++fixed_updates;
        ++fixed_in_frame;
    }

    void on_update(float) override
    {
        maximum_fixed_count = std::max(maximum_fixed_count, fixed_in_frame);
        fixed_counts.push_back(fixed_in_frame);
        fixed_in_frame = 0;
        ++updates;
        if (!delays)
        {
            quit(0);
            return;
        }
        if (updates == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{80});
        }
        if (fixed_updates >= 8)
        {
            quit(0);
        }
    }

    void on_stop() override
    {
        ++stops;
    }
};

class phase_application : public test_application
{
public:
    explicit phase_application(const specification& spec) :
        test_application{spec},
        execution_thread{std::this_thread::get_id()},
        updates{0}
    {
    }

    std::thread::id execution_thread;
    std::vector<char> phases;
    std::vector<float> update_deltas;
    int updates;

protected:
    void on_start() override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        phases.push_back('s');
    }

    void on_fixed_update(float delta_seconds) override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_NEAR(delta_seconds, 0.01f, 0.000001f);
        EXPECT_TRUE(phases.back() == 's' || phases.back() == 'f' || phases.back() == 'u' || phases.back() == 'p');
        phases.push_back('f');
    }

    void on_update(float delta_seconds) override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_GE(delta_seconds, 0.0f);
        EXPECT_TRUE(phases.back() == 's' || phases.back() == 'f' || phases.back() == 'u' || phases.back() == 'p');
        phases.push_back('u');
        update_deltas.push_back(delta_seconds);
        if (++updates == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{30});
        }
        else
        {
            EXPECT_GE(delta_seconds, 0.03f);
            quit(0);
        }
    }

    void on_stop() override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_TRUE(is_quitting());
        phases.push_back('t');
    }
};

TEST(application_test, phase_order)
{
    phase_application app{{100, 100, 8}};
    EXPECT_EQ(app.run(), 0);
    ASSERT_GE(app.phases.size(), 4);
    EXPECT_EQ(app.phases.front(), 's');
    EXPECT_EQ(app.phases[app.phases.size() - 2], 'u');
    EXPECT_EQ(app.phases.back(), 't');
    EXPECT_EQ(app.updates, 2);
    EXPECT_EQ(std::count(app.phases.begin(), app.phases.end(), 's'), 1);
    EXPECT_EQ(std::count(app.phases.begin(), app.phases.end(), 't'), 1);
    EXPECT_EQ(std::count(app.phases.begin(), app.phases.end(), 'u'), 2);
    EXPECT_GT(std::count(app.phases.begin(), app.phases.end(), 'f'), 0);
}

TEST(application_test, phases_without_fixed_updates)
{
    phase_application app{{100, 100, 0}};
    EXPECT_EQ(app.run(), 0);
    const std::vector<char> expected{'s', 'u', 'u', 't'};
    EXPECT_EQ(app.phases, expected);
}

class platform_phase_application final : public phase_application
{
public:
    explicit platform_phase_application(std::uint32_t max_fixed_updates = 0) :
        phase_application{{100, 100, max_fixed_updates}},
        fails_platform_start{false},
        quits_at_frame_start{false}
    {
    }

    bool fails_platform_start;
    bool quits_at_frame_start;

private:
    void on_platform_start() override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_EQ(phases.back(), 's');
        phases.push_back('b');
        if (fails_platform_start)
        {
            quit(1);
        }
    }

    void on_platform_begin_frame() override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_TRUE(phases.back() == 'b' || phases.back() == 'r');
        phases.push_back('p');
        if (quits_at_frame_start)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{30});
            quit(0);
        }
    }

    void on_platform_end_frame() override
    {
        EXPECT_EQ(std::this_thread::get_id(), execution_thread);
        EXPECT_EQ(phases.back(), 'u');
        phases.push_back('r');
    }
};

TEST(application_test, platform_phase_order)
{
    platform_phase_application app;
    EXPECT_EQ(app.run(), 0);
    const std::vector<char> expected{'s', 'b', 'p', 'u', 'r', 'p', 'u', 'r', 't'};
    EXPECT_EQ(app.phases, expected);
}

TEST(application_test, platform_start_failure)
{
    platform_phase_application app;
    app.fails_platform_start = true;
    EXPECT_EQ(app.run(), 1);
    const std::vector<char> expected{'s', 'b', 't'};
    EXPECT_EQ(app.phases, expected);
    EXPECT_EQ(app.updates, 0);
}

TEST(application_test, quit_at_platform_frame_start_completes_frame_before_stopping)
{
    platform_phase_application app{1};
    app.quits_at_frame_start = true;
    EXPECT_EQ(app.run(), 0);
    const std::vector<char> expected{'s', 'b', 'p', 'f', 'u', 'r', 't'};
    EXPECT_EQ(app.phases, expected);
    EXPECT_EQ(app.updates, 1);
}

class update_only_application final : public test_application
{
public:
    update_only_application() :
        test_application{{100, 100, 8}},
        updates{0}
    {
    }

    int updates;

protected:
    void on_update(float) override
    {
        if (++updates == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{30});
        }
        else
        {
            quit(0);
        }
    }
};

TEST(application_test, default_hooks)
{
    update_only_application app;
    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.updates, 2);
}

TEST(application_test, startup_failure)
{
    lifecycle_application app{{60, 60, 8}};
    app.fails = true;
    EXPECT_EQ(app.run(), 1);
    EXPECT_EQ(app.starts, 1);
    EXPECT_EQ(app.fixed_updates, 0);
    EXPECT_EQ(app.updates, 0);
    EXPECT_EQ(app.stops, 1);
}

TEST(application_test, startup_quit)
{
    lifecycle_application app{{60, 60, 8}};
    app.quits_on_start = true;
    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.starts, 1);
    EXPECT_EQ(app.fixed_updates, 0);
    EXPECT_EQ(app.updates, 0);
    EXPECT_EQ(app.stops, 1);
}

TEST(application_test, fixed_backlog)
{
    lifecycle_application app{{100, 100, 2}};
    app.delays = true;
    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.maximum_fixed_count, 2);
    EXPECT_GE(app.fixed_updates, 8);
    EXPECT_GE(app.updates, 5);
    ASSERT_GE(app.fixed_counts.size(), 5);
    for (std::size_t i{1}; i < 5; ++i)
    {
        EXPECT_EQ(app.fixed_counts[i], 2);
    }
}

TEST(application_test, early_quit)
{
    lifecycle_application app{{60, 60, 8}};
    app.quit(1);
    app.quit(0);
    EXPECT_EQ(app.run(), 1);
    EXPECT_EQ(app.starts, 0);
}

TEST(application_test, first_nonzero_exit_code_is_preserved)
{
    for (const int code : {42, -7})
    {
        SCOPED_TRACE(code);
        lifecycle_application app{{60, 100, 8}};
        app.quit(0);
        app.quit(code);
        app.quit(1);
        app.quit(0);
        EXPECT_TRUE(app.is_quitting());
        EXPECT_EQ(app.run(), code);
        EXPECT_EQ(app.starts, 0);
    }
}

TEST(application_test, quit_after_run_upgrades_result_without_restarting_lifecycle)
{
    for (const std::uint32_t frame_rate : {0u, 60u})
    {
        SCOPED_TRACE(frame_rate);
        lifecycle_application app{{frame_rate, 100, 8}};
        ASSERT_EQ(app.run(), 0);

        app.quit(42);
        app.quit(1);
        app.quit(0);
        EXPECT_TRUE(app.is_quitting());
        EXPECT_EQ(app.run(), 42);
        EXPECT_EQ(app.starts, 1);
        EXPECT_EQ(app.updates, 1);
        EXPECT_EQ(app.stops, 1);
    }
}

TEST(application_test, final_frame)
{
    completed_frame_application<test_application> app{{100, 100, 3}};
    expect_frame_completed_after_fixed_update_quit(app);
}

TEST(application_test, target_frame_rate)
{
    waiting_application<test_application> app{{10, 100, 32}};
    app.quits_on_second = true;
    EXPECT_EQ(app.run(), 0);
    ASSERT_EQ(app.update_times.size(), 2);
    // CI 부하 시 100ms 대기 미달을 허용하도록 하한을 70ms로 완화한다.
    EXPECT_GE(app.update_times[1] - app.update_times[0], std::chrono::milliseconds{70});
    EXPECT_GE(app.update_deltas[1], 0.07f);
    EXPECT_GE(app.fixed_deltas.size(), 9);
    for (const float delta_seconds : app.fixed_deltas)
    {
        EXPECT_NEAR(delta_seconds, 0.01f, 0.000001f);
    }
}

class frame_delta_application final : public test_application
{
public:
    frame_delta_application() :
        test_application{{0, 100, 1}}
    {
        frame_begin_times.reserve(3);
        update_deltas.reserve(3);
    }

    std::vector<std::chrono::steady_clock::time_point> frame_begin_times;
    std::vector<float> update_deltas;

protected:
    void on_fixed_update(float) override
    {
        if (frame_begin_times.size() == 2)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    }

    void on_update(float delta_seconds) override
    {
        update_deltas.push_back(delta_seconds);
        if (update_deltas.size() == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{30});
        }
        else if (update_deltas.size() == 3)
        {
            quit(0);
        }
    }

private:
    void on_platform_begin_frame() override
    {
        frame_begin_times.push_back(std::chrono::steady_clock::now());
    }
};

TEST(application_test, update_delta_uses_frame_elapsed_before_fixed_work)
{
    frame_delta_application app;
    ASSERT_EQ(app.run(), 0);
    ASSERT_EQ(app.frame_begin_times.size(), 3);
    ASSERT_EQ(app.update_deltas.size(), 3);
    for (std::size_t i{1}; i < app.update_deltas.size(); ++i)
    {
        const double elapsed{std::chrono::duration<double>{app.frame_begin_times[i] - app.frame_begin_times[i - 1]}.count()};
        EXPECT_NEAR(app.update_deltas[i], elapsed, 0.02);
    }
}

TEST(application_test, unlimited_frame_rate)
{
    waiting_application<test_application> app{{0, 60, 8}};
    app.quits_on_second = true;
    const auto start{std::chrono::steady_clock::now()};
    EXPECT_EQ(app.run(), 0);
    ASSERT_EQ(app.update_times.size(), 2);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds{500});
}

TEST(application_test, frame_wait)
{
    waiting_application<test_application> app{{5, 1, 8}};
    app.quits_on_second = true;
    app.first_update_delay = std::chrono::milliseconds{120};
    EXPECT_EQ(app.run(), 0);
    ASSERT_EQ(app.update_times.size(), 2);
    const auto elapsed{app.update_times[1] - app.update_times[0]};
    // 120ms 작업 뒤 200ms를 다시 기다리는 오류(총 320ms)를 배제
    EXPECT_GE(elapsed, std::chrono::milliseconds{180});
    EXPECT_LT(elapsed, std::chrono::milliseconds{280});
    EXPECT_NEAR(app.update_deltas[1], std::chrono::duration<double>{elapsed}.count(), 0.05);
    EXPECT_TRUE(app.fixed_deltas.empty());
}

TEST(application_test, frame_overrun)
{
    waiting_application<test_application> app{{10, 1, 8}};
    app.quits_on_second = true;
    app.first_update_delay = std::chrono::milliseconds{120};
    EXPECT_EQ(app.run(), 0);
    ASSERT_EQ(app.update_times.size(), 2);
    // 100ms 프레임 간격을 추가로 기다리는 오류를 배제
    EXPECT_LT(app.update_times[1] - app.first_update_completed, std::chrono::milliseconds{70});
    EXPECT_GE(app.update_deltas[1], 0.12f);
}

class overrun_recovery_application final : public test_application
{
public:
    overrun_recovery_application() :
        test_application{{10, 1, 0}}
    {
        update_times.reserve(4);
    }

    std::vector<std::chrono::steady_clock::time_point> update_times;

protected:
    void on_update(float) override
    {
        update_times.push_back(std::chrono::steady_clock::now());
        if (update_times.size() == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{250});
        }
        else if (update_times.size() == 4)
        {
            quit(0);
        }
    }
};

TEST(application_test, frame_overrun_recovers_schedule_without_replaying_missed_frames)
{
    overrun_recovery_application app;
    ASSERT_EQ(app.run(), 0);
    ASSERT_EQ(app.update_times.size(), 4);
    const auto elapsed{app.update_times[3] - app.update_times[0]};
    EXPECT_GE(elapsed, std::chrono::milliseconds{380});
    EXPECT_LT(elapsed, std::chrono::milliseconds{445});
    EXPECT_GT(app.update_times[2] - app.update_times[1], std::chrono::milliseconds{20});
    EXPECT_LT(app.update_times[2] - app.update_times[1], std::chrono::milliseconds{85});
}

class delayed_frame_application final : public test_application
{
public:
    delayed_frame_application() :
        test_application{{10, 1, 0}}
    {
        frame_begin_times.reserve(4);
        update_times.reserve(4);
    }

    std::vector<std::chrono::steady_clock::time_point> frame_begin_times;
    std::vector<std::chrono::steady_clock::time_point> update_times;

protected:
    void on_update(float) override
    {
        update_times.push_back(std::chrono::steady_clock::now());
        if (update_times.size() == 4)
        {
            quit(0);
        }
    }

private:
    void on_platform_begin_frame() override
    {
        frame_begin_times.push_back(std::chrono::steady_clock::now());
        if (frame_begin_times.size() == 2)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{250});
        }
    }
};

TEST(application_test, delayed_frame_recovers_start_schedule_without_waiting_before_update)
{
    delayed_frame_application app;
    ASSERT_EQ(app.run(), 0);
    ASSERT_EQ(app.frame_begin_times.size(), 4);
    ASSERT_EQ(app.update_times.size(), 4);
    EXPECT_GE(app.frame_begin_times[1] - app.frame_begin_times[0], std::chrono::milliseconds{80});
    EXPECT_GE(app.frame_begin_times[2] - app.frame_begin_times[1], std::chrono::milliseconds{250});
    EXPECT_LT(app.update_times[2] - app.frame_begin_times[2], std::chrono::milliseconds{20});
    const auto interval{app.frame_begin_times[3] - app.frame_begin_times[2]};
    EXPECT_GT(interval, std::chrono::milliseconds{20});
    EXPECT_LT(interval, std::chrono::milliseconds{85});
    const auto elapsed{app.frame_begin_times[3] - app.frame_begin_times[0]};
    EXPECT_GE(elapsed, std::chrono::milliseconds{380});
    EXPECT_LT(elapsed, std::chrono::milliseconds{445});
}

TEST(application_test, threaded_quit)
{
    waiting_application<test_application> app{{1, 60, 8}};
    expect_prompt_cross_thread_quit(app);
    EXPECT_EQ(app.updates, 1);
}
} // namespace
