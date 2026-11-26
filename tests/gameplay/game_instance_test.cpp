#include <gtest/gtest.h>

#include <nori/core/rtti.hpp>
#include <nori/core/transform.hpp>
#include <nori/gameplay/game_instance/game_instance.hpp>
#include <nori/gameplay/object/object.hpp>
#include <nori/gameplay/world/world.hpp>

namespace
{
TEST(game_instance_test, world_initializer)
{
    struct initializer
    {
        initializer(nori::gameplay::world& active, float x)
        {
            auto pose{nori::core::transform::identity};
            pose.position.x = x;
            active.create_object<nori::gameplay::object>(pose);
        }
    };

    nori::gameplay::game_instance game;
    auto& active{game.create_world<initializer>(3.0f)};
    ASSERT_EQ(active.objects().size(), 1u);
    EXPECT_FLOAT_EQ(active.find_object(active.objects().front())->root_component().world_transform().position.x, 3.0f);
    game.clear_world();
    EXPECT_EQ(game.active_world(), nullptr);
}
} // namespace
