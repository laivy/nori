#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace nori::core
{
template <typename T, typename Base = void>
class rtti;

class type_info
{
    template <typename T, typename Base>
    friend class rtti;

public:
    type_info(const type_info&) = delete;
    type_info(type_info&&) = delete;
    type_info& operator=(const type_info&) = delete;
    type_info& operator=(type_info&&) = delete;

    [[nodiscard]] constexpr const type_info* parent() const noexcept
    {
        return parent_;
    }

    [[nodiscard]] constexpr bool operator==(const type_info& other) const noexcept
    {
        return this == &other;
    }

    [[nodiscard]] constexpr bool is_a(const type_info& target) const noexcept
    {
        for (auto current{this}; current; current = current->parent_)
        {
            if (*current == target)
            {
                return true;
            }
        }
        return false;
    }

private:
    explicit constexpr type_info(const type_info* parent) noexcept :
        parent_{parent}
    {
    }

    const type_info* parent_;
};

template <typename T>
concept rtti_type = std::same_as<T, std::remove_cvref_t<T>> && requires(const T& value)
{
    typename T::rtti_registered_type;
    requires std::same_as<T, typename T::rtti_registered_type>;
    { T::static_type() } -> std::same_as<const type_info&>;
    { value.type() } -> std::same_as<const type_info&>;
};

// T must be the actual CRTP-derived type. Base must be a registered direct
// parent in a single public non-virtual inheritance chain. Do not construct
// or slice objects of rtti itself, or override the generated type() manually.
// Runtime queries identify the last registered base of unregistered subclasses.
template <typename T>
class rtti<T, void>
{
public:
    using rtti_registered_type = T;

    virtual ~rtti() = default;

    [[nodiscard]] static constexpr const type_info& static_type() noexcept
    {
        return type_;
    }

    [[nodiscard]] virtual const type_info& type() const noexcept
    {
        return static_type();
    }

    template <rtti_type U>
    [[nodiscard]] bool is_a() const noexcept
    {
        return type().is_a(U::static_type());
    }

private:
    // Non-const storage avoids folding identical read-only type records.
    static inline constinit type_info type_{nullptr};
};

template <typename T, typename Base>
class rtti : public Base
{
    static_assert(rtti_type<Base>);

public:
    using rtti_registered_type = T;
    using Base::Base;

    [[nodiscard]] static constexpr const type_info& static_type() noexcept
    {
        return type_;
    }

    [[nodiscard]] const type_info& type() const noexcept override
    {
        return static_type();
    }

private:
    static inline constinit type_info type_{&Base::static_type()};
};

// Type identity requires one shared metadata address per registered type.
// Shared-library boundaries that duplicate metadata are not supported.
// Downcasts require a live, fully constructed object. Null and mismatches
// return nullptr; upcasts use the ordinary C++ conversion.
template <rtti_type T, typename U>
requires (std::derived_from<std::remove_const_t<U>, T> || (rtti_type<std::remove_const_t<U>> && std::derived_from<T, std::remove_const_t<U>>)) && requires(U* value)
{
    static_cast<std::conditional_t<std::is_const_v<U>, const T, T>*>(value);
}
[[nodiscard]] auto cast(U* value) noexcept -> std::conditional_t<std::is_const_v<U>, const T, T>*
{
    using result_type = std::conditional_t<std::is_const_v<U>, const T, T>;

    if constexpr (std::derived_from<std::remove_const_t<U>, T>)
    {
        return static_cast<result_type*>(value);
    }
    else
    {
        if (!value || !value->type().is_a(T::static_type()))
        {
            return nullptr;
        }
        return static_cast<result_type*>(value);
    }
}

template <rtti_type T>
[[nodiscard]] T* cast(std::nullptr_t) noexcept
{
    return nullptr;
}
}
