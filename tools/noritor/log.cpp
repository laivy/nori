#include "pch.hpp"

#include "log.hpp"

namespace
{
using nori::editor::log;
constexpr std::size_t max_entries{256};
std::vector<log::entry> log_entries;

void append(log::level level, std::string message)
{
    log_entries.push_back({std::chrono::system_clock::now(), level, std::move(message)});
    if (log_entries.size() > max_entries)
    {
        log_entries.erase(log_entries.begin());
    }
}
} // namespace

namespace nori::editor
{
void log::info(std::string message)
{
    append(level::info, std::move(message));
}

void log::error(std::string message)
{
    append(level::error, std::move(message));
}

std::span<const log::entry> log::entries()
{
    return {log_entries.begin(), log_entries.end()};
}

void log::clear()
{
    log_entries.clear();
}
} // namespace nori::editor
