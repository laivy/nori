include_guard(GLOBAL)

include(FetchContent)

function(nori_register_package_license source_file installed_name)
    if (NOT EXISTS "${source_file}")
        message(FATAL_ERROR "Required package license file was not found: ${source_file}")
    endif ()

    install(FILES "${source_file}"
        DESTINATION licenses
        RENAME "${installed_name}"
    )
endfunction()

FetchContent_Declare(
    asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
    URL_HASH SHA256=9F2648FA483E58A6BF848D970EE0EA650CA19ED7769DFA520ED4F7B8D27AF1DB
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    DirectX-Headers
    URL https://github.com/microsoft/DirectX-Headers/archive/9e393d6d8a3b30dcc6f2806ef604ec16a27b0d7e.tar.gz
    URL_HASH SHA256=5b9ab816a4eebea0261e59b4f1e2885f901d7df629c41c7d6b5f8770c52283b1
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    DirectXShaderCompiler
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602.24/dxc_2026_05_27.zip
    URL_HASH SHA256=cf658aacf070d3045e31b8f1f8a696c2945f37c1095019481ef7c513368db3b4
    DOWNLOAD_NO_PROGRESS TRUE
)

FetchContent_Declare(
    DirectXTex
    URL https://github.com/microsoft/DirectXTex/archive/refs/tags/may2026.tar.gz
    URL_HASH SHA256=204ae68a66fb15d87867d836efbac9ae5aa799ca2a1867024d96a8168fa3651f
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/b61e56346a92cfcaf1f43a545ca37b0b32239654.tar.gz
    URL_HASH SHA256=100c1e0a0625c538d79e014dc8e73309c5b25c7ed8183fb91ae621a3681414a4
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    ufbx
    URL https://github.com/ufbx/ufbx/archive/fcc5d6ba444cfd3eb80677dba5e37e493941abe5.tar.gz
    URL_HASH SHA256=158ab0d66c789afb0a85a06d2ac40366c399d18c82cbda3acd43afb1838471a5
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

if (nori_build_core OR nori_build_io)
    find_package(Threads REQUIRED)
endif ()

if (nori_build_io)
    FetchContent_MakeAvailable(asio)

    if (NORI_INSTALL)
        file(READ "${asio_SOURCE_DIR}/COPYING" nori_asio_copyright)
        file(READ "${asio_SOURCE_DIR}/LICENSE_1_0.txt" nori_asio_license_text)
        set(nori_asio_license "${CMAKE_CURRENT_BINARY_DIR}/asio-LICENSE.txt")
        file(WRITE "${nori_asio_license}"
            "===== Asio Copyright Notice =====\n"
            "${nori_asio_copyright}\n\n"
            "===== Boost Software License 1.0 =====\n"
            "${nori_asio_license_text}"
        )

        nori_register_package_license(
            "${nori_asio_license}"
            asio-LICENSE.txt
        )
    endif ()

    add_library(nori_asio INTERFACE)
    add_library(nori::asio ALIAS nori_asio)
    target_include_directories(nori_asio SYSTEM INTERFACE $<BUILD_INTERFACE:${asio_SOURCE_DIR}/include>)
    target_compile_definitions(nori_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
endif ()

if ((nori_build_asset OR nori_build_graphics) AND WIN32)
    FetchContent_MakeAvailable(DirectXShaderCompiler)
    if (nori_build_graphics)
        add_executable(nori_dxc IMPORTED)
        set_target_properties(nori_dxc PROPERTIES IMPORTED_LOCATION "${directxshadercompiler_SOURCE_DIR}/bin/x64/dxc.exe")
    endif ()
endif ()

if (nori_build_asset)
    if (WIN32)
        if (NORI_INSTALL)
            file(READ "${directxshadercompiler_SOURCE_DIR}/LICENSE-MS.txt" nori_dxcompiler_license_ms)
            file(READ "${directxshadercompiler_SOURCE_DIR}/LICENSE-LLVM.txt" nori_dxcompiler_license_llvm)
            file(READ "${directxshadercompiler_SOURCE_DIR}/LICENSE-MIT.txt" nori_dxcompiler_license_mit)
            set(nori_dxcompiler_license "${CMAKE_CURRENT_BINARY_DIR}/dxcompiler-LICENSE.txt")
            file(WRITE "${nori_dxcompiler_license}"
                "===== Microsoft Software License Terms (LICENSE-MS.txt) =====\n"
                "${nori_dxcompiler_license_ms}\n\n"
                "===== LLVM Release License (LICENSE-LLVM.txt) =====\n"
                "${nori_dxcompiler_license_llvm}\n\n"
                "===== MIT License (LICENSE-MIT.txt) =====\n"
                "${nori_dxcompiler_license_mit}"
            )

            nori_register_package_license(
                "${nori_dxcompiler_license}"
                dxcompiler-LICENSE.txt
            )
        endif ()

        add_library(nori_dxcompiler SHARED IMPORTED)
        add_library(nori::dxcompiler ALIAS nori_dxcompiler)
        set_target_properties(nori_dxcompiler PROPERTIES
            IMPORTED_IMPLIB "${directxshadercompiler_SOURCE_DIR}/lib/x64/dxcompiler.lib"
            IMPORTED_LOCATION "${directxshadercompiler_SOURCE_DIR}/bin/x64/dxcompiler.dll"
            INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${directxshadercompiler_SOURCE_DIR}/inc>"
        )

        block(SCOPE_FOR POLICIES VARIABLES)
            set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
            set(BUILD_SHARED_LIBS OFF)
            set(BUILD_TOOLS OFF)
            set(BUILD_SAMPLE OFF)
            set(BUILD_DX11 OFF)
            set(BUILD_DX12 OFF)
            set(ENABLE_OPENEXR_SUPPORT OFF)
            set(ENABLE_LIBJPEG_SUPPORT OFF)
            set(ENABLE_LIBPNG_SUPPORT OFF)
            set(CMAKE_SKIP_INSTALL_RULES ON)
            FetchContent_MakeAvailable(DirectXTex)
        endblock()
        if (NORI_INSTALL)
            FetchContent_GetProperties(DirectXTex)
            if (DEFINED directxtex_SOURCE_DIR)
                nori_register_package_license(
                    "${directxtex_SOURCE_DIR}/LICENSE"
                    directxtex-LICENSE.txt
                )
            endif ()
        endif ()
        add_library(nori::directxtex ALIAS DirectXTex)
        set_target_properties(DirectXTex PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif ()

    FetchContent_MakeAvailable(ufbx)
    if (NORI_INSTALL)
        nori_register_package_license(
            "${ufbx_SOURCE_DIR}/LICENSE"
            ufbx-LICENSE.txt
        )
    endif ()
endif ()

function(nori_copy_asset_dxcompiler target)
    if (MSVC)
        target_link_options(${target} PRIVATE "/DELAYLOAD:dxcompiler.dll")
        target_link_libraries(${target} PRIVATE delayimp)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:nori_dxcompiler>
                $<TARGET_FILE_DIR:${target}>
            VERBATIM
        )
    endif ()
endfunction()

if (nori_build_graphics)
    block(SCOPE_FOR POLICIES VARIABLES)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(DXHEADERS_BUILD_GOOGLE_TEST OFF)
        set(DXHEADERS_BUILD_TEST OFF)
        set(DXHEADERS_INSTALL OFF)
        FetchContent_MakeAvailable(DirectX-Headers)
    endblock()
    if (NORI_INSTALL)
        FetchContent_GetProperties(DirectX-Headers)
        if (EXISTS "${directx-headers_SOURCE_DIR}/LICENSE")
            nori_register_package_license(
                "${directx-headers_SOURCE_DIR}/LICENSE"
                directx-headers-LICENSE.txt
            )
        endif ()
    endif ()

    if (nori_enable_imgui)
        FetchContent_MakeAvailable(imgui)
        if (NORI_INSTALL)
            nori_register_package_license(
                "${imgui_SOURCE_DIR}/LICENSE.txt"
                imgui-LICENSE.txt
            )
        endif ()
    endif ()
endif ()

if (NORI_BUILD_CLI)
    FetchContent_MakeAvailable(nlohmann_json)
    if (NORI_INSTALL)
        nori_register_package_license(
            "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT"
            nlohmann-json-LICENSE.txt
        )
    endif ()
endif ()

if (NORI_BUILD_TESTS)
    block(SCOPE_FOR POLICIES VARIABLES)
        set(gtest_force_shared_crt ON)
        set(INSTALL_GTEST OFF)
        FetchContent_MakeAvailable(googletest)
    endblock()
endif ()
