#include "pch.hpp"

#include "temporary_storage.hpp"

namespace nori::editor
{
temporary_storage::temporary_storage() :
    root_{nori::resource::create_root()},
    next_id_{0}
{
}

temporary_storage::~temporary_storage()
{
    nori::resource::remove(root_);
}

nori::resource::result<nori::resource::handle> temporary_storage::store(nori::resource::handle source)
{
    const nori::resource::handle container{nori::resource::create(root_, std::to_string(next_id_++), nori::resource::empty)};
    if (!container)
    {
        return std::unexpected{nori::resource::error_code::name_conflict};
    }

    const auto copied{nori::resource::copy(source, container)};
    if (!copied)
    {
        nori::resource::remove(container);
        return std::unexpected{copied.error()};
    }
    return *copied;
}

void temporary_storage::remove(nori::resource::handle stored)
{
    nori::resource::remove(nori::resource::parent_of(stored));
}
} // namespace nori::editor
