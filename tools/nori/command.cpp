#include <charconv>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>
#include <nori/resource.hpp>
#include "command.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace
{
constexpr std::string_view red{"\x1b[31m"};
constexpr std::string_view green{"\x1b[32m"};
constexpr std::string_view blue{"\x1b[34m"};
constexpr std::string_view reset{"\x1b[0m"};

using value = std::variant<std::int32_t, std::int64_t, float, std::string, nori::resource::image_asset>;

#ifdef _WIN32
bool is_png_path(std::string_view path)
{
    const std::filesystem::path filesystem_path{path};
    std::string extension{filesystem_path.extension().string()};
    for (char& ch : extension)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return extension == ".png";
}

class com_initializer
{
public:
    com_initializer()
    {
        result_ = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    ~com_initializer()
    {
        if (SUCCEEDED(result_))
        {
            ::CoUninitialize();
        }
    }

    [[nodiscard]]
    bool is_available() const
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{E_FAIL};
};

std::optional<std::wstring> utf8_to_wide(std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }

    const int size{
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)
    };
    if (size == 0)
    {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size
        ) == 0)
    {
        return std::nullopt;
    }
    return result;
}

std::optional<nori::resource::image_asset> load_png_image(std::string_view path)
{
    if (!is_png_path(path))
    {
        return std::nullopt;
    }

    const std::optional<std::wstring> wide_path{utf8_to_wide(path)};
    if (!wide_path)
    {
        return std::nullopt;
    }

    const com_initializer com;
    if (!com.is_available())
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(
            ::CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf())
            )
        ))
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(
            wide_path->c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()
        )))
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
    {
        return std::nullopt;
    }

    UINT width{};
    UINT height{};
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0)
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())))
    {
        return std::nullopt;
    }
    if (FAILED(converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom
        )))
    {
        return std::nullopt;
    }

    constexpr UINT bytes_per_pixel{4};
    if (width > std::numeric_limits<UINT>::max() / bytes_per_pixel)
    {
        return std::nullopt;
    }
    const UINT stride{width * bytes_per_pixel};
    const std::uint64_t data_size{static_cast<std::uint64_t>(stride) * height};
    if (data_size > static_cast<std::uint64_t>(std::numeric_limits<UINT>::max()) ||
        data_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        return std::nullopt;
    }

    nori::resource::image_asset asset{
        .img = {.format = nori::resource::image_format::r8g8b8a8, .width = width, .height = height},
        .data = std::vector<std::byte>(static_cast<std::size_t>(data_size))
    };
    if (FAILED(converter->CopyPixels(
            nullptr, stride, static_cast<UINT>(asset.data.size()), reinterpret_cast<BYTE*>(asset.data.data())
        )))
    {
        return std::nullopt;
    }

    return asset;
}
#else
std::optional<nori::resource::image_asset> load_png_image(std::string_view)
{
    return std::nullopt;
}
#endif

std::optional<std::string_view> get_file_path(std::string_view path)
{
    const std::size_t pos{path.find(nori::resource::extension)};
    if (pos == std::string_view::npos)
    {
        return std::nullopt;
    }
    return path.substr(0, pos + nori::resource::extension.size());
}

std::string type_to_string(nori::resource::type type)
{
    switch (type)
    {
    case nori::resource::type::folder:
        return "Folder";
    case nori::resource::type::int32:
        return "Int32";
    case nori::resource::type::int64:
        return "Int64";
    case nori::resource::type::float32:
        return "Float";
    case nori::resource::type::string:
        return "String";
    case nori::resource::type::image:
        return "Image";
    default:
        break;
    }
    return "None";
}

std::string value_to_string(nori::resource::handle handle)
{
    switch (nori::resource::get_type(handle))
    {
    case nori::resource::type::folder:
    {
        return "Folder";
    }
    case nori::resource::type::int32:
    {
        return std::to_string(nori::resource::get_int32(handle));
    }
    case nori::resource::type::int64:
    {
        return std::to_string(nori::resource::get_int64(handle));
    }
    case nori::resource::type::float32:
    {
        return std::to_string(nori::resource::get_float(handle));
    }
    case nori::resource::type::string:
    {
        return std::format("\"{}\"", nori::resource::get_string(handle));
    }
    case nori::resource::type::image:
    {
        const nori::resource::image img{nori::resource::get_image(handle)};
        return std::format("{}x{}", img.width, img.height);
    }
    default:
        break;
    }
    return "None";
}

