#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/handle.hpp>
#include <nori/core/slot_map.hpp>

namespace nori::core
{
template <typename Signature>
class multicast_delegate;

template <typename... Args>
requires((!std::is_rvalue_reference_v<Args> && (std::is_reference_v<Args> || std::is_copy_constructible_v<Args>)) && ...)
class multicast_delegate<void(Args...)>
{
public:
    using handle = basic_handle<multicast_delegate>;

    multicast_delegate() :
        broadcast_depth_{0}
    {
    }

    multicast_delegate(const multicast_delegate&) = delete;

    multicast_delegate(multicast_delegate&& other) noexcept :
        multicast_delegate{}
    {
        assert(other.broadcast_depth_ == 0);
        std::swap(listeners_, other.listeners_);
        handles_.swap(other.handles_);
    }

    multicast_delegate& operator=(const multicast_delegate&) = delete;

    multicast_delegate& operator=(multicast_delegate&& other) noexcept
    {
        assert(broadcast_depth_ == 0 && other.broadcast_depth_ == 0);
        if (this != &other)
        {
            multicast_delegate moved{std::move(other)};
            std::swap(listeners_, moved.listeners_);
            handles_.swap(moved.handles_);
        }
        return *this;
    }

    template <typename Func>
    requires std::is_invocable_r_v<void, Func&, Args...>
    [[nodiscard]] handle add(Func&& func)
    {
        const listener_state state{broadcast_depth_ == 0 ? listener_state::active : listener_state::pending_activation};
        const handle h{listeners_.emplace(listener{state, callback{std::forward<Func>(func)}})};
        handles_.push_back(h);
        return h;
    }

    template <typename Object, typename Method>
    requires(std::is_member_function_pointer_v<Method> && std::is_invocable_r_v<void, Method, Object*, Args...>)
    [[nodiscard]] handle add(Object* object, Method method)
    {
        auto func = [object, method](Args... args)
        {
            std::invoke(method, object, std::forward<Args>(args)...);
        };
        return add(std::move(func));
    }

    [[nodiscard]] bool remove(handle h) noexcept
    {
        if (!listeners_.contains(h))
        {
            return false;
        }
        listener& slot{listeners_.at(h)};
        if (slot.state == listener_state::pending_removal)
        {
            return false;
        }
        if (broadcast_depth_ != 0)
        {
            slot.state = listener_state::pending_removal;
        }
        else
        {
            erase(h);
        }
        return true;
    }

    [[nodiscard]] bool is_bound() const noexcept
    {
        return std::ranges::any_of(handles_, [this](handle h)
        {
            return listeners_.at(h).state != listener_state::pending_removal;
        });
    }

    void broadcast(Args... args)
    {
        const broadcast_guard guard{this};
        const std::size_t count{handles_.size()};
        for (std::size_t i{0}; i < count; ++i)
        {
            listener& slot{listeners_.at(handles_[i])};
            if (slot.state != listener_state::active)
            {
                continue;
            }
            slot.func(args...);
        }
    }

private:
    using callback = std::move_only_function<void(Args...)>;

    enum class listener_state
    {
        active,
        pending_activation,
        pending_removal
    };

    struct listener
    {
        listener_state state;
        callback func;
    };

    class broadcast_guard
    {
    public:
        explicit broadcast_guard(multicast_delegate* owner) noexcept :
            owner_{owner}
        {
            ++owner_->broadcast_depth_;
        }

        ~broadcast_guard() noexcept
        {
            --owner_->broadcast_depth_;
            if (owner_->broadcast_depth_ == 0)
            {
                owner_->on_finish();
            }
        }

        broadcast_guard(const broadcast_guard&) = delete;
        broadcast_guard(broadcast_guard&&) = delete;
        broadcast_guard& operator=(const broadcast_guard&) = delete;
        broadcast_guard& operator=(broadcast_guard&&) = delete;

    private:
        multicast_delegate* owner_;
    };

    void erase(handle h) noexcept
    {
        // 콜백 소멸자의 재진입 전 슬롯과 handle 제거
        callback removed{std::move(listeners_.at(h).func)};
        const auto it{std::ranges::find(handles_, h)};
        assert(it != handles_.end());
        *it = handles_.back();
        handles_.pop_back();
        listeners_.erase(h);
    }

    void on_finish() noexcept
    {
        for (const handle h : handles_)
        {
            listener& slot{listeners_.at(h)};
            if (slot.state == listener_state::pending_activation)
            {
                slot.state = listener_state::active;
            }
        }
        while (true)
        {
            const auto it{std::ranges::find_if(handles_, [this](handle h)
            {
                return listeners_.at(h).state == listener_state::pending_removal;
            })};
            if (it == handles_.end())
            {
                break;
            }
            erase(*it);
        }
    }

    slot_map<handle, listener> listeners_;
    std::vector<handle> handles_;
    std::uint32_t broadcast_depth_;
};
} // namespace nori::core
