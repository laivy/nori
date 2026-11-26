#pragma once

#include <concepts>
#include <memory>
#include <utility>

#include <nori/gameplay/world/world.hpp>

namespace nori::gameplay
{
class game_instance
{
public:
    game_instance() = default;
    ~game_instance();

    game_instance(const game_instance&) = delete;
    game_instance& operator=(const game_instance&) = delete;

    void stop();
    world& create_world();

    template <typename initializer, typename... arguments>
    requires std::constructible_from<initializer, world&, arguments...>
    world& create_world(arguments&&... args)
    {
        auto& active{create_world()};
        static_cast<void>(initializer(active, std::forward<arguments>(args)...));
        return active;
    }

    void clear_world();
    void update(float delta_seconds);
    void fixed_update(float delta_seconds);

    [[nodiscard]] world* active_world();
    [[nodiscard]] const world* active_world() const;

private:
    std::unique_ptr<world> active_;
};
} // namespace nori::gameplay