std::string type_and_value_to_string(nori::resource::handle handle)
{
    return std::format("{}: {}", type_to_string(nori::resource::get_type(handle)), value_to_string(handle));
}

std::optional<value> parse_value(std::string_view arg)
{
    if (arg.starts_with('@'))
    {
        return load_png_image(arg.substr(1));
    }

    if (arg.size() >= 2 && arg.front() == '\'' && arg.back() == '\'')
    {
        return std::string{arg.substr(1, arg.size() - 2)};
    }

    std::int32_t int32_value{};
    if (auto [ptr, ec]{std::from_chars(arg.data(), arg.data() + arg.size(), int32_value)};
        ec == std::errc{} && ptr == arg.data() + arg.size())
    {
        return int32_value;
    }

    std::int64_t int64_value{};
    if (auto [ptr, ec]{std::from_chars(arg.data(), arg.data() + arg.size(), int64_value)};
        ec == std::errc{} && ptr == arg.data() + arg.size())
    {
        return int64_value;
    }

    float float_value{};
    if (auto [ptr, ec]{std::from_chars(arg.data(), arg.data() + arg.size(), float_value)};
        ec == std::errc{} && ptr == arg.data() + arg.size())
    {
        return float_value;
    }

    return std::nullopt;
}

std::map<std::string, nori::resource::handle> get_children_by_name(nori::resource::handle handle)
{
    std::map<std::string, nori::resource::handle> children;
    for (const nori::resource::handle child : nori::resource::get_children(handle))
    {
        children.emplace(nori::resource::get_name(child), child);
    }
    return children;
}

enum class diff_state
{
    same,
    added,
    added_descendant,
    removed,
    changed,
    type_changed
};

struct node
{
    std::string name;
    diff_state state;
    std::string detail;
    std::vector<node> children;
};

node create_subtree(nori::resource::handle handle, diff_state current_state, bool is_root = true)
{
    node result{.name = nori::resource::get_name(handle), .state = diff_state::same};

    switch (current_state)
    {
    case diff_state::added:
        result.state = is_root ? diff_state::added : diff_state::added_descendant;
        break;
    case diff_state::removed:
        result.state = diff_state::removed;
        return result;
    default:
        result.state = current_state;
        break;
    }

    for (const nori::resource::handle child : nori::resource::get_children(handle))
    {
        result.children.push_back(create_subtree(child, current_state, false));
    }
    return result;
}

std::optional<node> create_diff_tree(nori::resource::handle lhs, nori::resource::handle rhs)
{
    node result{.name = nori::resource::get_name(lhs), .state = diff_state::same};

    const nori::resource::type lhs_type{nori::resource::get_type(lhs)};
    const nori::resource::type rhs_type{nori::resource::get_type(rhs)};
    if (lhs_type != rhs_type)
    {
        result.state = diff_state::type_changed;
        result.detail = std::format("{} -> {}", type_and_value_to_string(lhs), type_and_value_to_string(rhs));
    }
    else if (lhs_type != nori::resource::type::folder && value_to_string(lhs) != value_to_string(rhs))
    {
        result.state = diff_state::changed;
        result.detail = std::format("{} -> {}", type_and_value_to_string(lhs), type_and_value_to_string(rhs));
    }

    const std::map<std::string, nori::resource::handle> lhs_children{get_children_by_name(lhs)};
    const std::map<std::string, nori::resource::handle> rhs_children{get_children_by_name(rhs)};
    for (const auto& [name, lhs_child] : lhs_children)
    {
        if (const auto rhs_it{rhs_children.find(name)}; rhs_it != rhs_children.end())
        {
            if (std::optional<node> child{create_diff_tree(lhs_child, rhs_it->second)})
            {
                result.children.push_back(std::move(*child));
            }
        }
        else
        {
            result.children.push_back(create_subtree(lhs_child, diff_state::removed));
        }
    }
    for (const auto& [name, rhs_child] : rhs_children)
    {
        if (!lhs_children.contains(name))
        {
            result.children.push_back(create_subtree(rhs_child, diff_state::added));
        }
    }

    if (result.state == diff_state::same && result.children.empty())
    {
        return std::nullopt;
    }
    return result;
}

