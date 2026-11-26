#pragma once
#include <concepts>
#include <coroutine>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <nori/io/context.hpp>
#include <nori/io/strand.hpp>

namespace nori::io
{
template <typename T>
class awaitable;
} // namespace nori::io

namespace nori::io::detail
{
class executor;

class awaitable_promise_base
{
public:
    class final_awaiter
    {
    public:
        bool await_ready() const noexcept;

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coroutine) const noexcept
        {
            return await_suspend(coroutine.promise());
        }

        void await_resume() const noexcept;

    private:
        std::coroutine_handle<> await_suspend(const awaitable_promise_base& promise) const noexcept;
    };

    std::suspend_always initial_suspend() const noexcept;
    final_awaiter final_suspend() const noexcept;

    void unhandled_exception() noexcept;
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine, std::coroutine_handle<> continuation) noexcept;
    void bind_executor(context_handle ctx) noexcept;
    void bind_executor(strand_handle st) noexcept;

private:
    std::coroutine_handle<> continuation_;
    std::optional<std::weak_ptr<executor>> executor_;
    std::optional<std::weak_ptr<executor>> continuation_executor_;
};

template <typename T>
class awaitable_result
{
public:
    template <typename U>
    requires std::constructible_from<T, U&&>
    void return_value(U&& value)
    {
        value_.emplace(std::forward<U>(value));
    }

    T take()
    {
        return std::move(*value_);
    }

private:
    std::optional<T> value_;
};

template <>
class awaitable_result<void>
{
public:
    void return_void() const noexcept;
    void take() const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
template <typename T>
class [[nodiscard]] awaitable
{
    static_assert(!std::is_reference_v<T>);

public:
    class promise_type final :
        public detail::awaitable_promise_base,
        public detail::awaitable_result<T>
    {
    public:
        awaitable get_return_object() noexcept
        {
            return awaitable{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
    };

    class awaiter;

    awaitable() = delete;

    ~awaitable()
    {
        if (coroutine_)
        {
            coroutine_.destroy();
        }
    }

    awaitable(const awaitable&) = delete;

    awaitable(awaitable&& other) noexcept :
        coroutine_{std::exchange(other.coroutine_, {})}
    {
    }

    awaitable& operator=(const awaitable&) = delete;

    awaitable& operator=(awaitable&& other) noexcept
    {
        if (this != &other)
        {
            if (coroutine_)
            {
                coroutine_.destroy();
            }
            coroutine_ = std::exchange(other.coroutine_, {});
        }
        return *this;
    }

    awaiter operator co_await() && noexcept
    {
        return awaiter{std::exchange(coroutine_, {})};
    }

    void bind_executor(context_handle ctx) noexcept
    {
        coroutine_.promise().bind_executor(ctx);
    }

    void bind_executor(strand_handle st) noexcept
    {
        coroutine_.promise().bind_executor(st);
    }

private:
    using handle_type = std::coroutine_handle<promise_type>;

    explicit awaitable(handle_type coroutine) noexcept :
        coroutine_{coroutine}
    {
    }

    handle_type coroutine_;
};

template <typename T>
class awaitable<T>::awaiter
{
public:
    explicit awaiter(handle_type coroutine) noexcept :
        coroutine_{coroutine}
    {
    }

    ~awaiter()
    {
        if (coroutine_)
        {
            coroutine_.destroy();
        }
    }

    awaiter(const awaiter&) = delete;
    awaiter(awaiter&&) = delete;
    awaiter& operator=(const awaiter&) = delete;
    awaiter& operator=(awaiter&&) = delete;

    bool await_ready() const noexcept
    {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        return coroutine_.promise().await_suspend(coroutine_, continuation);
    }

    T await_resume()
    {
        if constexpr (std::is_void_v<T>)
        {
            coroutine_.promise().take();
            coroutine_.destroy();
            coroutine_ = {};
        }
        else
        {
            T value{coroutine_.promise().take()};
            coroutine_.destroy();
            coroutine_ = {};
            return value;
        }
    }

private:
    handle_type coroutine_;
};
} // namespace nori::io
