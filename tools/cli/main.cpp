#include <cstdlib>
#include <print>

#include <nori/resource.hpp>

#include "protocol.hpp"

int main()
{
    if (!nori::resource::initialize())
    {
        std::println(stderr, "failed to initialize resources");
        return EXIT_FAILURE;
    }

    const int exit_code{nori::cli::run()};
    nori::resource::shutdown();
    return exit_code;
}
