#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <nori/resource/error_code.hpp>

#include "../property.hpp"
#include "data_pool.hpp"
#include "format.hpp"
#include "name_table.hpp"
#include "property_table.hpp"

namespace nori::resource::codec
{
class writer
{
public:
    explicit writer(std::filesystem::path file_path);
    ~writer();

    result<void> append(std::string_view name, const property::value_type& value, std::uint32_t depth);
    result<void> finish();

private:
    struct payload
    {
        std::uint32_t index;
        const property::value_type* value;
    };

    result<void> encode_data_pool_value(data_pool::encoder& pool, const payload& pending);

    std::filesystem::path file_path_;
    std::filesystem::path temporary_path_;
    bool owns_temporary_;

    property_table::encoder properties_;
    name_table::encoder names_;
    std::vector<payload> pending_payloads_;
};
} // namespace nori::resource::codec