std::string format_node(const node& diff_node)
{
    switch (diff_node.state)
    {
    case diff_state::added:
        return std::format("[+] {}", diff_node.name);
    case diff_state::added_descendant:
        return diff_node.name;
    case diff_state::removed:
        return std::format("[-] {}", diff_node.name);
    case diff_state::changed:
    case diff_state::type_changed:
        return std::format("[/] {}: {}", diff_node.name, diff_node.detail);
    default:
        return diff_node.name;
    }
}

void print_diff_line(const node& diff_node, std::string_view prefix, bool is_last)
{
    const std::string_view branch{is_last ? "\xE2\x94\x94\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80 "};
    const std::string line{format_node(diff_node)};
    switch (diff_node.state)
    {
    case diff_state::added:
    case diff_state::added_descendant:
        std::println("{}{}{}{}{}", prefix, branch, green, line, reset);
        break;
    case diff_state::removed:
        std::println("{}{}{}{}{}", prefix, branch, red, line, reset);
        break;
    case diff_state::changed:
    case diff_state::type_changed:
        std::println("{}{}{}{}{}", prefix, branch, blue, line, reset);
        break;
    default:
        std::println("{}{}{}", prefix, branch, line);
        break;
    }
}

void print_diff_tree(const node& diff_node, const std::string& prefix, bool is_root, bool is_last)
{
    if (is_root)
    {
        std::println("{}", diff_node.name.empty() ? "/" : diff_node.name);
    }
    else
    {
        print_diff_line(diff_node, prefix, is_last);
    }

    const std::string child_prefix{is_root ? "" : std::format("{}{}", prefix, is_last ? "    " : "\xE2\x94\x82   ")};
    for (std::size_t i{0}; i < diff_node.children.size(); ++i)
    {
        print_diff_tree(diff_node.children[i], child_prefix, false, i + 1 == diff_node.children.size());
    }
}
} // namespace

