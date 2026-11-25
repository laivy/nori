#pragma once
#include <filesystem>
#include <shared_mutex>
#include <unordered_map>

#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include <nori/resource/path.hpp>

namespace nori::resource
{
class file_registry
{
public:
    explicit file_registry(std::filesystem::path root_path);

    [[nodiscard]] handle bind(handle root, const std::filesystem::path& file_path);
    void unbind(handle root);

    [[nodiscard]] std::filesystem::path file_path_of(handle root) const;
    [[nodiscard]] handle find(const std::filesystem::path& file_path) const;

    [[nodiscard]] result<std::filesystem::path> absolute(const std::filesystem::path& file_path) const;

private:
    std::filesystem::path root_path_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<handle, std::filesystem::path> files_;
};

[[nodiscard]] std::filesystem::path normalize_root_path(std::filesystem::path p);
} // namespace nori::resource
