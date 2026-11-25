#include <memory>
#include <utility>

#include <nori/core/abort.hpp>

#include "executor.hpp"

namespace
{
thread_local std::shared_ptr<nori::io::detail::executor> current_executor_;
} // namespace

namespace nori::io::detail
{
executor::~executor() = default;

std::shared_ptr<executor> current_executor() noexcept
{
    return current_executor_;
}

[[noreturn]] void abort_executor_failure() noexcept
{
    core::abort("failed to schedule nori::io coroutine");
}
} // namespace nori::io::detail

namespace nori::io
{
executor_scope::executor_scope(std::shared_ptr<detail::executor> executor) noexcept :
    previous_{std::exchange(current_executor_, std::move(executor))}
{
}

executor_scope::~executor_scope()
{
    current_executor_ = std::move(previous_);
}
} // namespace nori::io
