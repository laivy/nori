#pragma once
#include <cstdint>
#include <vector>

namespace nori::graphics::text
{
class glyph_atlas;

class page_refs
{
public:
    explicit page_refs(glyph_atlas* atlas) noexcept;
    ~page_refs();

    page_refs(const page_refs&) = delete;
    page_refs(page_refs&& other) noexcept;
    page_refs& operator=(const page_refs&) = delete;
    page_refs& operator=(page_refs&& other) noexcept;

    void retain(std::uint32_t page);

private:
    void reset() noexcept;

    glyph_atlas* atlas_;
    std::vector<std::uint32_t> pages_;
};
} // namespace nori::graphics::text
