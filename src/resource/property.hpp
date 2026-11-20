#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include <nori/resource/value.hpp>

namespace nori::resource
{
struct property
{
    using value_type = std::variant<
        empty_t,
        bool,
        std::int32_t,
        std::int64_t,
        float,
        std::string,
        image,
        binary
    >;

    std::string name;
    value_type value;
};

[[nodiscard]] property_type type_of(const property::value_type& value) noexcept;
} // namespace nori::resource
