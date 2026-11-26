#pragma once
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include <nori/resource/model.hpp>

namespace nori::resource
{
result<model> read_model(handle target);
}
