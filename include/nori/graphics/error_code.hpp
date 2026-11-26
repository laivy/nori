#pragma once
#include <expected>
#include <string_view>

namespace nori::graphics
{
enum class error_code
{
    invalid_argument,
    unsupported,
    backend_initialization_failed,
    descriptor_allocation_failed,
    resource_creation_failed,
    command_failed,
    synchronization_failed,
    swap_chain_resize_failed,
    swap_chain_invalidated,
    presentation_failed
};

template <class T>
using result = std::expected<T, error_code>;

constexpr std::string_view to_string(error_code error) noexcept
{
    switch (error)
    {
    case error_code::invalid_argument:
    {
        return "invalid graphics argument";
    }
    case error_code::unsupported:
    {
        return "graphics operation is unsupported";
    }
    case error_code::backend_initialization_failed:
    {
        return "graphics backend initialization failed";
    }
    case error_code::descriptor_allocation_failed:
    {
        return "descriptor allocation failed";
    }
    case error_code::resource_creation_failed:
    {
        return "graphics resource creation failed";
    }
    case error_code::command_failed:
    {
        return "graphics command failed";
    }
    case error_code::synchronization_failed:
    {
        return "graphics synchronization failed";
    }
    case error_code::swap_chain_resize_failed:
    {
        return "swap chain resize failed with back buffers retained";
    }
    case error_code::swap_chain_invalidated:
    {
        return "swap chain is invalidated";
    }
    case error_code::presentation_failed:
    {
        return "graphics presentation failed";
    }
    default:
        return "unknown graphics error";
    }
}
} // namespace nori::graphics
