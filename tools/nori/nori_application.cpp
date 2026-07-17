#include <clocale>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <nori/resource.hpp>
#include "command.hpp"
#include "nori_application.hpp"

#ifdef _WIN32
#include <utility>
#include <Windows.h>
#include <shellapi.h>
#endif

namespace
{
using exit_code = nori::core::application::exit_code;

std::vector<std::string> collect_args(int argc, char** argv)
{
#ifdef _WIN32
    std::ignore = argc;
    std::ignore = argv;

    int wide_argc{};
    wchar_t** wide_argv{::CommandLineToArgvW(::GetCommandLineW(), &wide_argc)};
    if (!wide_argv)
    {
        return {};
    }

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(wide_argc));
    for (int i{0}; i < wide_argc; ++i)
    {
        const std::wstring_view wide_arg{wide_argv[i]};
        if (wide_arg.empty())
        {
            args.emplace_back();
            continue;
        }

        const int size{::WideCharToMultiByte(
            CP_UTF8, 0, wide_arg.data(), static_cast<int>(wide_arg.size()), nullptr, 0, nullptr, nullptr
        )};
        if (size == 0)
        {
            args.emplace_back();
            continue;
        }

        std::string arg(static_cast<std::size_t>(size), '\0');
        if (::WideCharToMultiByte(
                CP_UTF8, 0, wide_arg.data(), static_cast<int>(wide_arg.size()), arg.data(), size, nullptr, nullptr
            ) == 0)
        {
            args.emplace_back();
            continue;
        }
        args.push_back(std::move(arg));
    }

    ::LocalFree(static_cast<HLOCAL>(wide_argv));
    ::SetConsoleOutputCP(CP_UTF8);
    return args;
#else
    const std::string_view locale{std::setlocale(LC_ALL, "")};
    if (!locale.contains("UTF-8"))
    {
        std::println(stderr, "Warning: locale is not UTF-8, non-ASCII characters may not work correctly.");
    }

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i{0}; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }
    return args;
#endif
}

exit_code run_command(std::span<const std::string> args)
{
    if (args.size() == 1)
    {
        command::help();
        return exit_code::success;
    }

    const std::string_view command{args[1]};
    if (command == "-h" || command == "--help" || command == "help")
    {
        command::help();
    }
    else if (command == "tree")
    {
        if (args.size() < 3)
        {
            std::println("Usage: nori tree <path>");
            return exit_code::error;
        }
        command::tree(args[2]);
    }
    else if (command == "get")
    {
        if (args.size() < 3)
        {
            std::println("Usage: nori get <path>");
            return exit_code::error;
        }
        command::get(args[2]);
    }
    else if (command == "set")
    {
        if (args.size() < 4)
        {
            std::println("Usage: nori set <path> <value>");
            return exit_code::error;
        }
        command::set(args[2], args[3]);
    }
    else if (command == "del")
    {
        if (args.size() < 3)
        {
            std::println("Usage: nori del <path>");
            return exit_code::error;
        }
        command::del(args[2]);
    }
    else if (command == "exists")
    {
        if (args.size() < 3)
        {
            std::println("Usage: nori exists <path>");
            return exit_code::error;
        }
        command::exists(args[2]);
    }
    else if (command == "diff")
    {
        if (args.size() < 4)
        {
            std::println("Usage: nori diff <filepath1> <filepath2>");
            return exit_code::error;
        }
        command::diff(args[2], args[3]);
    }
    else
    {
        std::println(stderr, "Error: Unknown command `{}`", command);
        command::help();
        return exit_code::error;
    }
    return exit_code::success;
}
} // namespace

nori_application::nori_application(const specification& spec) :
    nori::core::console_application{spec},
    resource_{nori::resource::runtime::specification{}},
    argc_{spec.argc},
    argv_{spec.argv},
    exit_code_{nori::core::application::exit_code::success}
{
}

nori_application::~nori_application()
{
}

void nori_application::run()
{
    const std::vector<std::string> args{collect_args(argc_, argv_)};
    const nori::core::application::exit_code result{run_command(args)};
    quit(result);
}

void nori_application::quit(nori::core::application::exit_code exit_code)
{
    exit_code_ = exit_code;
}

int nori_application::exit_code() const
{
    return static_cast<int>(exit_code_);
}
