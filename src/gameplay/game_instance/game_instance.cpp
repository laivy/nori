#include <utility>

#include <nori/core/abort.hpp>
#include <nori/gameplay/game_instance/game_instance.hpp>

namespace nori::gameplay
{
game_instance::~game_instance()
{
    stop();
}

void game_instance::stop()
{
    clear_world();
}

gameplay::world& game_instance::create_world()
{
    clear_world();
    world_ = std::make_unique<gameplay::world>();
    return *world_;
}

void game_instance::clear_world()
{
    if (clearing_world_ || (world_ && (world_->is_executing() || world_->flushing_)))
    {
        core::abort("cannot change worlds during execution; request a world change instead");
    }
    pending_world_change_ = nullptr;
    clearing_world_ = true;
    world_.reset();
    clearing_world_ = false;
}

void game_instance::request_clear_world()
{
    pending_world_change_ = [](game_instance& game)
    {
        game.clear_world();
    };
}

void game_instance::update(float delta_seconds)
{
    if (world_)
    {
        world_->update(delta_seconds);
    }
}

void game_instance::fixed_update(float delta_seconds)
{
    if (world_)
    {
        world_->fixed_update(delta_seconds);
    }
}

void game_instance::end_frame()
{
    if (world_)
    {
        world_->end_frame();
    }
    apply_world_change();
}

gameplay::world* game_instance::world()
{
    return world_.get();
}

const gameplay::world* game_instance::world() const
{
    return world_.get();
}

void game_instance::apply_world_change()
{
    if (!clearing_world_ && pending_world_change_)
    {
        auto change{std::exchange(pending_world_change_, nullptr)};
        change(*this);
    }
}
} // namespace nori::gameplay
