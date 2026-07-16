add_executable(nori)
nori_configure_target(nori)

target_sources(nori
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/command.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/command.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nori_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nori_application.hpp
)

target_link_libraries(nori
    PRIVATE
        nori::core
        nori::resource
)

if (WIN32)
    target_compile_definitions(nori PRIVATE WIN32_LEAN_AND_MEAN)
    target_link_libraries(nori PRIVATE windowscodecs)
endif ()

target_include_directories(nori
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

install(TARGETS nori
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
