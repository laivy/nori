#include <cassert>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include <nori/core/slot_map.hpp>
#include <nori/io/post.hpp>

#include "acceptor.hpp"
#include "connection.hpp"
#include "context.hpp"
#include "coroutine_entry.hpp"
#include "executor.hpp"
#include "registry.hpp"
#include "steady_timer.hpp"
#include "strand.hpp"

namespace
{
template <typename T>
struct resource_record
{
    nori::io::context_handle context;
    std::shared_ptr<T> state;
};

struct join_waiter
{
    std::coroutine_handle<> coroutine;
    std::weak_ptr<nori::io::detail::executor> executor;
    std::weak_ptr<nori::io::coroutine_entry> owner;
};

struct task_record
{
    nori::io::context_handle context;
    std::shared_ptr<nori::io::coroutine_entry> owner;
    std::vector<join_waiter> waiters;
};

struct registry_state
{
    std::mutex mutex;
    nori::core::slot_map<nori::io::context_handle, std::shared_ptr<nori::io::context>> contexts;
    nori::core::slot_map<nori::io::strand_handle, resource_record<nori::io::strand>> strands;
    nori::core::slot_map<nori::io::steady_timer_handle, resource_record<nori::io::steady_timer>> timers;
    nori::core::slot_map<nori::io::connection_handle, resource_record<nori::io::connection>> connections;
    nori::core::slot_map<nori::io::acceptor_handle, resource_record<nori::io::acceptor>> acceptors;
    nori::core::slot_map<nori::io::task_handle, task_record> tasks;
};

class join_operation
{
public:
    explicit join_operation(nori::io::task_handle task) noexcept;

    bool await_ready() const;
    bool await_suspend(std::coroutine_handle<> coroutine) const;
    void await_resume() const noexcept;

private:
    nori::io::task_handle task_;
};

registry_state registry;

template <typename Handle, typename T>
Handle insert_resource(nori::core::slot_map<Handle, resource_record<T>>& table, nori::io::context_handle ctx, std::shared_ptr<T> state)
{
    const std::lock_guard lock{registry.mutex};
    return table.emplace(resource_record<T>{ctx, std::move(state)});
}

template <typename Handle, typename T>
std::shared_ptr<T> acquire_resource(nori::core::slot_map<Handle, resource_record<T>>& table, Handle handle)
{
    const std::lock_guard lock{registry.mutex};
    return table[handle].state;
}

template <typename Handle, typename T>
std::shared_ptr<T> retire_resource(nori::core::slot_map<Handle, resource_record<T>>& table, Handle handle)
{
    std::shared_ptr<T> state;
    {
        const std::lock_guard lock{registry.mutex};
        state = std::move(table[handle].state);
        table.erase(handle);
    }
    return state;
}

template <typename Handle, typename T>
void detach_resources(nori::core::slot_map<Handle, resource_record<T>>& table, nori::io::context_handle ctx, std::vector<std::shared_ptr<T>>& states)
{
    for (auto it{table.begin()}; it != table.end();)
    {
        auto& [handle, entry]{*it};
        ++it;
        if (entry.context == ctx)
        {
            states.push_back(std::move(entry.state));
            table.erase(handle);
        }
    }
}

join_operation::join_operation(nori::io::task_handle task) noexcept :
    task_{task}
{
}

bool join_operation::await_ready() const
{
    const std::lock_guard lock{registry.mutex};
    return !registry.tasks.contains(task_);
}

bool join_operation::await_suspend(std::coroutine_handle<> coroutine) const
{
    const auto target{nori::io::detail::current_executor()};
    const auto owner{nori::io::current_coroutine_entry()};
    assert(target && owner);
    const std::lock_guard lock{registry.mutex};
    if (!registry.tasks.contains(task_))
    {
        return false;
    }
    registry.tasks[task_].waiters.push_back(join_waiter{coroutine, target, owner});
    return true;
}

void join_operation::await_resume() const noexcept
{
}
} // namespace

