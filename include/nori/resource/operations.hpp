#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nori/resource/binary.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/path.hpp>
#include <nori/resource/value.hpp>

namespace nori::resource
{
bool initialize(std::filesystem::path root_path = {});
void shutdown() noexcept;

handle create_root();
handle create(handle parent, std::string_view name, empty_t value);
handle create(handle parent, std::string_view name, bool value);
handle create(handle parent, std::string_view name, std::int32_t value);
handle create(handle parent, std::string_view name, std::int64_t value);
handle create(handle parent, std::string_view name, float value);
handle create(handle parent, std::string_view name, const char* value);
handle create(handle parent, std::string_view name, std::string_view value);
handle create(handle parent, std::string_view name, image value);
handle create(handle parent, std::string_view name, binary value);
result<handle> copy(handle source, handle destination);
result<handle> move(handle source, handle destination);
void remove(handle target);

result<void> reorder(handle target, std::size_t position);
result<void> rename(handle target, std::string_view name);

void set(handle target, empty_t value);
void set(handle target, bool value);
void set(handle target, std::int32_t value);
void set(handle target, std::int64_t value);
void set(handle target, float value);
void set(handle target, const char* value);
void set(handle target, std::string_view value);
void set(handle target, const image& value);
void set(handle target, image&& value);
void set(handle target, const binary& value);
void set(handle target, binary&& value);

std::string name_of(handle target);
path path_of(handle target);
property_type type_of(handle target);

handle get(const path& target);
handle get(handle parent, const path& subpath);
result<handle> try_get(const path& target);
result<handle> try_get(handle parent, const path& subpath);
handle root_of(handle target);
handle parent_of(handle target);
result<std::vector<handle>> children_of(handle target);
result<std::size_t> position_of(handle target);

template <gettable T>
T get(handle target);

template <gettable T>
T get(const path& target);

template <gettable T>
T get(handle parent, const path& subpath);

template <gettable T>
result<T> try_get(handle target);

template <gettable T>
result<T> try_get(const path& target);

template <gettable T>
result<T> try_get(handle parent, const path& subpath);

result<void> save(handle target);
result<void> save_as(handle target, const std::filesystem::path& file_path);

result<void> load(handle target);
result<void> unload(handle target);
} // namespace nori::resource
