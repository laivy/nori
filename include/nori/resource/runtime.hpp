#pragma once
#include <filesystem>

namespace nori::resource
{
class runtime
{
public:
    struct specification
    {
        std::filesystem::path mount_path;
    };

public:
    explicit runtime(specification spec);
    ~runtime() noexcept;

    runtime(const runtime&) = delete;
    runtime(runtime&&) = delete;
    runtime& operator=(const runtime&) = delete;
    runtime& operator=(runtime&&) = delete;
};
} // namespace nori::resource
