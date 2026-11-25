#include <nori/asset.hpp>

#include "file_io.hpp"

#ifdef _WIN32
#include <array>
#include <optional>
#include <string>
#include <utility>

#include <Windows.h>
// clang-format off
#include <wrl.h>
#include <dxcapi.h>
// clang-format on
#include <nori/core/encoding.hpp>

namespace
{
enum class shader_stage
{
    vertex,
    pixel,
    compute
};

struct module_guard
{
    ~module_guard()
    {
        if (handle)
        {
            ::FreeLibrary(handle);
        }
    }

    HMODULE handle{};
};

std::optional<shader_stage> classify_shader_stage(const std::filesystem::path& path)
{
    const std::string filename{nori::core::path_to_utf8(path.filename())};
    if (filename.ends_with(".vs.hlsl"))
    {
        return shader_stage::vertex;
    }
    if (filename.ends_with(".ps.hlsl"))
    {
        return shader_stage::pixel;
    }
    if (filename.ends_with(".cs.hlsl"))
    {
        return shader_stage::compute;
    }
    return std::nullopt;
}

std::wstring profile(const shader_stage stage)
{
    switch (stage)
    {
    case shader_stage::vertex:
    {
        return L"vs_6_0";
    }
    case shader_stage::pixel:
    {
        return L"ps_6_0";
    }
    case shader_stage::compute:
    {
        return L"cs_6_0";
    }
    default:
        break;
    }
    return {};
}

std::string blob_string(IDxcBlobUtf8* blob)
{
    return !blob || blob->GetStringLength() == 0 ? std::string{} : std::string{blob->GetStringPointer(), blob->GetStringLength()};
}
} // namespace
#endif

namespace nori::asset
{
result<resource::binary> cook_shader(const std::filesystem::path& source_path)
{
#ifdef _WIN32
    const std::optional<shader_stage> stage{classify_shader_stage(source_path)};
    if (!stage)
    {
        return std::unexpected{failure{.code = error_code::invalid_format}};
    }
    const auto source_data{read_file(source_path)};
    if (!source_data)
    {
        return std::unexpected{source_data.error()};
    }

    const std::string source{reinterpret_cast<const char*>(source_data->data()), source_data->size()};
    std::filesystem::path include_directory{source_path.parent_path()};
    if (include_directory.empty())
    {
        include_directory = std::filesystem::current_path();
    }

    module_guard module{::LoadLibraryW(L"dxcompiler.dll")};
    if (!module.handle)
    {
        return std::unexpected{failure{.code = error_code::unsupported_platform, .diagnostics = "dxcompiler.dll is unavailable."}};
    }

    Microsoft::WRL::ComPtr<IDxcUtils> utils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
    if (FAILED(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) || FAILED(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
    {
        return std::unexpected{failure{.code = error_code::unsupported_platform, .diagnostics = "DXC could not be initialized."}};
    }

    const std::wstring target{profile(*stage)};
    const std::wstring include_path{include_directory.wstring()};
    std::array arguments{
        L"-E",
        L"main",
        L"-T",
        target.c_str(),
        L"-HV",
        L"2021",
        L"-WX",
        L"-I",
        include_path.c_str(),
#ifndef NDEBUG
        L"-Zi",
        L"-Od",
        L"-Qembed_debug",
#else
        L"-O3",
#endif
    };
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler;
    if (FAILED(utils->CreateDefaultIncludeHandler(&include_handler)))
    {
        return std::unexpected{failure{.code = error_code::unsupported_platform, .diagnostics = "DXC include handling could not be initialized."}};
    }

    const DxcBuffer source_buffer{.Ptr = source.data(), .Size = source.size(), .Encoding = DXC_CP_UTF8};
    Microsoft::WRL::ComPtr<IDxcResult> compilation;
    if (FAILED(compiler->Compile(&source_buffer, arguments.data(), static_cast<UINT32>(arguments.size()), include_handler.Get(), IID_PPV_ARGS(&compilation))))
    {
        return std::unexpected{failure{.code = error_code::invalid_format, .diagnostics = "DXC failed to compile the source."}};
    }

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
    compilation->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    std::string diagnostics{blob_string(errors.Get())};
    HRESULT status{S_OK};
    if (FAILED(compilation->GetStatus(&status)) || FAILED(status))
    {
        return std::unexpected{failure{.code = error_code::invalid_format, .diagnostics = std::move(diagnostics)}};
    }

    Microsoft::WRL::ComPtr<IDxcBlob> object;
    if (FAILED(compilation->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr)) || !object)
    {
        return std::unexpected{failure{.code = error_code::invalid_format, .diagnostics = std::move(diagnostics)}};
    }

    const auto begin{static_cast<const std::byte*>(object->GetBufferPointer())};
    return resource::binary{.data = {begin, begin + object->GetBufferSize()}};
#else
    static_cast<void>(source_path);
    return std::unexpected{failure{.code = error_code::unsupported_platform, .diagnostics = "HLSL compilation is unsupported on this platform."}};
#endif
}
} // namespace nori::asset
