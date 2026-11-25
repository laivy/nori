#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

namespace
{
inline std::vector<nori::resource::handle> children_of_or_fail(nori::resource::handle target)
{
    auto res{nori::resource::children_of(target)};
    EXPECT_TRUE(res) << (res ? "" : nori::resource::to_string(res.error()));
    return res ? std::move(*res) : std::vector<nori::resource::handle>{};
}

template <typename T>
void write_value(std::ostream& out, const T& value)
{
    using U = std::make_unsigned_t<T>;
    const U bits{static_cast<U>(value)};
    for (std::size_t i{0}; i < sizeof(T); ++i)
    {
        out.put(static_cast<char>((bits >> (i * 8)) & 0xFFu));
    }
}

template <typename T>
T read_value(std::istream& in)
{
    using U = std::make_unsigned_t<T>;
    U bits{0};
    for (std::size_t i{0}; i < sizeof(T); ++i)
    {
        const auto byte{static_cast<U>(static_cast<unsigned char>(in.get()))};
        bits |= byte << (i * 8);
    }
    return static_cast<T>(bits);
}

inline void write_single_property_file(const std::filesystem::path& path, std::uint32_t version, std::uint32_t subtree_end, std::uint8_t value_type, std::string_view name = "value", std::int32_t int32_value = 0)
{
    std::ofstream out{path, std::ios::binary};
    constexpr std::array signature{'n', 'o', 'r', 'i'};
    out.write(signature.data(), signature.size());
    write_value(out, version);
    write_value(out, std::uint32_t{1});

    write_value(out, std::uint32_t{1});
    write_value(out, std::uint64_t{56});
    write_value(out, std::uint64_t{84});
    write_value(out, static_cast<std::uint64_t>(2 + name.size()));
    write_value(out, static_cast<std::uint64_t>(86 + name.size()));
    write_value(out, std::uint64_t{0});
    write_value(out, std::uint32_t{0});
    write_value(out, subtree_end);
    write_value(out, static_cast<std::uint32_t>(value_type));
    write_value(out, int32_value);
    write_value(out, std::uint32_t{0});
    write_value(out, std::uint64_t{0});
    write_value(out, static_cast<std::uint16_t>(name.size()));
    out.write(name.data(), static_cast<std::streamsize>(name.size()));
}

class resource_test : public testing::Test
{
protected:
    resource_test() :
        base_directory_{std::filesystem::temp_directory_path() / "nori-resource-tests"}
    {
    }

    void SetUp() override
    {
        const ::testing::TestInfo* info{::testing::UnitTest::GetInstance()->current_test_info()};
        std::string name{info != nullptr ? std::string{info->test_suite_name()} + "." + info->name() : "unknown"};
        for (char& c : name)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            {
                c = '_';
            }
        }
        directory_ = base_directory_ / name;
        std::error_code ec;
        std::filesystem::remove_all(directory_, ec);
        std::filesystem::create_directories(directory_);
        ASSERT_TRUE(nori::resource::initialize(directory_));
    }

    void TearDown() override
    {
        nori::resource::shutdown();
        std::error_code ec;
        std::filesystem::remove_all(directory_, ec);
    }

    std::filesystem::path base_directory_;
    std::filesystem::path directory_;
};

class value_test : public resource_test
{
};

class structure_test : public resource_test
{
};

class loading_test : public resource_test
{
};

class save_test : public resource_test
{
};

class path_resolution_test : public resource_test
{
};

class lifecycle_test : public testing::Test
{
protected:
    void TearDown() override
    {
        nori::resource::shutdown();
    }
};

// ASSERT_* 조기 리턴 시에도 remove_all이 실행되도록 RAII로 감싼 임시 디렉토리.
class scoped_temp_dir
{
public:
    explicit scoped_temp_dir(std::filesystem::path path) :
        path_{std::move(path)}
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~scoped_temp_dir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    scoped_temp_dir(const scoped_temp_dir&) = delete;
    scoped_temp_dir& operator=(const scoped_temp_dir&) = delete;

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

inline std::vector<std::string> path_segments(const nori::resource::path& value)
{
    std::vector<std::string> result;
    for (const std::string_view segment : nori::resource::subpath_iterator{value})
    {
        result.emplace_back(segment);
    }
    return result;
}

inline nori::resource::path bound_resource_path(const std::filesystem::path& file_path, std::string_view logical_path = {})
{
    std::error_code ec;
    const std::filesystem::path normalized{std::filesystem::weakly_canonical(file_path, ec)};
    if (ec)
    {
        ADD_FAILURE() << "weakly_canonical failed: " << ec.message();
        return {};
    }

    nori::resource::path result{normalized};
    if (!logical_path.empty())
    {
        result = result / nori::resource::path{logical_path};
    }
    return result;
}

template <typename Path>
nori::resource::handle create_resource(Path&& value)
{
    const nori::resource::path target{std::forward<Path>(value)};
    if (!target.has_file_path())
    {
        ADD_FAILURE() << "resource path has no file path";
        return {};
    }

    if (const auto found{nori::resource::try_get(target)})
    {
        auto root{*found};
        while (const auto parent{nori::resource::parent_of(root)})
        {
            root = parent;
        }
        if (const auto res{nori::resource::load(root)}; !res)
        {
            ADD_FAILURE() << "load failed: " << nori::resource::to_string(res.error());
            return {};
        }
        return *found;
    }
    else if (found.error() != nori::resource::error_code::not_found)
    {
        ADD_FAILURE() << "try_get failed: " << nori::resource::to_string(found.error());
        return {};
    }

    const nori::resource::path root_path{target.file_path()};
    nori::resource::handle root{};
    if (const auto found{nori::resource::try_get(root_path)})
    {
        root = *found;
    }
    else
    {
        if (found.error() != nori::resource::error_code::not_found)
        {
            ADD_FAILURE() << "try_get root failed: " << nori::resource::to_string(found.error());
            return {};
        }
        const nori::resource::handle created{nori::resource::create_root()};
        root = created;
        if (const auto res{nori::resource::save_as(root, root_path.file_path())}; !res)
        {
            ADD_FAILURE() << "save_as failed: " << nori::resource::to_string(res.error());
            nori::resource::remove(root);
            return {};
        }
    }

    if (const auto res{nori::resource::load(root)}; !res)
    {
        ADD_FAILURE() << "load failed: " << nori::resource::to_string(res.error());
        return {};
    }
    nori::resource::handle current{root};
    for (const std::string_view name : nori::resource::subpath_iterator{target})
    {
        if (const auto res{nori::resource::try_get(current, name)})
        {
            current = *res;
            continue;
        }
        else if (res.error() != nori::resource::error_code::not_found)
        {
            ADD_FAILURE() << "try_get child failed: " << nori::resource::to_string(res.error());
            return {};
        }

        const nori::resource::handle created{nori::resource::create(current, name, nori::resource::empty)};
        if (!created)
        {
            ADD_FAILURE() << "create failed: " << nori::resource::to_string(nori::resource::error_code::name_conflict);
            return {};
        }
        current = created;
    }
    return current;
}
} // namespace
