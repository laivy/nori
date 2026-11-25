#pragma once
#include <functional>

#include <nori/io/context.hpp>
#include <nori/io/strand.hpp>

namespace nori::io
{
[[nodiscard]] bool post(context_handle ctx, std::move_only_function<void()> function);
[[nodiscard]] bool post(strand_handle st, std::move_only_function<void()> function);
} // namespace nori::io
