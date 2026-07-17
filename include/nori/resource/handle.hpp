#pragma once
#include <nori/core/handle.hpp>

namespace nori::resource
{
struct resource_tag;
using handle = core::handle<resource_tag>;
} // namespace nori::resource
