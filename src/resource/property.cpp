#include <cassert>
#include <type_traits>
#include <variant>

#include "property.hpp"

namespace nori::resource
{
property_type type_of(const property::value_type& value) noexcept
{
    assert(!value.valueless_by_exception());
    return property_type{static_cast<std::underlying_type_t<property_type>>(value.index())};
}
} // namespace nori::resource
