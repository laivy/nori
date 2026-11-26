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

world& game_instance::create_world()
{
    clear_world();
    active_ = std::make_unique<world>();
    return *active_;
}

void game_instance::clear_world()
{
    active_.reset();
}

void game_instance::update(float delta_seconds)
{
    if (active_)
    {
        active_->update(delta_seconds);
    }
}

void game_instance::fixed_update(float delta_seconds)
{
    if (active_)
    {
        active_->fixed_update(delta_seconds);
    }
}

world* game_instance::active_world()
{
    return active_.get();
}

const world* game_instance::active_world() const
{
    return active_.get();
}
} // namespace nori::gameplay
