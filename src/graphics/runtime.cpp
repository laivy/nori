#include "context.hpp"
#include "runtime.hpp"

namespace nori::graphics
{
runtime::runtime()
{
    context::initialize();
}

runtime::~runtime() noexcept
{
    context::reset();
}
} // namespace nori::graphics
