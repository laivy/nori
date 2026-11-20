#include <coroutine>
#include <memory>
#include <tuple>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/io/awaitable.hpp>
#include <nori/io/task_handle.hpp>

#include "coroutine_entry.hpp"
#include "executor.hpp"
#include "registry.hpp"

namespace
{
thread_local std::shared_ptr<nori::io::coroutine_entry> current_coroutine_entry_;
} // namespace

namespace nori::io::detail
{
bool post_resume(std::shared_ptr<executor> target, std::coroutine_handle<> coroutine)
{
    return nori::io::post_resume(std::move(target), coroutine, current_coroutine_entry());
}
} // namespace nori::io::detail

namespace nori::io
{
class coroutine_entry::promise_type
{
public:
    class final_awaiter
    {
    public:
        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<promise_type> coroutine) const noexcept
        {
            auto& promise{coroutine.promise()};
            const std::shared_ptr<detail::executor> executor{promise.root_executor_.lock()};
            const std::shared_ptr<coroutine_entry> owner{promise.owner_.lock()};
            if (!executor || !owner)
            {
                return;
            }

            std::ignore = executor->post([task = promise.task_]()
            {
                release(task);
            });
        }

        void await_resume() const noexcept
        {
        }
    };

    coroutine_entry get_return_object() noexcept
    {
        return coroutine_entry{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() const noexcept
    {
        return {};
    }

    final_awaiter final_suspend() const noexcept
    {
        return {};
    }

    void return_void() const noexcept
    {
    }

    void unhandled_exception() const noexcept
    {
        core::abort("unhandled exception in nori::io coroutine");
    }

    void bind(std::shared_ptr<detail::executor> root_executor, std::shared_ptr<coroutine_entry> owner, task_handle task) noexcept
    {
        root_executor_ = std::move(root_executor);
        owner_ = std::move(owner);
        task_ = task;
    }

private:
    std::weak_ptr<detail::executor> root_executor_;
    std::weak_ptr<coroutine_entry> owner_;
    task_handle task_{};
};

coroutine_entry::coroutine_entry(std::coroutine_handle<promise_type> coroutine) noexcept :
    coroutine_{coroutine},
    task_{}
{
}

coroutine_entry::~coroutine_entry()
{
    if (coroutine_)
    {
        coroutine_.destroy();
    }
    if (task_)
    {
        finish(task_);
    }
}

coroutine_entry::coroutine_entry(coroutine_entry&& other) noexcept :
    coroutine_{std::exchange(other.coroutine_, {})},
    task_{std::exchange(other.task_, {})}
{
}

std::coroutine_handle<> coroutine_entry::coroutine() const noexcept
{
    return coroutine_;
}

void coroutine_entry::bind(std::shared_ptr<detail::executor> root_executor, std::shared_ptr<coroutine_entry> owner, task_handle task) noexcept
{
    auto& promise{std::coroutine_handle<promise_type>::from_address(coroutine_.address()).promise()};
    task_ = task;
    promise.bind(std::move(root_executor), std::move(owner), task);
}

coroutine_entry_scope::coroutine_entry_scope(std::shared_ptr<coroutine_entry> owner) noexcept :
    previous_{std::exchange(current_coroutine_entry_, std::move(owner))}
{
}

coroutine_entry_scope::~coroutine_entry_scope()
{
    current_coroutine_entry_ = std::move(previous_);
}

coroutine_entry make_coroutine_entry(awaitable<void> work)
{
    co_await std::move(work);
}

std::shared_ptr<coroutine_entry> current_coroutine_entry() noexcept
{
    return current_coroutine_entry_;
}

void resume_coroutine(std::shared_ptr<detail::executor> target, std::coroutine_handle<> coroutine, std::shared_ptr<coroutine_entry> owner)
{
    executor_scope executor_guard{std::move(target)};
    coroutine_entry_scope owner_guard{std::move(owner)};
    coroutine.resume();
}

bool post_resume(std::shared_ptr<detail::executor> target, std::coroutine_handle<> coroutine, std::shared_ptr<coroutine_entry> owner)
{
    if (!target)
    {
        return false;
    }
    return target->post([coroutine, owner = std::move(owner)]() mutable
    {
        resume_coroutine(detail::current_executor(), coroutine, std::move(owner));
    });
}
} // namespace nori::io
