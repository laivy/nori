#include <nori/core/timer.hpp>

namespace nori::core
{
timer::timer() :
    previous_{std::chrono::steady_clock::now()}
{
}

double timer::tick()
{
    const std::chrono::steady_clock::time_point current{std::chrono::steady_clock::now()};
    const std::chrono::duration<double> delta_seconds{current - previous_};
    previous_ = current;
    return delta_seconds.count();
}

void timer::reset()
{
    previous_ = std::chrono::steady_clock::now();
}
} // namespace nori::core
