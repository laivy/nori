include(CMakePackageConfigHelpers)

if (TARGET nori_core OR TARGET nori_resource OR TARGET nori_graphics OR TARGET nori_network)
    set(nori_cmake_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/nori)

    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/nori-config.cmake.in
        ${PROJECT_BINARY_DIR}/nori-config.cmake
        INSTALL_DESTINATION ${nori_cmake_install_dir}
    )
    write_basic_package_version_file(
        ${PROJECT_BINARY_DIR}/nori-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT nori_targets
        FILE nori-targets.cmake
        NAMESPACE nori::
        DESTINATION ${nori_cmake_install_dir}
    )
    install(FILES
        ${PROJECT_BINARY_DIR}/nori-config.cmake
        ${PROJECT_BINARY_DIR}/nori-config-version.cmake
        DESTINATION ${nori_cmake_install_dir}
    )
endif ()

install(FILES ${PROJECT_SOURCE_DIR}/README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nori
)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
    DESTINATION licenses
    RENAME nori-LICENSE.txt
)

if (PROJECT_IS_TOP_LEVEL)
    set(nori_package_arch ${CMAKE_GENERATOR_PLATFORM})
    if (NOT nori_package_arch)
        set(nori_package_arch ${CMAKE_SYSTEM_PROCESSOR})
    endif ()
    if (nori_package_arch MATCHES "^(AMD64|x86_64)$")
        set(nori_package_arch x64)
    endif ()

    string(TOLOWER "${CMAKE_SYSTEM_NAME}" nori_package_system)

    set(CPACK_PACKAGE_NAME nori)
    set(CPACK_PACKAGE_VENDOR nori)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nori C++ game development libraries and tools")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist")

    if (MSVC)
        set(CPACK_PACKAGE_FILE_NAME
            "nori-${PROJECT_VERSION}-${nori_package_system}-msvc${MSVC_TOOLSET_VERSION}-${nori_package_arch}"
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" nori_compiler_major "${CMAKE_CXX_COMPILER_VERSION}")
        set(CPACK_PACKAGE_FILE_NAME
            "nori-${PROJECT_VERSION}-${nori_package_system}-gcc${nori_compiler_major}-${nori_package_arch}"
        )
    else ()
        set(CPACK_PACKAGE_FILE_NAME
            "nori-${PROJECT_VERSION}-${nori_package_system}-${nori_package_arch}"
        )
    endif ()

    set(CPACK_GENERATOR ZIP)
    include(CPack)
endif ()
