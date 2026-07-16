#include <algorithm>
#include <utility>
#include <vector>
#include "event.hpp"

namespace nori::core
{
struct event_dispatcher::state
{
    struct listener_slot
    {
        std::uint64_t id;
        event_listener listener;
    };

    std::uint64_t next_id;
    std::vector<listener_slot> slots;
    std::uint32_t dispatch_depth;
    bool has_unsubscribed_slots;
};

event_dispatcher::listener_handle::listener_handle() :
    id_{0}
{
}

event_dispatcher::listener_handle::listener_handle(std::weak_ptr<state> state, std::uint64_t id) :
    state_{std::move(state)},
    id_{id}
{
}

event_dispatcher::listener_handle::listener_handle(listener_handle&& other) noexcept :
    state_{std::move(other.state_)},
    id_{other.id_}
{
    other.id_ = 0;
}

event_dispatcher::listener_handle::~listener_handle()
{
    unsubscribe();
}

event_dispatcher::listener_handle& event_dispatcher::listener_handle::operator=(listener_handle&& other) noexcept
{
    if (this != &other)
    {
        unsubscribe();
        state_ = std::move(other.state_);
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

bool event_dispatcher::listener_handle::subscribed() const
{
    if (id_ == 0)
    {
        return false;
    }
    if (auto state{state_.lock()})
    {
        const auto slot{std::ranges::find(state->slots, id_, &state::listener_slot::id)};
        if (slot == state->slots.end())
        {
            return false;
        }
        if (!slot->listener)
        {
            return false;
        }
        return true;
    }
    return false;
}

void event_dispatcher::listener_handle::unsubscribe()
{
    if (id_ == 0)
    {
        return;
    }
    if (auto state{state_.lock()})
    {
        const auto slot{std::ranges::find(state->slots, id_, &state::listener_slot::id)};
        if (slot != state->slots.end())
        {
            slot->listener = nullptr;
            state->has_unsubscribed_slots = true;
        }
    }
    id_ = 0;
}

event_dispatcher::event_dispatcher() :
    state_{std::make_shared<state>(1)}
{
}

event_dispatcher::~event_dispatcher() = default;

event_dispatcher::listener_handle event_dispatcher::add_listener(event_listener listener)
{
    const std::uint64_t id{state_->next_id++};
    state_->slots.push_back(state::listener_slot{id, std::move(listener)});
    return listener_handle{state_, id};
}

void event_dispatcher::add_static_listener(event_listener listener)
{
    const std::uint64_t id{state_->next_id++};
    state_->slots.push_back(state::listener_slot{id, std::move(listener)});
}

void event_dispatcher::dispatch(event& e)
{
    ++state_->dispatch_depth;
    for (const auto& slot : state_->slots)
    {
        auto listener{slot.listener};
        if (!listener)
        {
            continue;
        }
        if (listener(e))
        {
            e.is_handled = true;
            break;
        }
    }
    --state_->dispatch_depth;

    // 이벤트 처리하다가 삭제된 리스너 제거
    if (state_->dispatch_depth == 0 && state_->has_unsubscribed_slots)
    {
        std::erase_if(
            state_->slots,
            [](const state::listener_slot& slot)
            {
                return !slot.listener;
            }
        );
        state_->has_unsubscribed_slots = false;
    }
}
} // namespace nori::core
