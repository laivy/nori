#pragma once
#include <array>
#include <cstddef>
#include <string>

namespace nori::editor
{
class asset_manager;

class console
{
public:
    console();

    void draw(asset_manager& assets);

    bool is_open() const;
    void set_open(bool open);

private:
    std::string text_filter_;
    std::string display_text_;
    std::array<bool, 2> filters_;
    bool open_;

    bool was_at_bottom_;
    bool scroll_to_bottom_;
    std::size_t displayed_entry_count_;
};
} // namespace nori::editor
