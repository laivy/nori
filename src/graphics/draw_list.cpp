#include "draw_list.hpp"

namespace nori::graphics
{
void draw_list::add(const model& value)
{
    models_.push_back(value);
}

void draw_list::add(const quad& value)
{
    quads_.push_back(value);
}

void draw_list::add(const fullscreen_draw& value)
{
    fullscreen_draws_.push_back(value);
}

void draw_list::clear()
{
    models_.clear();
    quads_.clear();
    fullscreen_draws_.clear();
}

std::span<const model> draw_list::models() const noexcept
{
    return models_;
}

std::span<const quad> draw_list::quads() const noexcept
{
    return quads_;
}

std::span<const fullscreen_draw> draw_list::fullscreen_draws() const noexcept
{
    return fullscreen_draws_;
}
} // namespace nori::graphics
