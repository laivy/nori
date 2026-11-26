#pragma once
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <nori/io/awaitable.hpp>
#include <nori/io/task_handle.hpp>

namespace nori::io
{
[[nodiscard]] task_handle co_spawn(context_handle ctx, awaitable<void> work);
[[nodiscard]] task_handle co_spawn(strand_handle st, awaitable<void> work);

template <typename Executor, typename T>
requires std::same_as<Executor, context_handle> || std::same_as<Executor, strand_handle>
awaitable<T> run_on(Executor executor, awaitable<T> work)
{
    work.bind_executor(executor);
    return work;
}

template <typename Executor, typename Function>
requires(std::same_as<Executor, context_handle> || std::same_as<Executor, strand_handle>) && std::invocable<std::decay_t<Function>&>
auto run_on(Executor executor, Function&& function)
{
    using function_type = std::decay_t<Function>;
    using result_type = std::invoke_result_t<function_type&>;

    auto work = [](function_type function) -> awaitable<result_type>
    {
        if constexpr (std::is_void_v<result_type>)
        {
            std::invoke(function);
            co_return;
        }
        else
        {
            co_return std::invoke(function);
        }
    }(std::forward<Function>(function));

    return run_on(executor, std::move(work));
}
} // namespace nori::io
