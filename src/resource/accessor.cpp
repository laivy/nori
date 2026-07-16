#include <cassert>
#include <utility>
#include "accessor.hpp"
#include "context.hpp"

namespace nori::resource
{
namespace
{
context& get_context()
{
    context* const value{context::instance()};
    assert(value);
    return *value;
}
} // namespace

handle create(std::string_view name)
{
    return get_context().create(name, std::monostate{});
}

void remove(handle target)
{
    get_context().remove(target);
}

result<void> set_name(handle target, std::string_view name)
{
    return get_context().set_name(target, name);
}

void set_value(handle target, std::int32_t value)
{
    get_context().set_value(target, value);
}

void set_value(handle target, std::int64_t value)
{
    get_context().set_value(target, value);
}

void set_value(handle target, float value)
{
    get_context().set_value(target, value);
}

void set_value(handle target, const char* value)
{
    set_value(target, std::string{value});
}

void set_value(handle target, const std::string& value)
{
    get_context().set_value(target, value);
}

void set_value(handle target, const image_asset& value)
{
    get_context().set_value(target, value);
}

result<void> set_parent(handle child, handle parent)
{
    return get_context().set_parent(child, parent);
}

handle get(std::string_view path)
{
    return get_context().get(path);
}

handle get_parent(handle target)
{
    return get_context().get_parent(target);
}

std::vector<handle> get_children(handle target)
{
    return get_context().get_children(target);
}

std::string get_name(handle target)
{
    return get_context().get_name(target);
}

type get_type(handle target)
{
    return get_context().get_type(target);
}

std::optional<type> get_type(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_type(target)} : std::nullopt;
}

std::optional<type> get_type(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_type(target)} : std::nullopt;
}

std::int32_t get_int32(handle target)
{
    return get_context().get_value<std::int32_t>(target);
}

std::optional<std::int32_t> get_int32(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_int32(target)} : std::nullopt;
}

std::optional<std::int32_t> get_int32(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_int32(target)} : std::nullopt;
}

std::int64_t get_int64(handle target)
{
    return get_context().get_value<std::int64_t>(target);
}

std::optional<std::int64_t> get_int64(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_int64(target)} : std::nullopt;
}

std::optional<std::int64_t> get_int64(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_int64(target)} : std::nullopt;
}

float get_float(handle target)
{
    return get_context().get_value<float>(target);
}

std::optional<float> get_float(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_float(target)} : std::nullopt;
}

std::optional<float> get_float(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_float(target)} : std::nullopt;
}

std::string get_string(handle target)
{
    return get_context().get_value<std::string>(target);
}

std::optional<std::string> get_string(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_string(target)} : std::nullopt;
}

std::optional<std::string> get_string(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_string(target)} : std::nullopt;
}

image get_image(handle target)
{
    return get_context().get_value<image>(target);
}

std::optional<image> get_image(std::string_view path)
{
    const handle target{get(path)};
    return target ? std::optional{get_image(target)} : std::nullopt;
}

std::optional<image> get_image(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? std::optional{get_image(target)} : std::nullopt;
}

const image_asset& get_image_asset(handle target)
{
    return get_context().get_value<image_asset>(target);
}

const image_asset* get_image_asset(std::string_view path)
{
    const handle target{get(path)};
    return target ? &get_image_asset(target) : nullptr;
}

const image_asset* get_image_asset(handle parent, std::string_view path)
{
    const handle target{get_context().get(parent, path)};
    return target ? &get_image_asset(target) : nullptr;
}

result<void> serialize(const std::filesystem::path& path, handle target)
{
    return get_context().serialize(path, target);
}
} // namespace nori::resource
