#pragma once
#include <chrono>
#include <format>
#include <span>
#include <string>
#include <utility>

namespace nori::editor
{
class log
{
public:
    enum class level
    {
        info,
        error
    };

    struct entry
    {
        std::chrono::system_clock::time_point timestamp;
        level severity;
        std::string text;
    };

    static void info(std::string message);
    static void error(std::string message);

    template <typename... Args>
    static void info(std::format_string<Args...> format, Args&&... args)
    {
        info(std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void error(std::format_string<Args...> format, Args&&... args)
    {
        error(std::format(format, std::forward<Args>(args)...));
    }

    static std::span<const entry> entries();
    static void clear();
};
} // namespace nori::editor
