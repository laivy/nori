add_library(nori_resource STATIC)
add_library(nori::resource ALIAS nori_resource)

target_sources(nori_resource
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/resource.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/binary.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/error_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/image.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/operations.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/path.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/value.hpp
    PRIVATE
        codec/data_pool.cpp
        codec/data_pool.hpp
        codec/format.hpp
        codec/name_table.cpp
        codec/name_table.hpp
        codec/property_table.cpp
        codec/property_table.hpp
        codec/writer.cpp
        codec/writer.hpp
        context.cpp
        context.hpp
        file_registry.cpp
        file_registry.hpp
        hierarchy.cpp
        hierarchy.hpp
        loader.cpp
        loader.hpp
        operations.cpp
        path.cpp
        property.cpp
        property.hpp
)

nori_configure_target(nori_resource)
target_link_libraries(nori_resource PUBLIC nori::core)
set_target_properties(nori_resource PROPERTIES EXPORT_NAME resource)
