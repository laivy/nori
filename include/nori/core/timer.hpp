#pragma once
#include <chrono>

namespace nori::core
{
class timer
{
public:
    timer();

    void reset();
    float tick();
    float elapsed() const;

private:
    std::chrono::steady_clock::time_point start_;
    std::chrono::steady_clock::time_point previous_;
};
} // namespace nori::core
