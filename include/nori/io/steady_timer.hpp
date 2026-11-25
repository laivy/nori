#pragma once
#include <chrono>

#include <nori/core/handle.hpp>
#include <nori/io/awaitable.hpp>
#include <nori/io/context.hpp>
#include <nori/io/error_code.hpp>

namespace nori::io::detail
{
struct steady_timer_tag;

class steady_timer_deleter
{
public:
    void operator()(core::basic_handle<steady_timer_tag> handle) const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
using steady_timer_handle = core::basic_handle<detail::steady_timer_tag>;
using unique_steady_timer_handle = core::unique_handle<steady_timer_handle, detail::steady_timer_deleter>;

[[nodiscard]] unique_steady_timer_handle create_steady_timer(context_handle ctx);
void destroy(steady_timer_handle timer);

[[nodiscard]] awaitable<result<void>> async_wait(steady_timer_handle timer, std::chrono::steady_clock::duration duration);
[[nodiscard]] awaitable<result<void>> async_wait_until(steady_timer_handle timer, std::chrono::steady_clock::time_point deadline);
void cancel(steady_timer_handle timer);
} // namespace nori::io
