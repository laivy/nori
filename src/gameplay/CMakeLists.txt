add_library(nori_gameplay STATIC)
add_library(nori::gameplay ALIAS nori_gameplay)

target_sources(nori_gameplay
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/component/component.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/component/physics_body_component.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/component/scene_component.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/detail/object_registry.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/game_instance/game_instance.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/object/object.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/object/object_handle.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/world/world.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/gameplay/world/world_events.hpp
    PRIVATE
        component/component.cpp
        component/physics_body_component.cpp
        component/scene_component.cpp
        detail/object_registry.cpp
        game_instance/game_instance.cpp
        object/object.cpp
        world/world.cpp
        world/world_events.cpp
)

nori_configure_target(nori_gameplay)
target_link_libraries(nori_gameplay PUBLIC nori::core nori::physics)

set_target_properties(nori_gameplay PROPERTIES EXPORT_NAME gameplay)
