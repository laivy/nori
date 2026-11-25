#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include <nori/core/abort.hpp>
#include <nori/resource/operations.hpp>

#include "context.hpp"
#include "model.hpp"

namespace
{
std::unique_ptr<nori::resource::context> instance;
} // namespace

namespace nori::resource
{
bool initialize(std::filesystem::path root_path)
{
    auto normalized{normalize_root_path(std::move(root_path))};
    if (normalized.empty())
    {
        return false;
    }
    instance = std::make_unique<context>(std::move(normalized));
    return true;
}

void shutdown() noexcept
{
    instance.reset();
}

handle create_root()
{
    return instance->create_root();
}

handle create(handle parent, std::string_view name, empty_t value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = value});
}

handle create(handle parent, std::string_view name, bool value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = value});
}

handle create(handle parent, std::string_view name, std::int32_t value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = value});
}

handle create(handle parent, std::string_view name, std::int64_t value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = value});
}

handle create(handle parent, std::string_view name, float value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = value});
}

handle create(handle parent, std::string_view name, const char* value)
{
    return create(parent, name, std::string_view{value});
}

handle create(handle parent, std::string_view name, std::string_view value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = std::string{value}});
}

handle create(handle parent, std::string_view name, image value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = std::move(value)});
}

handle create(handle parent, std::string_view name, binary value)
{
    return instance->create(parent, property{.name = std::string{name}, .value = std::move(value)});
}

result<handle> copy(handle source, handle destination)
{
    return instance->copy(source, destination);
}

result<handle> move(handle source, handle destination)
{
    return instance->move(source, destination);
}

void remove(handle target)
{
    instance->remove(target);
}

result<void> reorder(handle target, std::size_t position)
{
    return instance->reorder(target, position);
}

result<void> rename(handle target, std::string_view name)
{
    return instance->rename(target, name);
}

void set(handle target, empty_t value)
{
    instance->set(target, value);
}

void set(handle target, bool value)
{
    instance->set(target, value);
}

void set(handle target, std::int32_t value)
{
    instance->set(target, value);
}

void set(handle target, std::int64_t value)
{
    instance->set(target, value);
}

void set(handle target, float value)
{
    instance->set(target, value);
}

void set(handle target, const char* value)
{
    set(target, std::string_view{value});
}

void set(handle target, std::string_view value)
{
    instance->set(target, std::string{value});
}

void set(handle target, const image& value)
{
    instance->set(target, value);
}

void set(handle target, image&& value)
{
    instance->set(target, std::move(value));
}

void set(handle target, const binary& value)
{
    instance->set(target, value);
}

void set(handle target, binary&& value)
{
    instance->set(target, std::move(value));
}

std::string name_of(handle target)
{
    return instance->name_of(target);
}

path path_of(handle target)
{
    return instance->path_of(target);
}

property_type type_of(handle target)
{
    return instance->type_of(target);
}

handle get(const path& target)
{
    if (const auto res{try_get(target)})
    {
        return *res;
    }
    core::abort("fail to get resource");
}

handle get(handle parent, const path& subpath)
{
    if (const auto res{try_get(parent, subpath)})
    {
        return *res;
    }
    core::abort("fail to get resource");
}

result<handle> try_get(const path& target)
{
    return instance->try_get(target);
}

result<handle> try_get(handle parent, const path& subpath)
{
    return instance->try_get(parent, subpath);
}

handle root_of(handle target)
{
    return instance->root_of(target);
}

handle parent_of(handle target)
{
    return instance->parent_of(target);
}

result<std::vector<handle>> children_of(handle target)
{
    return instance->children_of(target);
}

result<std::size_t> position_of(handle target)
{
    return instance->position_of(target);
}

template <gettable T>
T get(handle target)
{
    if (auto res{try_get<T>(target)})
    {
        return std::move(*res);
    }
    core::abort("fail to get resource");
}

template <gettable T>
T get(const path& target)
{
    if (auto res{try_get<T>(target)})
    {
        return std::move(*res);
    }
    core::abort("fail to get resource");
}

template <gettable T>
T get(handle parent, const path& subpath)
{
    if (auto res{try_get<T>(parent, subpath)})
    {
        return std::move(*res);
    }
    core::abort("fail to get resource");
}

template <gettable T>
result<T> try_get(handle target)
{
    if constexpr (std::same_as<T, model>)
    {
        return read_model(target);
    }
    else
    {
        return instance->try_get<T>(target);
    }
}

template <gettable T>
result<T> try_get(const path& target)
{
    const auto found{try_get(target)};
    if (!found)
    {
        return std::unexpected{found.error()};
    }
    return try_get<T>(*found);
}

template <gettable T>
result<T> try_get(handle parent, const path& subpath)
{
    const auto found{try_get(parent, subpath)};
    if (!found)
    {
        return std::unexpected{found.error()};
    }
    return try_get<T>(*found);
}

#define NORI_INSTANTIATE_ACCESSORS(T) \
    template T get<T>(handle); \
    template T get<T>(const path&); \
    template T get<T>(handle, const path&); \
    template result<T> try_get<T>(handle); \
    template result<T> try_get<T>(const path&); \
    template result<T> try_get<T>(handle, const path&)

NORI_INSTANTIATE_ACCESSORS(bool);
NORI_INSTANTIATE_ACCESSORS(std::int32_t);
NORI_INSTANTIATE_ACCESSORS(std::int64_t);
NORI_INSTANTIATE_ACCESSORS(float);
NORI_INSTANTIATE_ACCESSORS(std::string);
NORI_INSTANTIATE_ACCESSORS(image);
NORI_INSTANTIATE_ACCESSORS(binary);
NORI_INSTANTIATE_ACCESSORS(image_view);
NORI_INSTANTIATE_ACCESSORS(binary_view);
NORI_INSTANTIATE_ACCESSORS(model);

#undef NORI_INSTANTIATE_ACCESSORS

result<void> save(handle target)
{
    return instance->save(target);
}

result<void> save_as(handle target, const std::filesystem::path& file_path)
{
    return instance->save_as(target, file_path);
}

result<void> load(handle target)
{
    return instance->load(target);
}

result<void> unload(handle target)
{
    return instance->unload(target);
}
} // namespace nori::resource
