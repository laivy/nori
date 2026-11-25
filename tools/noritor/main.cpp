#include "pch.hpp"

#include "application.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const nori::core::windows_application::specification spec{
        .title = "Noritor",
        .width = 1600,
        .height = 900,
        .resizable = true,
        .fixed_update_rate = 60,
        .max_fixed_updates_per_frame = 8
    };
    nori::editor::application app{spec};
    return app.run();
}
