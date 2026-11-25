#pragma once
#include <array>
#include <string>

#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::editor
{
class text_outline_example
{
public:
    text_outline_example();

    bool render();
    void draw();
    void release();
    bool is_open() const;
    void set_open(bool open);

private:
    bool prepare();

    bool open_;
    bool dirty_;
    float font_size_;
    float thickness_;
    std::array<float, 4> outline_color_;
    std::array<float, 4> background_color_;
    std::string error_;
    nori::graphics::text::unique_font_handle font_;
    std::array<nori::graphics::text::unique_layout_handle, 4> layouts_;
    nori::graphics::unique_texture_handle texture_;
};
}
