#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include "deserialize.hpp"
#include "path.hpp"

namespace
{
std::optional<std::pair<std::filesystem::path, std::string>> parse_resource_path(std::string_view path)
{
    const std::size_t pos{path.rfind(nori::resource::extension)};
    if (pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    std::filesystem::path file_path{path.substr(0, pos + nori::resource::extension.size())};
    std::string sub_path;
    if (path.size() > pos + nori::resource::extension.size())
    {
        sub_path = path.substr(pos + nori::resource::extension.size() + 1);
    }
    return std::pair{std::move(file_path), std::move(sub_path)};
}

bool can_read(std::istream& in, std::uint64_t size)
{
    const std::streamoff current{in.tellg()};
    if (current < 0)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff end{in.tellg()};
    in.seekg(current);
    return in && end >= current && size <= static_cast<std::uint64_t>(end - current);
}

template <typename T>
T read(std::istream& in)
{
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

template <typename T>
requires std::is_same_v<T, std::string>
T read(std::istream& in)
{
    const std::uint32_t size{read<std::uint32_t>(in)};
    if (!in || !can_read(in, size))
    {
        in.setstate(std::ios::failbit);
        return {};
    }

    std::string value(size, '\0');
    in.read(reinterpret_cast<char*>(value.data()), size);
    return value;
}

template <typename T>
requires std::is_same_v<T, nori::resource::image_asset>
T read(std::istream& in)
{
    T value{};
    const std::uint8_t format{read<std::uint8_t>(in)};
    if (format > static_cast<std::uint8_t>(nori::resource::image_format::r8g8b8a8))
    {
        in.setstate(std::ios::failbit);
        return {};
    }
    value.img.format = static_cast<nori::resource::image_format>(format);
    value.img.width = read<std::uint32_t>(in);
    value.img.height = read<std::uint32_t>(in);

    const std::uint64_t size{read<std::uint64_t>(in)};
    if (!in || !can_read(in, size) || size > std::numeric_limits<std::size_t>::max())
    {
        in.setstate(std::ios::failbit);
        return {};
    }
    value.data.resize(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(value.data.data()), static_cast<std::streamsize>(value.data.size()));
    return value;
}

template <typename T>
requires std::is_same_v<T, nori::resource::property::value_type>
T read(std::istream& in)
{
    const std::uint8_t type{read<std::uint8_t>(in)};
    switch (type)
    {
    case 0:
        return std::monostate{};
    case 1:
        return read<std::int32_t>(in);
    case 2:
        return read<std::int64_t>(in);
    case 3:
        return read<float>(in);
    case 4:
        return read<std::string>(in);
    case 5:
        return read<nori::resource::image_asset>(in);
    default:
        in.setstate(std::ios::failbit);
        break;
    }
    return std::monostate{};
}

template <typename T>
requires std::is_same_v<T, nori::resource::file::chunk_entry>
constexpr std::size_t size_of()
{
    return sizeof(T::name_pos) + sizeof(T::value_pos);
}

nori::resource::result<std::vector<nori::resource::file::hierarchy_entry>> read_hierarchies(
    std::istream& in,
    std::uint32_t property_count
)
{
    std::vector<nori::resource::file::hierarchy_entry> hierarchies;
    hierarchies.reserve(property_count);
    for (std::uint32_t i{0}; i < property_count; ++i)
    {
        nori::resource::file::hierarchy_entry& hierarchy{hierarchies.emplace_back()};
        hierarchy.name_pos = read<std::streamoff>(in);

        const std::uint32_t child_count{read<std::uint32_t>(in)};
        if (!in || child_count > property_count)
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }
        hierarchy.child_indices.resize(child_count);
        in.read(
            reinterpret_cast<char*>(hierarchy.child_indices.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * child_count)
        );
        if (!in)
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }
    }
    return hierarchies;
}

bool are_hierarchies_valid(std::span<const nori::resource::file::hierarchy_entry> hierarchies)
{
    const std::size_t property_count{hierarchies.size()};
    std::vector<std::uint32_t> parent_counts(property_count, 0);
    for (const nori::resource::file::hierarchy_entry& hierarchy : hierarchies)
    {
        for (const std::uint32_t child_index : hierarchy.child_indices)
        {
            if (child_index >= property_count || ++parent_counts[child_index] > 1)
            {
                return false;
            }
        }
    }

    std::vector<std::uint32_t> nodes_to_visit;
    nodes_to_visit.reserve(property_count);
    for (std::uint32_t i{0}; i < property_count; ++i)
    {
        if (parent_counts[i] == 0)
        {
            nodes_to_visit.push_back(i);
        }
    }

    std::size_t visited_count{0};
    while (!nodes_to_visit.empty())
    {
        const std::uint32_t current{nodes_to_visit.back()};
        nodes_to_visit.pop_back();
        ++visited_count;
        for (const std::uint32_t child : hierarchies[current].child_indices)
        {
            if (--parent_counts[child] == 0)
            {
                nodes_to_visit.push_back(child);
            }
        }
    }
    if (visited_count != property_count)
    {
        return false;
    }
    return true;
}

std::vector<std::uint32_t> get_top_level_property_indices(std::span<const nori::resource::file::hierarchy_entry> hierarchies)
{
    std::vector<bool> has_parent(hierarchies.size(), false);
    for (const nori::resource::file::hierarchy_entry& hierarchy : hierarchies)
    {
        for (const std::uint32_t child_index : hierarchy.child_indices)
        {
            has_parent[child_index] = true;
        }
    }

    std::vector<std::uint32_t> result;
    result.reserve(hierarchies.size());
    for (std::uint32_t i{0}; i < hierarchies.size(); ++i)
    {
        if (!has_parent[i])
        {
            result.push_back(i);
        }
    }
    return result;
}

nori::resource::result<std::vector<nori::resource::file::chunk_entry>> read_chunks(
    std::istream& in,
    std::streamoff file_size,
    std::span<const nori::resource::file::hierarchy_entry> hierarchies
)
{
    const std::streamoff chunk_begin{in.tellg()};
    const std::uint64_t chunk_size{
        size_of<nori::resource::file::chunk_entry>() * static_cast<std::uint64_t>(hierarchies.size())
    };
    if (chunk_begin < 0 || !can_read(in, chunk_size))
    {
        return std::unexpected{nori::resource::error_code::invalid_format};
    }

    std::vector<nori::resource::file::chunk_entry> chunks(hierarchies.size());
    in.read(reinterpret_cast<char*>(chunks.data()), static_cast<std::streamsize>(chunk_size));
    const std::streamoff binary_begin{in.tellg()};
    if (!in)
    {
        return std::unexpected{nori::resource::error_code::invalid_format};
    }

    const auto is_valid_position = [binary_begin, file_size](std::streamoff position)
    {
        return position >= binary_begin && position < file_size;
    };
    for (std::size_t i{0}; i < hierarchies.size(); ++i)
    {
        const nori::resource::file::hierarchy_entry& hierarchy{hierarchies[i]};
        const nori::resource::file::chunk_entry& chunk{chunks[i]};
        if (!is_valid_position(hierarchy.name_pos) ||
            !is_valid_position(chunk.name_pos) ||
            !is_valid_position(chunk.value_pos) ||
            hierarchy.name_pos != chunk.name_pos)
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }
        in.seekg(chunk.name_pos);
        if (read<std::uint8_t>(in) != 4 /* std::string */ || !in)
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }
    }
    in.seekg(binary_begin);
    return chunks;
}

