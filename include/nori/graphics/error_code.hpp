#pragma once
#include <expected>
#include <string_view>

namespace nori::graphics
{
enum class error_code
{
    not_initialized,
    invalid_argument,
    invalid_handle,
    invalid_state,
    resource_in_use,
    descriptor_allocation_failed,
    resource_creation_failed,
    command_failed,
    synchronization_failed,
    presentation_failed,
    backend_initialization_failed,
    unsupported
};

template <class T>
using result = std::expected<T, error_code>;

constexpr std::string_view to_string(error_code error) noexcept
{
    switch (error)
    {
    case error_code::not_initialized:
        return "graphics runtime is not initialized";
    case error_code::invalid_argument:
        return "invalid graphics argument";
    case error_code::invalid_handle:
        return "invalid graphics handle";
    case error_code::invalid_state:
        return "invalid graphics state";
    case error_code::resource_in_use:
        return "graphics resource is in use";
    case error_code::descriptor_allocation_failed:
        return "descriptor allocation failed";
    case error_code::resource_creation_failed:
        return "graphics resource creation failed";
    case error_code::command_failed:
        return "graphics command failed";
    case error_code::synchronization_failed:
        return "graphics synchronization failed";
    case error_code::presentation_failed:
        return "graphics presentation failed";
    case error_code::backend_initialization_failed:
        return "graphics backend initialization failed";
    case error_code::unsupported:
        return "graphics operation is unsupported";
    }
    return "unknown graphics error";
}
} // namespace nori::graphics
