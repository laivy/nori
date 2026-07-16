#include <fstream>
#include "serialize.hpp"

namespace
{
template <typename T>
void write(std::ostream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
} // namespace

namespace nori::resource
{
serializer::serializer() :
    header_{}
{
}

serializer& serializer::path(const std::filesystem::path& path)
{
    path_ = path;
    return *this;
}

serializer& serializer::header(file::header_data header)
{
    header_ = header;
    return *this;
}

serializer& serializer::hierarchies(std::span<const file::hierarchy_entry> hierarchies)
{
    hierarchies_ = hierarchies;
    return *this;
}

serializer& serializer::chunks(std::span<const file::chunk_entry> chunks)
{
    chunks_ = chunks;
    return *this;
}

serializer& serializer::binary(std::span<const std::byte> binary)
{
    binary_ = binary;
    return *this;
}

result<void> serializer::execute() const
{
    std::ofstream out{path_, std::ios::binary};
    if (!out)
    {
        return std::unexpected{error_code::io_failure};
    }

    write(out, header_.signature);
    write(out, header_.version);

    write(out, static_cast<std::uint32_t>(hierarchies_.size()));
    for (const file::hierarchy_entry& hierarchy : hierarchies_)
    {
        write(out, hierarchy.name_pos);
        write(out, static_cast<std::uint32_t>(hierarchy.child_indices.size()));
        out.write(
            reinterpret_cast<const char*>(hierarchy.child_indices.data()),
            sizeof(std::uint32_t) * hierarchy.child_indices.size()
        );
    }

    for (const file::chunk_entry& chunk : chunks_)
    {
        write(out, chunk.name_pos);
        write(out, chunk.value_pos);
    }

    out.write(reinterpret_cast<const char*>(binary_.data()), binary_.size());
    if (!out.good())
    {
        return std::unexpected{error_code::io_failure};
    }
    return {};
}
} // namespace nori::resource
