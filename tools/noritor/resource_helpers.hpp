#pragma once
#include <cassert>
#include <utility>
#include <vector>

#include <nori/resource/operations.hpp>

namespace nori::editor
{
inline std::vector<nori::resource::handle> children_of_loaded(nori::resource::handle target)
{
    auto res{nori::resource::children_of(target)};
    assert(res);
    return std::move(*res);
}
} // namespace nori::editor