nori::resource::result<std::vector<std::uint32_t>> find_target_property_indices(
    std::istream& in,
    std::string_view sub_path,
    std::span<const nori::resource::file::hierarchy_entry> hierarchies,
    std::span<const std::uint32_t> top_level_indices
)
{
    if (sub_path.empty())
    {
        std::vector<std::uint32_t> result(hierarchies.size());
        std::iota(result.begin(), result.end(), std::uint32_t{0});
        return result;
    }

    std::optional<std::uint32_t> root_index;
    std::span<const std::uint32_t> candidates{top_level_indices};
    for (const auto subrange : sub_path | std::views::split(nori::resource::delimiter))
    {
        const std::string_view name{subrange};
        const auto it{std::ranges::find(candidates, name, [&in, &hierarchies, backup = in.tellg()](std::uint32_t index)
            {
                const nori::resource::file::hierarchy_entry& hierarchy{hierarchies[index]};
                in.seekg(hierarchy.name_pos + static_cast<std::streamoff>(sizeof(std::uint8_t)));
                const std::string child_name{read<std::string>(in)};
                in.seekg(backup);
                return child_name;
            })};
        if (it == candidates.end())
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }
        root_index = *it;
        candidates = hierarchies[*it].child_indices;
    }
    if (!root_index)
    {
        return std::unexpected{nori::resource::error_code::invalid_format};
    }

    std::vector<std::uint32_t> result;
    std::vector<std::uint32_t> nodes_to_visit{*root_index};
    while (!nodes_to_visit.empty())
    {
        const std::uint32_t index{nodes_to_visit.back()};
        nodes_to_visit.pop_back();
        result.push_back(index);
        for (const std::uint32_t child : hierarchies[index].child_indices)
        {
            nodes_to_visit.push_back(child);
        }
    }
    return result;
}

nori::resource::result<nori::resource::property> read_property(
    std::istream& in,
    const nori::resource::file::chunk_entry& chunk
)
{
    nori::resource::property result{};
    in.seekg(chunk.name_pos + static_cast<std::streamoff>(sizeof(std::uint8_t)));
    result.name = read<std::string>(in);
    in.seekg(chunk.value_pos);
    result.value = read<nori::resource::property::value_type>(in);
    if (!in)
    {
        return std::unexpected{nori::resource::error_code::invalid_format};
    }
    return result;
}

