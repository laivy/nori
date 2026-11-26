#pragma once
#include <concepts>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
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
    gameplay::world& create_world();

    template <typename Initializer, typename... Args>
    requires std::constructible_from<Initializer, gameplay::world&, Args...>
    gameplay::world& create_world(Args&&... args)
    {
        auto& new_world{create_world()};
        static_cast<void>(Initializer(new_world, std::forward<Args>(args)...));
        return new_world;
    }

    void clear_world();

    template <typename Initializer, typename... Args>
    requires std::constructible_from<Initializer, gameplay::world&, std::decay_t<Args>...> && (std::constructible_from<std::decay_t<Args>, Args> && ...)
    void request_world_change(Args&&... args)
    {
        pending_world_change_ = [stored = std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}](game_instance& game) mutable
        {
            std::apply([&game](auto&&... stored_args)
            {
                game.create_world<Initializer>(std::move(stored_args)...);
            }, std::move(stored));
        };
    }

    void request_clear_world();
    void update(float delta_seconds);
    void fixed_update(float delta_seconds);
    void end_frame();

    [[nodiscard]] gameplay::world* world();
    [[nodiscard]] const gameplay::world* world() const;

private:
    void apply_world_change();

    std::unique_ptr<gameplay::world> world_;
    std::move_only_function<void(game_instance&)> pending_world_change_;
    bool clearing_world_{};
};
} // namespace nori::gameplay
