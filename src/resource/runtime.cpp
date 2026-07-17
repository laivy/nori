#include <utility>
#include "context.hpp"
#include "runtime.hpp"

namespace nori::resource
{
runtime::runtime(specification spec)
{
    assert(!context::instance());
    context::initialize(std::move(spec));
    assert(context::instance());
}

runtime::~runtime() noexcept
{
    assert(context::instance());
    context::reset();
}
} // namespace nori::resource
#include <cassert>
