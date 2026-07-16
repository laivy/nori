#include "pch.hpp"
#include "noritor_application.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    nori::core::windows_application::specification spec{
        .title = L"Noritor",
        .width = 1600,
        .height = 900
    };
    noritor_application app{std::move(spec)};
    app.run();
    return 0;
}
