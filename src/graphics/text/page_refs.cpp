#include <algorithm>
#include <utility>

#include "text/page_refs.hpp"
#include "text/glyph_atlas.hpp"

namespace nori::graphics::text
{
page_refs::page_refs(glyph_atlas* atlas) noexcept :
    atlas_{atlas}
{
}

page_refs::~page_refs()
{
    reset();
}

page_refs::page_refs(page_refs&& other) noexcept :
    atlas_{std::exchange(other.atlas_, nullptr)},
    pages_{std::move(other.pages_)}
{
}

page_refs& page_refs::operator=(page_refs&& other) noexcept
{
    if (this != &other)
    {
        reset();
        atlas_ = std::exchange(other.atlas_, nullptr);
        pages_ = std::move(other.pages_);
    }
    return *this;
}

void page_refs::retain(std::uint32_t page)
{
    if (!std::ranges::contains(pages_, page))
    {
        atlas_->retain(page);
        pages_.push_back(page);
    }
}

void page_refs::reset() noexcept
{
    if (atlas_)
    {
        atlas_->release(pages_);
        atlas_ = nullptr;
    }
}
} // namespace nori::graphics::text
