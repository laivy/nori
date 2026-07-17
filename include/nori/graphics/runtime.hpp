#pragma once

namespace nori::graphics
{
class runtime
{
public:
    runtime();
    ~runtime() noexcept;

    runtime(const runtime&) = delete;
    runtime(runtime&&) = delete;
    runtime& operator=(const runtime&) = delete;
    runtime& operator=(runtime&&) = delete;
};
} // namespace nori::graphics
