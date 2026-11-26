#pragma once

namespace nori::editor
{
class settings
{
public:
    settings();

    void initialize();

    void render_menu();

    void push();
    void pop() const;

private:
    void apply_theme();

    float font_size_;
};
} // namespace nori::editor
