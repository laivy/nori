#pragma once
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <nori/core/input_code.hpp>

namespace nori::core
{
struct window_close_event
{
};

struct window_destroy_event
{
};

struct window_resize_event
{
    int width;
    int height;
};

struct key_down_event
{
    key_code key;
    bool repeat;
};

struct key_up_event
{
    key_code key;
};

struct mouse_button_down_event
{
    mouse_button button;
    int x;
    int y;
};

struct mouse_button_up_event
{
    mouse_button button;
    int x;
    int y;
};

struct mouse_move_event
{
    int x;
    int y;
};

struct mouse_wheel_event
{
    int x;
    int y;
    float delta;
};

using event_data = std::variant<
    window_close_event,
    window_destroy_event,
    window_resize_event,
    key_down_event,
    key_up_event,
    mouse_button_down_event,
    mouse_button_up_event,
    mouse_move_event,
    mouse_wheel_event
>;

struct event
{
    event_data data;
    bool is_handled;
};

class event_dispatcher
{
private:
    struct state;
    using event_listener = std::function<bool(event&)>;

public:
    class [[nodiscard]] listener_handle
    {
        friend class event_dispatcher;

    public:
        listener_handle();
        ~listener_handle();

        listener_handle(const listener_handle&) = delete;
        listener_handle(listener_handle&& other) noexcept;
        listener_handle& operator=(const listener_handle&) = delete;
        listener_handle& operator=(listener_handle&& other) noexcept;

        bool subscribed() const;
        void unsubscribe();

    private:
        listener_handle(std::weak_ptr<state> state, std::uint64_t id);

    private:
        std::weak_ptr<state> state_;
        std::uint64_t id_;
    };

public:
    event_dispatcher();
    ~event_dispatcher();

    event_dispatcher(const event_dispatcher&) = delete;
    event_dispatcher(event_dispatcher&&) noexcept = default;
    event_dispatcher& operator=(const event_dispatcher&) = delete;
    event_dispatcher& operator=(event_dispatcher&&) noexcept = default;

    template <typename Listener>
    requires(!std::is_invocable_r_v<bool, std::remove_reference_t<Listener>&, event&>)
    listener_handle add_listener(Listener&& listener)
    {
        return add_listener(make_event_listener(std::forward<Listener>(listener)));
    }

    template <typename Listener>
    requires(!std::is_invocable_r_v<bool, std::remove_reference_t<Listener>&, event&>)
    void add_static_listener(Listener&& listener)
    {
        add_static_listener(make_event_listener(std::forward<Listener>(listener)));
    }

    void dispatch(event& e);

private:
    template <typename Listener>
    static event_listener make_event_listener(Listener&& listener)
    {
        return [callback = std::forward<Listener>(listener)](event& e) mutable
        {
            return std::visit(
                [&callback](auto&& value) -> bool
                {
                    if constexpr (std::is_invocable_r_v<bool, decltype(callback)&, decltype(value)>)
                    {
                        return callback(std::forward<decltype(value)>(value));
                    }
                    else
                    {
                        return false;
                    }
                },
                e.data
            );
        };
    }

    listener_handle add_listener(event_listener listener);
    void add_static_listener(event_listener listener);

private:
    std::shared_ptr<state> state_;
};
} // namespace nori::core
