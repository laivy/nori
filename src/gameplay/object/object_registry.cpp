#include <atomic>
#include <limits>
#include <utility>

#include <nori/core/abort.hpp>

#include <nori/gameplay/object/object_registry.hpp>

namespace
{
std::atomic<std::uint64_t> next_registry_id{1};
}

namespace nori::gameplay
{
object_registry::object_registry() :
    id_{next_registry_id.fetch_add(1, std::memory_order_relaxed)}
{
    if (id_ == 0)
    {
        core::abort("object registry identifier exhausted");
    }
}

object_registry::slot_handle object_registry::resolve(object_handle handle) const
{
    if (handle.world_id != id_ || handle.slot >= std::numeric_limits<std::uint32_t>::max() || handle.generation > std::numeric_limits<std::uint32_t>::max())
    {
        return {};
    }
    const slot_handle key{static_cast<std::uint32_t>(handle.slot + 1), static_cast<std::uint32_t>(handle.generation)};
    return objects_.contains(key) ? key : slot_handle{};
}

object_handle object_registry::make_handle(slot_handle handle) const
{
    return {id_, handle.index - 1, handle.generation};
}

object_handle object_registry::insert(std::unique_ptr<object> value)
{
    if (!value)
    {
        core::abort("cannot register a null game object");
    }
    return make_handle(objects_.emplace(entry{std::move(value), false}));
}

object* object_registry::find(object_handle handle)
{
    return const_cast<object*>(std::as_const(*this).find(handle));
}

const object* object_registry::find(object_handle handle) const
{
    const auto key{resolve(handle)};
    if (!key)
    {
        return nullptr;
    }
    const auto& value{objects_.at(key)};
    return value.pending_removal ? nullptr : value.value.get();
}

bool object_registry::contains(object_handle handle) const
{
    return find(handle) != nullptr;
}

std::vector<object_handle> object_registry::objects() const
{
    std::vector<object_handle> result;
    result.reserve(objects_.size());
    for (const auto& [key, value] : objects_)
    {
        if (!value.pending_removal)
        {
            result.push_back(make_handle(key));
        }
    }
    return result;
}

bool object_registry::destroy(object_handle handle)
{
    if (!contains(handle))
    {
        return false;
    }
    objects_.at(resolve(handle)).pending_removal = true;
    return true;
}

void object_registry::flush_pending_changes()
{
    std::vector<slot_handle> keys;
    keys.reserve(objects_.size());
    for (const auto& [key, value] : objects_)
    {
        keys.push_back(key);
    }
    for (const auto key : keys)
    {
        if (!objects_.contains(key))
        {
            continue;
        }
        auto& value{objects_.at(key)};
        if (value.pending_removal)
        {
            objects_.erase(key);
        }
        else
        {
            value.value->finish_update();
        }
    }
}
} // namespace nori::gameplay
