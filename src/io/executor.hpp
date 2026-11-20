#pragma once
#include <functional>
#include <memory>

namespace nori::io
{
class context;
} // namespace nori::io

namespace nori::io::detail
{
class executor
{
public:
    virtual ~executor();
    [[nodiscard]] virtual bool post(std::move_only_function<void()> function) = 0;
    [[nodiscard]] virtual bool is_stopped() const = 0;
    [[nodiscard]] virtual std::shared_ptr<context> root() = 0;
};

std::shared_ptr<executor> current_executor() noexcept;
[[noreturn]] void abort_executor_failure() noexcept;
} // namespace nori::io::detail

namespace nori::io
{
class executor_scope
{
public:
    explicit executor_scope(std::shared_ptr<detail::executor> executor) noexcept;
    ~executor_scope();

    executor_scope(const executor_scope&) = delete;
    executor_scope(executor_scope&&) = delete;
    executor_scope& operator=(const executor_scope&) = delete;
    executor_scope& operator=(executor_scope&&) = delete;

private:
    std::shared_ptr<detail::executor> previous_;
};
} // namespace nori::io
