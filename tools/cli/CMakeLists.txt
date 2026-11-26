add_library(nori_cli_core STATIC)

target_sources(nori_cli_core
    PRIVATE
        command/command.cpp
        command/command.hpp
        command/common.cpp
        command/common.hpp
        command/del.cpp
        command/diff.cpp
        command/exists.cpp
        command/get.cpp
        command/import.cpp
        command/rename.cpp
        command/reorder.cpp
        command/set.cpp
        command/transfer.cpp
        command/tree.cpp
        protocol.cpp
        protocol.hpp
)

target_include_directories(nori_cli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
nori_configure_target(nori_cli_core)
target_link_libraries(nori_cli_core
    PUBLIC
        nori::resource
        nori::asset
        nlohmann_json::nlohmann_json
    PRIVATE
        nori::core
)

add_executable(nori_cli)

target_sources(nori_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
nori_configure_target(nori_cli)
target_link_libraries(nori_cli PRIVATE nori_cli_core)
nori_copy_asset_dxcompiler(nori_cli)
set_target_properties(nori_cli PROPERTIES OUTPUT_NAME nori)

if (NORI_INSTALL)
    install(TARGETS nori_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if (WIN32)
        install(FILES $<TARGET_FILE:nori_dxcompiler>
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif ()
endif ()
