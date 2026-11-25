add_library(nori_core STATIC)
add_library(nori::core ALIAS nori_core)

target_sources(nori_core
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/core.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/abort.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/application.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/console_application.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/delegate.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/encoding.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/endian.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/hash.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/input.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/input_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/matrix.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/multicast_delegate.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/rtti.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/slot_map.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/timer.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/core/vector.hpp
    PRIVATE
        abort.cpp
        application.cpp
        console_application.cpp
        encoding.cpp
        input.cpp
        input_internal.hpp
        timer.cpp
        vector.cpp
)

if (WIN32)
    target_sources(nori_core
        PUBLIC
            FILE_SET HEADERS
            FILES
                ${PROJECT_SOURCE_DIR}/include/nori/core/windows_application.hpp
        PRIVATE
            windows_application.cpp
    )
endif ()

nori_configure_target(nori_core)
target_link_libraries(nori_core PUBLIC Threads::Threads)
set_target_properties(nori_core PROPERTIES EXPORT_NAME core)