namespace nori::io
{
std::shared_ptr<context> acquire(context_handle handle)
{
    const std::lock_guard lock{registry.mutex};
    return registry.contexts[handle];
}

std::shared_ptr<strand> acquire(strand_handle handle)
{
    return acquire_resource(registry.strands, handle);
}

connection_handle insert(context_handle ctx, std::shared_ptr<connection> state)
{
    return insert_resource(registry.connections, ctx, std::move(state));
}

task_handle insert(context_handle ctx, std::shared_ptr<coroutine_entry> owner)
{
    const std::lock_guard lock{registry.mutex};
    return registry.tasks.emplace(task_record{.context = ctx, .owner = std::move(owner)});
}

void release(task_handle task)
{
    std::shared_ptr<coroutine_entry> owner;
    {
        const std::lock_guard lock{registry.mutex};
        owner = std::move(registry.tasks[task].owner);
    }
}

void finish(task_handle task)
{
    std::vector<join_waiter> waiters;
    {
        const std::lock_guard lock{registry.mutex};
        assert(registry.tasks.contains(task));
        assert(!registry.tasks[task].owner);
        waiters = std::move(registry.tasks[task].waiters);
        registry.tasks.erase(task);
    }
    for (const auto& waiter : waiters)
    {
        const auto owner{waiter.owner.lock()};
        const auto target{waiter.executor.lock()};
        if (owner && target)
        {
            std::ignore = post_resume(target, waiter.coroutine, owner);
        }
    }
}

unique_context_handle create_context()
{
    const auto state{std::make_shared<context>()};
    context_handle handle;
    {
        const std::lock_guard lock{registry.mutex};
        handle = registry.contexts.emplace(state);
    }
    state->set_handle(handle);
    return {handle, {}};
}

void destroy(context_handle ctx)
{
    std::shared_ptr<context> root;
    std::vector<std::shared_ptr<strand>> strands;
    std::vector<std::shared_ptr<steady_timer>> timers;
    std::vector<std::shared_ptr<connection>> connections;
    std::vector<std::shared_ptr<acceptor>> acceptors;
    std::vector<std::shared_ptr<coroutine_entry>> owners;
    {
        const std::lock_guard lock{registry.mutex};
        root = std::move(registry.contexts[ctx]);
        registry.contexts.erase(ctx);
        detach_resources(registry.strands, ctx, strands);
        detach_resources(registry.timers, ctx, timers);
        detach_resources(registry.connections, ctx, connections);
        detach_resources(registry.acceptors, ctx, acceptors);
        for (auto& [_, entry] : registry.tasks)
        {
            if (entry.context == ctx && entry.owner)
            {
                owners.push_back(std::move(entry.owner));
            }
        }
    }
    root->stop();
    for (const auto& listener : acceptors)
    {
        listener->close();
    }
    for (const auto& target : connections)
    {
        target->close();
    }
    for (const auto& timer : timers)
    {
        timer->cancel(true);
    }
    owners.clear();
    acceptors.clear();
    connections.clear();
    timers.clear();
    strands.clear();
    root.reset();
}

void run(context_handle ctx)
{
    acquire(ctx)->run();
}

void poll(context_handle ctx)
{
    acquire(ctx)->poll();
}

void stop(context_handle ctx)
{
    acquire(ctx)->stop();
}

bool post(context_handle ctx, std::move_only_function<void()> function)
{
    return acquire(ctx)->post(std::move(function));
}

unique_strand_handle create_strand(context_handle ctx)
{
    return {insert_resource(registry.strands, ctx, std::make_shared<strand>(acquire(ctx))), {}};
}

void destroy(strand_handle st)
{
    retire_resource(registry.strands, st);
}

bool post(strand_handle st, std::move_only_function<void()> function)
{
    return acquire(st)->post(std::move(function));
}

unique_steady_timer_handle create_steady_timer(context_handle ctx)
{
    return {insert_resource(registry.timers, ctx, std::make_shared<steady_timer>(acquire(ctx))), {}};
}

void destroy(steady_timer_handle timer)
{
    retire_resource(registry.timers, timer)->cancel(true);
}

awaitable<result<void>> async_wait(steady_timer_handle timer, std::chrono::steady_clock::duration duration)
{
    return acquire_resource(registry.timers, timer)->async_wait(duration);
}

awaitable<result<void>> async_wait_until(steady_timer_handle timer, std::chrono::steady_clock::time_point deadline)
{
    return acquire_resource(registry.timers, timer)->async_wait_until(deadline);
}

void cancel(steady_timer_handle timer)
{
    acquire_resource(registry.timers, timer)->cancel(false);
}

awaitable<result<void>> async_send(connection_handle target, opacket packet)
{
    return acquire_resource(registry.connections, target)->async_send(std::move(packet));
}

awaitable<result<ipacket>> async_receive(connection_handle target)
{
    return acquire_resource(registry.connections, target)->async_receive();
}

void close(connection_handle target)
{
    acquire_resource(registry.connections, target)->close();
}

endpoint remote_endpoint(connection_handle target)
{
    return acquire_resource(registry.connections, target)->remote_endpoint();
}

void destroy(connection_handle target)
{
    retire_resource(registry.connections, target)->close();
}

result<unique_acceptor_handle> create_acceptor(context_handle ctx, acceptor_options options)
{
    const std::shared_ptr<context> root{acquire(ctx)};
    if (root->is_stopped())
    {
        return std::unexpected{error_code::invalid_state};
    }
    const auto listener{std::make_shared<acceptor>(root, std::move(options))};
    if (const auto res{listener->open()}; !res)
    {
        return std::unexpected{res.error()};
    }
    return unique_acceptor_handle{insert_resource(registry.acceptors, ctx, listener), {}};
}

awaitable<result<unique_connection_handle>> async_accept(acceptor_handle listener)
{
    return acquire_resource(registry.acceptors, listener)->async_accept();
}

void close(acceptor_handle listener)
{
    acquire_resource(registry.acceptors, listener)->close();
}

std::uint16_t port(acceptor_handle listener)
{
    return acquire_resource(registry.acceptors, listener)->port();
}

void destroy(acceptor_handle listener)
{
    retire_resource(registry.acceptors, listener)->close();
}

awaitable<void> async_join(task_handle task)
{
    co_await join_operation{task};
}
} // namespace nori::io

namespace nori::io::detail
{
void context_deleter::operator()(context_handle handle) const noexcept
{
    destroy(handle);
}

void strand_deleter::operator()(strand_handle handle) const noexcept
{
    destroy(handle);
}

void steady_timer_deleter::operator()(steady_timer_handle handle) const noexcept
{
    destroy(handle);
}

void acceptor_deleter::operator()(acceptor_handle handle) const noexcept
{
    destroy(handle);
}

void connection_deleter::operator()(connection_handle handle) const noexcept
{
    destroy(handle);
}
} // namespace nori::io::detail
