#include <coroutine>
#include <memory>
#include <utility>

#include "executor.hpp"
#include "io_operation.hpp"

namespace nori::io
{
schedule_operation::schedule_operation(std::shared_ptr<detail::executor> executor) noexcept :
    executor_{std::move(executor)},
    scheduled_{false}
{
}

bool schedule_operation::await_ready() const noexcept
{
    return false;
}

bool schedule_operation::await_suspend(std::coroutine_handle<> continuation)
{
    const std::shared_ptr<detail::executor> executor{executor_.lock()};
    if (!executor)
    {
        return false;
    }

    if (executor == detail::current_executor())
    {
        scheduled_ = true;
        return false;
    }

    scheduled_ = true;
    if (!post_resume(executor, continuation))
    {
        scheduled_ = false;
        return false;
    }
    return true;
}

bool schedule_operation::await_resume() const noexcept
{
    return scheduled_;
}

schedule_operation schedule_on(std::shared_ptr<detail::executor> executor) noexcept
{
    return schedule_operation{std::move(executor)};
}
} // namespace nori::io
