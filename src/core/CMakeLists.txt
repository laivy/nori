add_library(nori_core STATIC)
add_library(nori::core ALIAS nori_core)
set_property(TARGET nori_core PROPERTY EXPORT_NAME core)
target_compile_features(nori_core PUBLIC cxx_std_23)
nori_configure_target(nori_core)

set(nori_core_headers
    ${PROJECT_SOURCE_DIR}/include/nori/core.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/application.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/console_application.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/event.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/handle.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/input.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/input_code.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/singleton.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/slot_map.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/thread_pool.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/timer.hpp
    ${PROJECT_SOURCE_DIR}/include/nori/core/types.hpp
)

set(nori_core_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/console_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timer.cpp
)

if (WIN32)
    list(APPEND nori_core_headers
        ${PROJECT_SOURCE_DIR}/include/nori/core/windows_application.hpp
    )
    list(APPEND nori_core_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/windows_application.cpp
    )

    target_compile_definitions(nori_core
        PUBLIC
            _UNICODE
            _WIN32_WINNT=0x0A00 # Windows 10
            NOMINMAX
            UNICODE
            WIN32_LEAN_AND_MEAN
    )
endif ()

target_sources(nori_core
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${nori_core_headers}
    PRIVATE
        ${nori_core_sources}
)
target_include_directories(nori_core PRIVATE ${PROJECT_SOURCE_DIR}/include/nori/core)

install(TARGETS nori_core EXPORT noriTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
