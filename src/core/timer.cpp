#include "timer.hpp"

namespace nori::core
{
timer::timer()
{
    reset();
}

void timer::reset()
{
    start_ = std::chrono::steady_clock::now();
    previous_ = start_;
}

float timer::tick()
{
    const std::chrono::steady_clock::time_point current{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> delta_seconds{current - previous_};
    previous_ = current;
    return delta_seconds.count();
}

float timer::elapsed() const
{
    const std::chrono::steady_clock::time_point current{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> elapsed_seconds{current - start_};
    return elapsed_seconds.count();
}
} // namespace nori::core
