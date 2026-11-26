#include <memory>
#include <utility>

#include <nori/io/co_spawn.hpp>

#include "context.hpp"
#include "coroutine_entry.hpp"
#include "executor.hpp"
#include "registry.hpp"
#include "strand.hpp"

namespace
{
nori::io::task_handle co_spawn_task(std::shared_ptr<nori::io::detail::executor> target, nori::io::awaitable<void> work)
{
    const auto root{target->root()};
    if (!root)
    {
        return {};
    }
    const auto owner{std::make_shared<nori::io::coroutine_entry>(nori::io::make_coroutine_entry(std::move(work)))};
    const auto task{root->add_task(owner)};
    if (!task)
    {
        return {};
    }
    owner->bind(root, owner, task);
    if (!nori::io::post_resume(std::move(target), owner->coroutine(), owner))
    {
        nori::io::release(task);
        return {};
    }
    return task;
}
} // namespace

namespace nori::io
{
task_handle co_spawn(context_handle ctx, awaitable<void> work)
{
    return co_spawn_task(acquire(ctx), std::move(work));
}

task_handle co_spawn(strand_handle st, awaitable<void> work)
{
    return co_spawn_task(acquire(st), std::move(work));
}
} // namespace nori::io
