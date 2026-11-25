#pragma once
#include <functional>
#include <memory>
#include <mutex>

#include <asio/any_io_executor.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>

#include <nori/io/context.hpp>
#include <nori/io/task_handle.hpp>

#include "executor.hpp"

namespace nori::io
{
class coroutine_entry;

class context final : public detail::executor, public std::enable_shared_from_this<context>
{
public:
    context();
    ~context() override;

    void set_handle(context_handle handle) noexcept;
    context_handle handle() const noexcept;

    void run();
    void poll();
    void stop();

    [[nodiscard]] bool post(std::move_only_function<void()> function) override;
    [[nodiscard]] bool is_stopped() const override;
    [[nodiscard]] std::shared_ptr<context> root() override;
    asio::any_io_executor native_executor();
    [[nodiscard]] bool submit(asio::any_io_executor target, std::move_only_function<void()> function);
    [[nodiscard]] task_handle add_task(std::shared_ptr<coroutine_entry> owner);

private:
    using work_guard = asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context native_;
    work_guard work_;
    mutable std::mutex mutex_;
    bool stopped_;
    context_handle handle_;
};
} // namespace nori::io
