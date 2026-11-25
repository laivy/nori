#pragma once
#include <filesystem>
#include <string>
#include <string_view>

#include <nori/io/context.hpp>

#include "command.hpp"

namespace nori::io
{
template <typename T>
class awaitable;
}

namespace nori::editor
{
class context;

class import_workflow
{
public:
    import_workflow(context* ctx, nori::io::context_handle main_context, nori::io::context_handle worker_context);

    void start_image(std::filesystem::path path, resource_request_target request);
    void start_model(std::filesystem::path path, resource_request_target request);
    void start_replacement(std::filesystem::path path, resource_request_target request);

private:
    nori::io::awaitable<void> import_image(std::filesystem::path path, resource_request_target request);
    nori::io::awaitable<void> import_model(std::filesystem::path path, resource_request_target request);
    nori::io::awaitable<void> replace_image(std::filesystem::path path, resource_request_target request);

    void report(nori::resource::error_code error);

    context* context_;
    nori::io::context_handle main_context_;
    nori::io::context_handle worker_context_;
};
} // namespace nori::editor
