#pragma once
#include <cstddef>
#include <filesystem>
#include <span>
#include <nori/resource/error_code.hpp>
#include "format.hpp"

namespace nori::resource
{
class serializer
{
public:
    serializer();

    serializer& path(const std::filesystem::path& path);
    serializer& header(file::header_data header);
    serializer& hierarchies(std::span<const file::hierarchy_entry> hierarchies);
    serializer& chunks(std::span<const file::chunk_entry> chunks);
    serializer& binary(std::span<const std::byte> binary);
    result<void> execute() const;

private:
    std::filesystem::path path_;
    file::header_data header_;
    std::span<const file::hierarchy_entry> hierarchies_;
    std::span<const file::chunk_entry> chunks_;
    std::span<const std::byte> binary_;
};
} // namespace nori::resource
