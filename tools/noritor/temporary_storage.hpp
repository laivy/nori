#pragma once
#include <cstdint>

#include <nori/resource.hpp>

namespace nori::editor
{
class temporary_storage
{
public:
    temporary_storage();
    ~temporary_storage();

    temporary_storage(const temporary_storage&) = delete;
    temporary_storage(temporary_storage&&) = delete;
    temporary_storage& operator=(const temporary_storage&) = delete;
    temporary_storage& operator=(temporary_storage&&) = delete;

    nori::resource::result<nori::resource::handle> store(nori::resource::handle source);
    void remove(nori::resource::handle stored);

private:
    nori::resource::handle root_;
    std::uint64_t next_id_;
};
} // namespace nori::editor
