#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace nori::core
{
template <typename Signature>
class delegate;

template <typename R, typename... Args>
class delegate<R(Args...)>
{
public:
    delegate() = default;

    delegate(const delegate&) = delete;
    delegate(delegate&&) noexcept = default;
    delegate& operator=(const delegate&) = delete;
    delegate& operator=(delegate&&) noexcept = default;

    template <typename Func>
    requires std::is_invocable_r_v<R, Func&, Args...>
    void bind(Func&& func)
    {
        auto replacement{std::make_shared<callback>(std::forward<Func>(func))};
        const std::shared_ptr<callback> previous{std::exchange(func_, std::move(replacement))};
    }

    template <typename Object, typename Method>
    requires(std::is_member_function_pointer_v<Method> && std::is_invocable_r_v<R, Method, Object*, Args...>)
    void bind(Object* object, Method method)
    {
        auto func = [object, method](Args... args) -> R
        {
            return std::invoke(method, object, std::forward<Args>(args)...);
        };
        bind(std::move(func));
    }

    void unbind() noexcept
    {
        const std::shared_ptr<callback> previous{std::move(func_)};
    }

    [[nodiscard]] bool is_bound() const noexcept
    {
        return func_ && static_cast<bool>(*func_);
    }

    R execute(Args... args)
    {
        const std::shared_ptr<callback> func{func_};
        return (*func)(std::forward<Args>(args)...);
    }

private:
    using callback = std::move_only_function<R(Args...)>;

    std::shared_ptr<callback> func_;
};
} // namespace nori::core