namespace command
{
void help()
{
    std::println("Usage: nori [-h | --help] <command> [<args>]");
    std::println();
    std::println("  tree <path>               Visualize the data structure in a tree format.");
    std::println("  get <path>                Print the data at the specified path.");
    std::println("                            * String data will be printed wrapped in double quotes (\"\").");
    std::println("  set <path> <value>        Store or update data at the specified path.");
    std::println("                            * Number: Input digits without spaces (e.g., 5)");
    std::println("                            * String: Wrap the value in single quotes (e.g., 'value')");
    std::println("                            * Image: Prefix a PNG path with @ (Windows only, stored as r8g8b8a8)");
    std::println("  del <path>                Delete the data at the specified path.");
    std::println("                            * All sub-path data will be deleted as well.");
    std::println("  exists <path>             Check if data exists at the specified path.");
    std::println("                            * Returns either true or false.");
    std::println("  diff <filepath1> <filepath2>");
    std::println("                            Compare two data files or paths.");
    std::println();
}

void tree(std::string_view path)
{
    const nori::resource::handle handle{nori::resource::get(path)};
    if (!handle)
    {
        std::println("Error: Can not open `{}`", path);
        return;
    }

    [](this auto&& self, nori::resource::handle current, int depth, bool is_last, const std::string& prefix) -> void
    {
        if (!current)
        {
            return;
        }

        if (depth > 0)
        {
            std::print("{}", prefix);
            std::print("{}", is_last ? "\xE2\x94\x94\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80 ");
        }
        std::println("{}", nori::resource::get_name(current));

        const std::vector<nori::resource::handle> children{nori::resource::get_children(current)};
        for (std::size_t i{0}; i < children.size(); ++i)
        {
            std::string child_prefix{prefix};
            if (depth > 0)
            {
                child_prefix += is_last ? "    " : "\xE2\x94\x82   ";
            }
            self(children[i], depth + 1, i + 1 == children.size(), child_prefix);
        }
    }(handle, 0, false, "");
}

void get(std::string_view path)
{
    const nori::resource::handle handle{nori::resource::get(path)};
    if (!handle)
    {
        std::println("Error: Can not open `{}`", path);
        return;
    }
    std::print("{}", value_to_string(handle));
}

void set(std::string_view path, std::string_view arg)
{
    const std::optional<std::string_view> file_path{get_file_path(path)};
    if (!file_path)
    {
        std::println("Error: Path must specify a '{}' file.", nori::resource::extension);
        return;
    }

    const nori::resource::handle root = [file_path]()
    {
        if (nori::resource::handle handle{nori::resource::get(*file_path)})
        {
            return handle;
        }
        return nori::resource::create(*file_path);
    }();
    if (!root)
    {
        std::println("Error: Failed to create or load file");
        std::println("    {}", *file_path);
        return;
    }

    const nori::resource::handle handle = [path]()
    {
        if (nori::resource::handle existing{nori::resource::get(path)})
        {
            return existing;
        }
        return nori::resource::create(path);
    }();
    if (!handle)
    {
        std::println("Error: Failed to create or load property");
        std::println("    {}", path);
        return;
    }

    const std::optional<value> parsed_value{parse_value(arg)};
    if (!parsed_value)
    {
        std::println("Error: Unsupported value `{}`", arg);
        std::println("    Use a number, a single-quoted string, or a Windows PNG path prefixed with @.");
        return;
    }

    std::visit(
        [handle](const auto& parsed)
        {
            nori::resource::set_value(handle, parsed);
        },
        *parsed_value
    );

    const std::filesystem::path filesystem_path{*file_path};
    if (filesystem_path.has_parent_path())
    {
        std::filesystem::create_directories(filesystem_path.parent_path());
    }
    if (const auto result{nori::resource::serialize(filesystem_path, root)}; !result)
    {
        std::println("Error: {}: {}", nori::resource::to_string(result.error()), *file_path);
    }
}

void del(std::string_view path)
{
    const std::optional<std::string_view> file_path{get_file_path(path)};
    if (!file_path)
    {
        std::println("Error: Path must specify a '{}' file.", nori::resource::extension);
        return;
    }

    const nori::resource::handle root{nori::resource::get(*file_path)};
    if (!root)
    {
        std::println("Error: Can not open `{}`", *file_path);
        return;
    }

    if (path.size() == file_path->size())
    {
        std::error_code ec{};
        if (!std::filesystem::remove(std::filesystem::path{*file_path}, ec) && ec)
        {
            std::println("Error: Failed to delete file: {}", *file_path);
            return;
        }
        nori::resource::remove(root);
        return;
    }

    const nori::resource::handle handle{nori::resource::get(path)};
    if (!handle)
    {
        std::println("Error: Can not open `{}`", path);
        return;
    }
    nori::resource::remove(handle);
    if (const auto result{nori::resource::serialize(std::filesystem::path{*file_path}, root)}; !result)
    {
        std::println("Error: {}: {}", nori::resource::to_string(result.error()), *file_path);
    }
}

void exists(std::string_view path)
{
    std::print("{}", nori::resource::get(path) ? "true" : "false");
}

void diff(std::string_view lhs_path, std::string_view rhs_path)
{
    if (!lhs_path.ends_with(nori::resource::extension))
    {
        std::println("Error: Invalid path `{}`", lhs_path);
        return;
    }
    if (!rhs_path.ends_with(nori::resource::extension))
    {
        std::println("Error: Invalid path `{}`", rhs_path);
        return;
    }

    const nori::resource::handle lhs{nori::resource::get(lhs_path)};
    if (!lhs)
    {
        std::println("Error: Can not open `{}`", lhs_path);
        return;
    }
    const nori::resource::handle rhs{nori::resource::get(rhs_path)};
    if (!rhs)
    {
        std::println("Error: Can not open `{}`", rhs_path);
        return;
    }

    std::optional<node> diff_tree{create_diff_tree(lhs, rhs)};
    if (!diff_tree)
    {
        std::println("No differences.");
        return;
    }
    diff_tree->name = lhs_path;
    print_diff_tree(*diff_tree, "", true, true);
}
} // namespace command
