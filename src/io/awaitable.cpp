#include <nori/core/abort.hpp>
#include <nori/io/awaitable.hpp>

#include "context.hpp"
#include "coroutine_entry.hpp"
#include "executor.hpp"
#include "registry.hpp"
#include "strand.hpp"

namespace nori::io::detail
{
bool awaitable_promise_base::final_awaiter::await_ready() const noexcept
{
    return false;
}

void awaitable_promise_base::final_awaiter::await_resume() const noexcept
{
}

std::coroutine_handle<> awaitable_promise_base::final_awaiter::await_suspend(const awaitable_promise_base& promise) const noexcept
{
    const std::coroutine_handle<> continuation{promise.continuation_};
    if (!continuation)
    {
        return std::noop_coroutine();
    }

    if (!promise.continuation_executor_)
    {
        return continuation;
    }

    const std::shared_ptr<executor> continuation_executor{promise.continuation_executor_->lock()};
    if (!continuation_executor)
    {
        abort_executor_failure();
    }

    if (continuation_executor == current_executor())
    {
        return continuation;
    }

    if (!post_resume(continuation_executor, continuation))
    {
        abort_executor_failure();
    }
    return std::noop_coroutine();
}

std::suspend_always awaitable_promise_base::initial_suspend() const noexcept
{
    return {};
}

awaitable_promise_base::final_awaiter awaitable_promise_base::final_suspend() const noexcept
{
    return {};
}

void awaitable_promise_base::unhandled_exception() noexcept
{
    core::abort("unhandled exception in nori::io coroutine");
}

std::coroutine_handle<> awaitable_promise_base::await_suspend(std::coroutine_handle<> coroutine, std::coroutine_handle<> continuation) noexcept
{
    continuation_ = continuation;

    const std::shared_ptr<executor> continuation_executor{current_executor()};
    if (continuation_executor)
    {
        continuation_executor_.emplace(continuation_executor);
    }
    else
    {
        continuation_executor_.reset();
    }

    if (!executor_)
    {
        executor_ = continuation_executor_;
    }
    if (!executor_)
    {
        return coroutine;
    }

    const std::shared_ptr<executor> target{executor_->lock()};
    if (!target)
    {
        abort_executor_failure();
    }
    if (target == continuation_executor)
    {
        return coroutine;
    }
    if (!post_resume(target, coroutine))
    {
        abort_executor_failure();
    }
    return std::noop_coroutine();
}

void awaitable_promise_base::bind_executor(context_handle ctx) noexcept
{
    executor_.emplace(acquire(ctx));
}

void awaitable_promise_base::bind_executor(strand_handle st) noexcept
{
    executor_.emplace(acquire(st));
}

void awaitable_result<void>::return_void() const noexcept
{
}

void awaitable_result<void>::take() const noexcept
{
}
} // namespace nori::io::detail
