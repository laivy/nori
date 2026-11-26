include(CMakePackageConfigHelpers)

list(TRANSFORM nori_enabled_modules PREPEND nori_ OUTPUT_VARIABLE nori_install_targets)
list(APPEND nori_install_targets nori)
if (nori_enable_imgui)
    list(APPEND nori_install_targets nori_imgui nori_graphics_imgui)
endif ()

install(TARGETS ${nori_install_targets}
    EXPORT nori_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET imgui_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgui
)

set(nori_cmake_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/nori")

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/nori-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/nori-config.cmake"
    INSTALL_DESTINATION "${nori_cmake_install_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/nori-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT nori_targets
    FILE nori-targets.cmake
    NAMESPACE nori::
    DESTINATION "${nori_cmake_install_dir}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/nori-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/nori-config-version.cmake"
    DESTINATION "${nori_cmake_install_dir}"
)

install(FILES "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

if (PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME nori)
    set(CPACK_PACKAGE_VENDOR nori)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nori C++ game development libraries and tools")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    if (WIN32)
        set(CPACK_GENERATOR ZIP)
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    else ()
        set(CPACK_GENERATOR TGZ)
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    endif ()

    string(TOLOWER "${CMAKE_SYSTEM_NAME}" nori_package_system)
    set(nori_package_arch ${CMAKE_GENERATOR_PLATFORM})
    if (NOT nori_package_arch)
        set(nori_package_arch ${CMAKE_SYSTEM_PROCESSOR})
    endif ()
    if (nori_package_arch MATCHES "^(AMD64|x86_64)$")
        set(nori_package_arch x64)
    endif ()

    set(nori_toolset_version_json null)
    if (MSVC)
        set(nori_package_compiler msvc)
        string(REGEX REPLACE "^([0-9]+)([0-9])$" "\\1.\\2" nori_package_compiler_version "${MSVC_TOOLSET_VERSION}")
        if (CMAKE_CXX_COMPILER MATCHES "/MSVC/([0-9.]+)/")
            set(nori_toolset_version_json "\"${CMAKE_MATCH_1}\"")
        elseif (CMAKE_VS_PLATFORM_TOOLSET_VERSION)
            set(nori_toolset_version_json "\"${CMAKE_VS_PLATFORM_TOOLSET_VERSION}\"")
        endif ()
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(nori_package_compiler gcc)
        string(REGEX MATCH "^[0-9]+" nori_package_compiler_version "${CMAKE_CXX_COMPILER_VERSION}")
    else ()
        string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" nori_package_compiler)
        string(REGEX MATCH "^[0-9]+" nori_package_compiler_version "${CMAKE_CXX_COMPILER_VERSION}")
    endif ()

    set(CPACK_PACKAGE_FILE_NAME "nori-${PROJECT_VERSION}-${nori_package_system}-${nori_package_arch}-${nori_package_compiler}-${nori_package_compiler_version}")

    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/package_build_info.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/package_build_info.cmake"
        @ONLY
    )
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_BINARY_DIR}/package_build_info.cmake")

    include(CPack)
endif ()
