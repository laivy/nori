add_library(nori_asset STATIC)
add_library(nori::asset ALIAS nori_asset)

set_source_files_properties("${ufbx_SOURCE_DIR}/ufbx.c" PROPERTIES LANGUAGE CXX)

target_sources(nori_asset
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/asset.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/asset/cook.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/asset/error_code.hpp
    PRIVATE
        ${ufbx_SOURCE_DIR}/ufbx.c
        binary_cook.cpp
        file_io.cpp
        file_io.hpp
        image_cook.cpp
        image_decode.cpp
        image_decode.hpp
        model_cook.cpp
        shader_cook.cpp
)
target_include_directories(nori_asset SYSTEM PRIVATE "${ufbx_SOURCE_DIR}")
nori_configure_target(nori_asset)
target_link_libraries(nori_asset
    PUBLIC
        nori::core
        nori::resource
)

if (WIN32)
    target_link_libraries(nori_asset PRIVATE
        "$<BUILD_LOCAL_INTERFACE:nori::directxtex>$<INSTALL_INTERFACE:nori::asset_directxtex>"
        "$<BUILD_LOCAL_INTERFACE:nori::dxcompiler>$<INSTALL_INTERFACE:nori::asset_dxcompiler>"
        delayimp
        ole32
    )
endif ()

set_target_properties(nori_asset PROPERTIES EXPORT_NAME asset)

if (NORI_INSTALL AND WIN32)
    install(FILES $<TARGET_FILE:DirectXTex>
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    )
    install(FILES
        $<TARGET_LINKER_FILE:nori_dxcompiler>
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(FILES
        $<TARGET_FILE:nori_dxcompiler>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif ()
