#pragma once
#include <nori/core/handle.hpp>
#include <nori/io/awaitable.hpp>

namespace nori::io::detail
{
struct task_tag;
} // namespace nori::io::detail

namespace nori::io
{
using task_handle = core::basic_handle<detail::task_tag>;

[[nodiscard]] awaitable<void> async_join(task_handle task);
} // namespace nori::io
