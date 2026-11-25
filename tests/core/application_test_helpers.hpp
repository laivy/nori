#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/application.hpp>

namespace
{
using test_application = nori::core::application;

template <typename Base>
class completed_frame_application : public Base
{
public:
    explicit completed_frame_application(const typename Base::specification& spec) :
        Base{spec},
        updates{0},
        final_fixed_updates{0},
        stops{0}
    {
    }

    int updates;
    int final_fixed_updates;
    int stops;

protected:
    void on_fixed_update(float) override
    {
        if (updates == 1)
        {
            ++final_fixed_updates;
            this->quit(0);
        }
    }

    void on_update(float) override
    {
        if (++updates == 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{40});
        }
        else
        {
            EXPECT_TRUE(this->is_quitting());
            EXPECT_EQ(final_fixed_updates, 3);
        }
    }

    void on_stop() override
    {
        EXPECT_EQ(updates, 2);
        ++stops;
    }
};

template <typename App>
void expect_frame_completed_after_fixed_update_quit(App& app)
{
    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.final_fixed_updates, 3);
    EXPECT_EQ(app.updates, 2);
    EXPECT_EQ(app.stops, 1);
}

template <typename Base>
class waiting_application : public Base
{
public:
    explicit waiting_application(const typename Base::specification& spec) :
        Base{spec},
        enters{false},
        updates{0},
        quits_on_second{false},
        first_update_delay{0}
    {
    }

    std::atomic<bool> enters;
    int updates;
    bool quits_on_second;
    std::chrono::milliseconds first_update_delay;
    std::chrono::steady_clock::time_point first_update_completed;
    std::vector<std::chrono::steady_clock::time_point> update_times;
    std::vector<float> update_deltas;
    std::vector<float> fixed_deltas;

protected:
    void on_fixed_update(float delta_seconds) override
    {
        fixed_deltas.push_back(delta_seconds);
    }

    void on_update(float delta_seconds) override
    {
        update_times.push_back(std::chrono::steady_clock::now());
        update_deltas.push_back(delta_seconds);
        ++updates;
        enters.store(true);
        enters.notify_all();
        if (updates == 1)
        {
            std::this_thread::sleep_for(first_update_delay);
            first_update_completed = std::chrono::steady_clock::now();
        }
        if (quits_on_second && updates == 2)
        {
            this->quit(0);
        }
    }
};

template <typename App>
void expect_prompt_cross_thread_quit(App& app)
{
    std::jthread requester{[&app]()
    {
        app.enters.wait(false);
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        app.quit(0);
    }};
    const auto start{std::chrono::steady_clock::now()};
    EXPECT_EQ(app.run(), 0);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds{500});
}
} // namespace
