add_library(nori_physics STATIC)
add_library(nori::physics ALIAS nori_physics)

target_sources(nori_physics
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/body.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/body_handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/collider.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/collider_handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/contact.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/query.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/physics2d/scene.hpp
    PRIVATE
        physics2d/scene.cpp
)

nori_configure_target(nori_physics)
target_link_libraries(nori_physics
    PUBLIC
        nori::core
    PRIVATE
        "$<BUILD_LOCAL_INTERFACE:box2d::box2d>$<INSTALL_INTERFACE:nori::physics_box2d>"
)

set_target_properties(nori_physics PROPERTIES EXPORT_NAME physics)

if (NORI_INSTALL)
    install(FILES $<TARGET_FILE:box2d>
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
        RENAME "${CMAKE_STATIC_LIBRARY_PREFIX}nori-box2d${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
endif ()
