#pragma once
#include <chrono>

namespace nori::core
{
class timer
{
public:
    timer();

    double tick();
    void reset();

private:
    std::chrono::steady_clock::time_point previous_;
};
} // namespace nori::core
