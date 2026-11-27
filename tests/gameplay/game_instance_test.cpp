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
    EXPECT_EQ(game.world(), nullptr);
}
class update_probe final : public nori::core::rtti<update_probe, nori::gameplay::object>
{
public:
    update_probe(nori::gameplay::game_instance& game, bool& called) :
        game_{game},
        called_{called}
    {
    }

protected:
    void on_update(float) override
    {
        game_.request_clear_world();
        EXPECT_EQ(game_.world(), world());
        called_ = true;
    }

private:
    nori::gameplay::game_instance& game_;
    bool& called_;
};

TEST(game_instance_test, update_can_request_clearing_the_world)
{
    bool called{};
    nori::gameplay::game_instance game;
    auto& active{game.create_world()};
    active.create_object<update_probe>(game, called);

    game.update(1.0f / 60.0f);

    EXPECT_TRUE(called);
    EXPECT_EQ(game.world(), &active);
    game.end_frame();
    EXPECT_EQ(game.world(), nullptr);
}
} // namespace
