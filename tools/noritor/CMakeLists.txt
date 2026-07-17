if (NOT WIN32)
    message(FATAL_ERROR "NORI_BUILD_NORITOR is only supported on Windows.")
endif ()

add_executable(noritor WIN32)
nori_configure_target(noritor)

set(source_files
    ${CMAKE_CURRENT_SOURCE_DIR}/context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/explorer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/explorer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hierarchy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hierarchy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inspector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inspector.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/noritor_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/noritor_application.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.hpp
)

target_sources(noritor PRIVATE ${source_files})
target_precompile_headers(noritor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp)
target_link_libraries(noritor PRIVATE nori::imgui Microsoft::DirectX-Headers windowscodecs)
target_include_directories(noritor PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>)

nori_compile_hlsl(
    TARGET noritor
    SOURCE shaders/fullscreen.hlsl
    ENTRY VSMain
    PROFILE vs_6_0
    OUTPUT generated/$<CONFIG>/shaders/fullscreen_vs.hpp
    VARIABLE fullscreen_vs
)
nori_compile_hlsl(
    TARGET noritor
    SOURCE shaders/fullscreen.hlsl
    ENTRY PSMain
    PROFILE ps_6_0
    OUTPUT generated/$<CONFIG>/shaders/fullscreen_ps.hpp
    VARIABLE fullscreen_ps
)
install(TARGETS noritor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
