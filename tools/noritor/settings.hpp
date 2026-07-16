#pragma once

class settings
{
private:
    enum class theme
    {
        classic,
        light,
        dark,
        one_dark_pro
    };

public:
    settings();

    void render_menu();
    void push();
    void pop() const;

private:
    void initialize();
    void apply_theme();
    void push_font() const;
    void pop_font() const;

private:
    theme theme_;
    bool is_theme_dirty_;
    ImFont* font_;
    std::string font_name_;
    float font_size_;
};