nori::resource::result<nori::resource::deserialized_data> build_deserialized_data(
    std::istream& in,
    std::span<const nori::resource::file::hierarchy_entry> hierarchies,
    std::span<const nori::resource::file::chunk_entry> chunks,
    std::span<const std::uint32_t> selected_indices,
    std::span<const std::uint32_t> top_level_indices,
    bool is_full_file
)
{
    nori::resource::deserialized_data result;
    result.properties.reserve(selected_indices.size());

    // 파일에 저장된 프로퍼티 인덱스를 result.properties 벡터의 인덱스로 매핑
    std::unordered_map<std::uint32_t, std::uint32_t> local_indices;
    local_indices.reserve(selected_indices.size());
    for (const std::uint32_t index : selected_indices)
    {
        auto property_result{read_property(in, chunks[index])};
        if (!property_result)
        {
            return std::unexpected{property_result.error()};
        }
        local_indices.emplace(index, static_cast<std::uint32_t>(result.properties.size()));
        result.properties.push_back(std::move(*property_result));
    }

    // 파일에 저장된 자식 프로퍼티 인덱스를 result.properties 벡터의 인덱스로 변환하여 result.child_indices에 저장
    result.child_indices.reserve(selected_indices.size());
    for (const std::uint32_t index : selected_indices)
    {
        const nori::resource::file::hierarchy_entry& hierarchy{hierarchies[index]};
        std::vector<std::uint32_t> children;
        children.reserve(hierarchy.child_indices.size());
        for (const std::uint32_t child_index : hierarchy.child_indices)
        {
            if (const auto it{local_indices.find(child_index)}; it != local_indices.end())
            {
                children.push_back(it->second);
            }
        }
        result.child_indices.emplace(local_indices.at(index), std::move(children));
    }

    if (is_full_file)
    {
        result.top_level_prop_indices.assign(top_level_indices.begin(), top_level_indices.end());
    }
    else if (!selected_indices.empty())
    {
        result.top_level_prop_indices.push_back(0);
    }
    return result;
}
} // namespace

namespace nori::resource
{
result<deserialized_data> deserialize(std::string_view path)
{
    auto parsed_path{parse_resource_path(path)};
    if (!parsed_path)
    {
        return std::unexpected{error_code::invalid_format};
    }

    const auto& [file_path, sub_path]{*parsed_path};
    std::ifstream in{file_path, std::ios::binary};
    if (!in)
    {
        return std::unexpected{error_code::io_failure};
    }

    const file::header_data header{
        .signature = read<std::array<char, 4>>(in),
        .version = read<std::uint32_t>(in)
    };
    if (!in || header.signature != file::signature || header.version != file::version)
    {
        return std::unexpected{error_code::invalid_format};
    }

    // total_property_count 유효성 검사
    // 파일 크기와 프로퍼티 최소 크기를 이용하여 유효한지 확인
    const std::uint32_t total_property_count{read<std::uint32_t>(in)};
    const std::streamoff metadata_begin{in.tellg()};
    in.seekg(0, std::ios::end);
    const std::streamoff file_size{in.tellg()};
    in.seekg(metadata_begin);
    constexpr std::uint64_t minimum_property_size{sizeof(file::hierarchy_entry::name_pos) + sizeof(std::uint32_t) + size_of<file::chunk_entry>()};
    if (!in || file_size < metadata_begin || static_cast<std::uint64_t>(total_property_count) > static_cast<std::uint64_t>(file_size - metadata_begin) / minimum_property_size)
    {
        return std::unexpected{error_code::invalid_format};
    }

    // 계층 구조 읽기
    auto hierarchy_result{read_hierarchies(in, total_property_count)};
    if (!hierarchy_result)
    {
        return std::unexpected{hierarchy_result.error()};
    }
    const std::vector<file::hierarchy_entry> hierarchies{std::move(*hierarchy_result)};

    // 계층 구조의 부모-자식 관계가 올바른지 확인
    if (!are_hierarchies_valid(hierarchies))
    {
        return std::unexpected{error_code::invalid_format};
    }

    // 청크 읽기
    auto chunk_result{read_chunks(in, file_size, hierarchies)};
    if (!chunk_result)
    {
        return std::unexpected{chunk_result.error()};
    }
    const std::vector<file::chunk_entry> chunks{std::move(*chunk_result)};

    // 역직렬화 대상 프로퍼티 인덱스 취합
    const std::vector<std::uint32_t> top_level_property_indices{get_top_level_property_indices(hierarchies)};
    auto target_result{find_target_property_indices(in, sub_path, hierarchies, top_level_property_indices)};
    if (!target_result)
    {
        return std::unexpected{target_result.error()};
    }
    const std::vector<std::uint32_t> target_property_indices{std::move(*target_result)};

    // 결과
    return build_deserialized_data(in, hierarchies, chunks, target_property_indices, top_level_property_indices, sub_path.empty());
}
} // namespace nori::resource
