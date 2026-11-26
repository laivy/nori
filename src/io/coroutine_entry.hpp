#pragma once
#include <coroutine>
#include <memory>

#include <nori/io/task_handle.hpp>

namespace nori::io::detail
{
class executor;

bool post_resume(std::shared_ptr<executor> target, std::coroutine_handle<> coroutine);
} // namespace nori::io::detail

namespace nori::io
{
template <typename T>
class awaitable;

class coroutine_entry
{
public:
    class promise_type;

    explicit coroutine_entry(std::coroutine_handle<promise_type> coroutine) noexcept;
    ~coroutine_entry();

    coroutine_entry(const coroutine_entry&) = delete;
    coroutine_entry(coroutine_entry&& other) noexcept;
    coroutine_entry& operator=(const coroutine_entry&) = delete;
    coroutine_entry& operator=(coroutine_entry&&) = delete;

    std::coroutine_handle<> coroutine() const noexcept;
    void bind(std::shared_ptr<detail::executor> root_executor, std::shared_ptr<coroutine_entry> owner, task_handle task) noexcept;

private:
    std::coroutine_handle<> coroutine_;
    task_handle task_;
};

class coroutine_entry_scope
{
public:
    explicit coroutine_entry_scope(std::shared_ptr<coroutine_entry> owner) noexcept;
    ~coroutine_entry_scope();

    coroutine_entry_scope(const coroutine_entry_scope&) = delete;
    coroutine_entry_scope(coroutine_entry_scope&&) = delete;
    coroutine_entry_scope& operator=(const coroutine_entry_scope&) = delete;
    coroutine_entry_scope& operator=(coroutine_entry_scope&&) = delete;

private:
    std::shared_ptr<coroutine_entry> previous_;
};

coroutine_entry make_coroutine_entry(awaitable<void> work);
std::shared_ptr<coroutine_entry> current_coroutine_entry() noexcept;

void resume_coroutine(std::shared_ptr<detail::executor> target, std::coroutine_handle<> coroutine, std::shared_ptr<coroutine_entry> owner);
bool post_resume(std::shared_ptr<detail::executor> target, std::coroutine_handle<> coroutine, std::shared_ptr<coroutine_entry> owner);
} // namespace nori::io
