#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

namespace nori::core
{
template <typename Tag>
struct basic_handle
{
    constexpr bool operator==(const basic_handle& other) const
    {
        return index == other.index && generation == other.generation;
    }

    explicit constexpr operator bool() const noexcept
    {
        return index != 0;
    }

    std::uint32_t index;
    std::uint32_t generation;
};

template <typename Handle, typename Deleter>
class unique_handle
{
public:
    unique_handle() noexcept(std::is_nothrow_default_constructible_v<Deleter>)
    requires std::is_default_constructible_v<Deleter>
        :
        handle_{},
        deleter_{}
    {
    }

    unique_handle(Handle handle, Deleter deleter) noexcept
    requires std::is_move_constructible_v<Deleter>
        :
        handle_{handle},
        deleter_{std::move(deleter)}
    {
    }

    ~unique_handle()
    {
        reset();
    }

    unique_handle(const unique_handle&) = delete;

    unique_handle(unique_handle&& other) noexcept
    requires std::is_move_constructible_v<Deleter>
        :
        handle_{other.release()},
        deleter_{std::move(other.deleter_)}
    {
    }

    unique_handle& operator=(const unique_handle&) = delete;

    unique_handle& operator=(unique_handle&& other) noexcept
    requires std::is_move_assignable_v<Deleter>
    {
        if (this != &other)
        {
            reset();
            deleter_ = std::move(other.deleter_);
            handle_ = other.release();
        }
        return *this;
    }

    [[nodiscard]] Handle get() const noexcept
    {
        return handle_;
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(handle_);
    }

    [[nodiscard]] Handle release() noexcept
    {
        return std::exchange(handle_, Handle{});
    }

    void reset(Handle handle = {}) noexcept
    {
        const Handle old{std::exchange(handle_, handle)};
        if (old)
        {
            deleter_(old);
        }
    }

private:
    Handle handle_;
    [[no_unique_address]] Deleter deleter_;
};
} // namespace nori::core

namespace std
{
template <typename Tag>
struct hash<nori::core::basic_handle<Tag>>
{
    std::size_t operator()(nori::core::basic_handle<Tag> handle) const noexcept
    {
        const std::uint64_t value{(static_cast<std::uint64_t>(handle.generation) << 32) + handle.index};
        return std::hash<std::uint64_t>{}(value);
    }
};
} // namespace std
