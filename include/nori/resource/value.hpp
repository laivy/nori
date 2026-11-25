#pragma once
#include <concepts>
#include <cstdint>
#include <string>

#include <nori/resource/binary.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/model.hpp>

namespace nori::resource
{
struct empty_t
{
    constexpr bool operator==(const empty_t&) const = default;
};

inline constexpr empty_t empty{};

enum class property_type : std::uint8_t
{
    empty,
    boolean,
    int32,
    int64,
    float32,
    string,
    image,
    binary
};

template <typename T>
concept gettable =
    std::same_as<T, bool> ||
    std::same_as<T, std::int32_t> ||
    std::same_as<T, std::int64_t> ||
    std::same_as<T, float> ||
    std::same_as<T, std::string> ||
    std::same_as<T, image> ||
    std::same_as<T, image_view> ||
    std::same_as<T, binary> ||
    std::same_as<T, binary_view> ||
    std::same_as<T, model>;
} // namespace nori::resource
