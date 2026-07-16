add_library(nori_resource STATIC)
add_library(nori::resource ALIAS nori_resource)
set_property(TARGET nori_resource PROPERTY EXPORT_NAME resource)
target_compile_features(nori_resource PUBLIC cxx_std_23)
nori_configure_target(nori_resource)

target_sources(nori_resource
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/resource.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/accessor.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/error_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/image.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/iterator.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/path.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/runtime.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/resource/type.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/accessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/deserialize.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/deserialize.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/format.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/iterator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/serialize.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/serialize.hpp
)
target_include_directories(nori_resource PRIVATE ${PROJECT_SOURCE_DIR}/include/nori/resource)
target_link_libraries(nori_resource PUBLIC nori::core)

install(TARGETS nori_resource EXPORT noriTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
