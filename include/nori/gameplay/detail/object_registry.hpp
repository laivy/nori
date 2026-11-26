#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include <nori/core/handle.hpp>
#include <nori/core/slot_map.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/object/object_handle.hpp>

namespace nori::gameplay::detail
{
class object_registry
{
public:
    explicit object_registry(std::uint64_t world_id);
    ~object_registry() = default;

    object_registry(const object_registry&) = delete;
    object_registry& operator=(const object_registry&) = delete;

    object_handle insert(std::unique_ptr<object> obj);

    [[nodiscard]] object* find(object_handle handle);
    [[nodiscard]] const object* find(object_handle handle) const;
    [[nodiscard]] bool contains(object_handle handle) const;
    [[nodiscard]] std::vector<object_handle> objects() const;

    std::unique_ptr<object> extract(object_handle handle);

private:
    using slot_handle = core::basic_handle<object_tag>;

    [[nodiscard]] slot_handle find_slot(object_handle handle) const;
    [[nodiscard]] object_handle make_handle(slot_handle handle) const;

    std::uint64_t world_id_;
    core::slot_map<slot_handle, std::unique_ptr<object>> objects_;
};
} // namespace nori::gameplay::detail
