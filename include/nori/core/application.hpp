#pragma once
#include <utility>
#include <nori/core/event.hpp>

namespace nori::core
{
class application
{
public:
    enum class exit_code
    {
        success,
        error,
        restart
    };

public:
    virtual ~application() = default;

    application(const application&) = delete;
    application(application&&) = delete;
    application& operator=(const application&) = delete;
    application& operator=(application&&) = delete;

    virtual void run() = 0;
    virtual void quit(exit_code code) = 0;

    template <typename Listener>
    event_dispatcher::listener_handle add_event_listener(Listener&& listener)
    {
        return event_dispatcher_.add_listener(std::forward<Listener>(listener));
    }

    template <typename Listener>
    void add_static_event_listener(Listener&& listener)
    {
        event_dispatcher_.add_static_listener(std::forward<Listener>(listener));
    }

protected:
    application() = default;

    virtual void on_event(event& e);
    virtual void on_tick(float delta_seconds);

    void process_event(event& e);

private:
    event_dispatcher event_dispatcher_;
};
} // namespace nori::core
