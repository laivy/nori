#include <utility>

#include <nori/core/abort.hpp>
#include <nori/gameplay/detail/object_registry.hpp>

namespace nori::gameplay::detail
{
object_registry::object_registry(std::uint64_t world_id) :
    world_id_{world_id}
{
    if (world_id_ == 0)
    {
        core::abort("invalid world identifier");
    }
}

object_registry::slot_handle object_registry::find_slot(object_handle handle) const
{
    return handle.world_id == world_id_ && objects_.contains(handle.slot) ? handle.slot : slot_handle{};
}

object_handle object_registry::make_handle(slot_handle handle) const
{
    return {world_id_, handle};
}

object_handle object_registry::insert(std::unique_ptr<object> obj)
{
    if (!obj)
    {
        core::abort("cannot register a null game object");
    }
    return make_handle(objects_.emplace(std::move(obj)));
}

object* object_registry::find(object_handle handle)
{
    return const_cast<object*>(std::as_const(*this).find(handle));
}

const object* object_registry::find(object_handle handle) const
{
    const auto slot{find_slot(handle)};
    return slot ? objects_.at(slot).get() : nullptr;
}

bool object_registry::contains(object_handle handle) const
{
    return find(handle) != nullptr;
}

std::vector<object_handle> object_registry::objects() const
{
    std::vector<object_handle> handles;
    handles.reserve(objects_.size());
    for (const auto& [slot, _] : objects_)
    {
        handles.push_back(make_handle(slot));
    }
    return handles;
}

std::unique_ptr<object> object_registry::extract(object_handle handle)
{
    const auto slot{find_slot(handle)};
    auto obj{std::move(objects_.at(slot))};
    objects_.erase(slot);
    return obj;
}
} // namespace nori::gameplay::detail
