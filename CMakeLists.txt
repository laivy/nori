cmake_minimum_required(VERSION 4.0)

project(nori VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(${PROJECT_SOURCE_DIR}/cmake/compile_options.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/dependencies.cmake)

# 모듈
option(NORI_BUILD_CORE "Build nori core support." OFF)
option(NORI_BUILD_RESOURCE "Build nori resource support." OFF)
option(NORI_BUILD_GRAPHICS "Build nori graphics support." OFF)
option(NORI_BUILD_NETWORK "Build nori network support." OFF)

# 모듈 옵션
option(NORI_ENABLE_IMGUI "Enable Dear ImGui integration for nori graphics." OFF)
option(NORI_INSTALL "Install nori libraries and package metadata." ${PROJECT_IS_TOP_LEVEL})

# 툴
option(NORI_BUILD_CLI "Build nori command line tool." OFF)
option(NORI_BUILD_NORITOR "Build nori visual resource editor." OFF)

# 테스트
if (PROJECT_IS_TOP_LEVEL)
    option(BUILD_TESTING "Build the testing tree." OFF)
    include(CTest)
endif ()

# 모듈 의존성 확인
if (NORI_BUILD_RESOURCE AND NOT NORI_BUILD_CORE)
    message(FATAL_ERROR "NORI_BUILD_RESOURCE requires NORI_BUILD_CORE.")
endif ()

if (NORI_BUILD_GRAPHICS AND NOT NORI_BUILD_CORE)
    message(FATAL_ERROR "NORI_BUILD_GRAPHICS requires NORI_BUILD_CORE.")
endif ()

if (NORI_BUILD_GRAPHICS AND NOT NORI_BUILD_RESOURCE)
    message(FATAL_ERROR "NORI_BUILD_GRAPHICS requires NORI_BUILD_RESOURCE.")
endif ()

if (NORI_BUILD_CLI AND (NOT NORI_BUILD_CORE OR NOT NORI_BUILD_RESOURCE))
    message(FATAL_ERROR "NORI_BUILD_CLI requires NORI_BUILD_CORE and NORI_BUILD_RESOURCE.")
endif ()

if (NORI_BUILD_NORITOR AND NOT NORI_BUILD_GRAPHICS)
    message(FATAL_ERROR "NORI_BUILD_NORITOR requires NORI_BUILD_GRAPHICS.")
endif ()

if (NORI_BUILD_NORITOR AND NOT NORI_ENABLE_IMGUI)
    message(FATAL_ERROR "NORI_BUILD_NORITOR requires NORI_ENABLE_IMGUI.")
endif ()

if (NORI_BUILD_CORE)
    add_subdirectory(src/core)
endif ()

if (NORI_BUILD_RESOURCE)
    add_subdirectory(src/resource)
endif ()

if (NORI_BUILD_GRAPHICS)
    add_subdirectory(src/graphics)
endif ()

if (NORI_BUILD_NETWORK)
    add_subdirectory(src/network)
endif ()

if (NORI_BUILD_CLI)
    add_subdirectory(tools/nori)
endif ()

if (NORI_BUILD_NORITOR)
    add_subdirectory(tools/noritor)
endif ()

if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif ()

if (NORI_INSTALL)
    include(${PROJECT_SOURCE_DIR}/cmake/package.cmake)
endif ()
