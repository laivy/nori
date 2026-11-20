#include <cstdio>
#include <cstdlib>
#include <print>

#include <nori/core/abort.hpp>

namespace nori::core
{
[[noreturn]] void abort(std::string_view message, std::source_location location) noexcept
{
    std::println(stderr, "{}\n  at {}:{}:{} in {}", message, location.file_name(), location.line(), location.column(), location.function_name());
    std::fflush(stderr);
    std::abort();
}
} // namespace nori::core
