cmake_minimum_required(VERSION 4.2)

project(nori VERSION 0.1.0 LANGUAGES C CXX)

if (MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,Release>:Embedded>")
endif ()

include(GNUInstallDirs)
include(${PROJECT_SOURCE_DIR}/cmake/compile_options.cmake)

option(NORI_BUILD_CORE "Build nori core support." ON)
option(NORI_BUILD_IO "Build nori coroutine and I/O support." ON)
option(NORI_BUILD_RESOURCE "Build nori resource support." ON)
option(NORI_BUILD_ASSET "Build nori asset support." ON)
option(NORI_BUILD_GRAPHICS "Build nori graphics support." ${WIN32})

option(NORI_ENABLE_IMGUI "Enable Dear ImGui integration for nori graphics." OFF)

option(NORI_BUILD_CLI "Build nori command line tool." OFF)
option(NORI_BUILD_NORITOR "Build nori visual resource editor." OFF)

option(NORI_BUILD_TESTS "Build nori tests." ${PROJECT_IS_TOP_LEVEL})
option(NORI_INSTALL "Install nori libraries and package metadata." OFF)

set(nori_build_core ${NORI_BUILD_CORE})
set(nori_build_io ${NORI_BUILD_IO})
set(nori_build_resource ${NORI_BUILD_RESOURCE})
set(nori_build_asset ${NORI_BUILD_ASSET})
set(nori_build_graphics ${NORI_BUILD_GRAPHICS})
set(nori_enable_imgui ${NORI_ENABLE_IMGUI})

if (NORI_BUILD_CLI)
    set(nori_build_asset ON)
endif ()

if (NORI_BUILD_NORITOR)
    set(nori_build_io ON)
    set(nori_build_asset ON)
    set(nori_enable_imgui ON)
endif ()

if (nori_enable_imgui)
    set(nori_build_graphics ON)
endif ()

if (nori_build_asset OR nori_build_graphics)
    set(nori_build_resource ON)
endif ()

if (nori_build_resource OR nori_build_io)
    set(nori_build_core ON)
endif ()

if (NOT WIN32 AND (nori_build_graphics OR NORI_BUILD_NORITOR))
    message(FATAL_ERROR "This configuration is only supported on Windows.")
endif ()

set(nori_enabled_modules)
foreach (nori_module IN ITEMS core io resource asset graphics)
    if (nori_build_${nori_module})
        list(APPEND nori_enabled_modules ${nori_module})
    endif ()
endforeach ()
list(JOIN nori_enabled_modules ", " nori_enabled_modules_text)
message(STATUS "nori modules: ${nori_enabled_modules_text}")

if (NORI_INSTALL AND NOT nori_enabled_modules)
    message(FATAL_ERROR "NORI_INSTALL requires at least one enabled module.")
endif ()

include(${PROJECT_SOURCE_DIR}/cmake/dependencies.cmake)

foreach (nori_module IN LISTS nori_enabled_modules)
    add_subdirectory(src/${nori_module})
endforeach ()

# 활성화된 모듈을 한 번에 사용하는 소비자용 타겟
if (nori_enabled_modules)
    add_library(nori INTERFACE)
    add_library(nori::nori ALIAS nori)

    foreach (nori_module IN LISTS nori_enabled_modules)
        target_link_libraries(nori INTERFACE nori::${nori_module})
    endforeach ()
endif ()

if (NORI_BUILD_CLI)
    add_subdirectory(tools/cli)
endif ()

if (NORI_BUILD_NORITOR)
    add_subdirectory(tools/noritor)
endif ()

if (NORI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (NORI_INSTALL)
    include(${PROJECT_SOURCE_DIR}/cmake/package.cmake)
endif ()
