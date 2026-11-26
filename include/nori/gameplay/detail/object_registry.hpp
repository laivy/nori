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

    object_handle insert(std::unique_ptr<object> value);

    [[nodiscard]] object* find(object_handle handle);
    [[nodiscard]] const object* find(object_handle handle) const;
    [[nodiscard]] bool contains(object_handle handle) const;
    [[nodiscard]] std::vector<object_handle> objects() const;

    bool destroy(object_handle handle);
    void flush_pending_changes();

private:
    using slot_handle = core::basic_handle<object_tag>;

    struct entry
    {
        std::unique_ptr<object> value;
        bool pending_removal{};
    };

    [[nodiscard]] slot_handle resolve(object_handle handle) const;
    [[nodiscard]] object_handle make_handle(slot_handle handle) const;

    std::uint64_t id_;
    core::slot_map<slot_handle, entry> objects_;
};
} // namespace nori::gameplay::detail
