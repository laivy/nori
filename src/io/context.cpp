#include <memory>
#include <utility>

#include <asio/post.hpp>

#include "context.hpp"
#include "registry.hpp"

namespace nori::io
{
context::context() :
    work_{asio::make_work_guard(native_)},
    stopped_{false},
    handle_{}
{
}

context::~context()
{
    stop();
}

void context::set_handle(context_handle handle) noexcept
{
    handle_ = handle;
}

context_handle context::handle() const noexcept
{
    return handle_;
}

void context::run()
{
    native_.run();
}

void context::poll()
{
    native_.poll();
}

void context::stop()
{
    const std::lock_guard lock{mutex_};
    if (!stopped_)
    {
        stopped_ = true;
        work_.reset();
        native_.stop();
    }
}

bool context::post(std::move_only_function<void()> function)
{
    const std::weak_ptr<context> self{weak_from_this()};
    return submit(native_.get_executor(), [self, function = std::move(function)]() mutable
    {
        const auto current{self.lock()};
        if (!current)
        {
            return;
        }
        executor_scope scope{current};
        std::move(function)();
    });
}

bool context::is_stopped() const
{
    const std::lock_guard lock{mutex_};
    return stopped_;
}

std::shared_ptr<context> context::root()
{
    return shared_from_this();
}

asio::any_io_executor context::native_executor()
{
    return native_.get_executor();
}

bool context::submit(asio::any_io_executor target, std::move_only_function<void()> function)
{
    const std::lock_guard lock{mutex_};
    if (stopped_)
    {
        return false;
    }
    asio::post(target, std::move(function));
    return true;
}

task_handle context::add_task(std::shared_ptr<coroutine_entry> owner)
{
    const std::lock_guard lock{mutex_};
    if (stopped_)
    {
        return {};
    }
    return insert(handle_, std::move(owner));
}
} // namespace nori::io
