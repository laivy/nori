#pragma once
#include <coroutine>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <nori/io/awaitable.hpp>

#include "coroutine_entry.hpp"
#include "executor.hpp"

namespace nori::io
{
class schedule_operation
{
public:
    explicit schedule_operation(std::shared_ptr<detail::executor> executor) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> continuation);
    bool await_resume() const noexcept;

private:
    std::weak_ptr<detail::executor> executor_;
    bool scheduled_;
};

template <typename Result>
class [[nodiscard]] io_operation
{
public:
    using completion_handler = std::move_only_function<void(Result)>;
    using starter = std::move_only_function<void(completion_handler)>;

    explicit io_operation(starter start) :
        start_{std::move(start)}
    {
    }

    io_operation(const io_operation&) = delete;
    io_operation(io_operation&&) = delete;
    io_operation& operator=(const io_operation&) = delete;
    io_operation& operator=(io_operation&&) = delete;

    bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> continuation)
    {
        const std::shared_ptr<detail::executor> executor{detail::current_executor()};
        if (!executor)
        {
            detail::abort_executor_failure();
        }

        const std::weak_ptr<detail::executor> executor_ref{executor};
        const std::shared_ptr<coroutine_entry> owner{current_coroutine_entry()};
        std::move(start_)([this, continuation, executor_ref, owner](Result result) mutable
        {
            result_.emplace(std::move(result));

            const std::shared_ptr<detail::executor> executor{executor_ref.lock()};
            if (!executor)
            {
                detail::abort_executor_failure();
            }

            resume_coroutine(executor, continuation, std::move(owner));
        });
    }

    Result await_resume()
    {
        return std::move(*result_);
    }

private:
    starter start_;
    std::optional<Result> result_;
};

schedule_operation schedule_on(std::shared_ptr<detail::executor> executor) noexcept;

template <typename Result, typename Starter>
io_operation<Result> async_io_operation(Starter&& start)
{
    return io_operation<Result>{typename io_operation<Result>::starter{std::forward<Starter>(start)}};
}
} // namespace nori::io
