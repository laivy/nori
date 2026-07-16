#include "accessor.hpp"
#include "iterator.hpp"

namespace nori::resource
{
iterator::iterator() :
    index_{0}
{
}

iterator::iterator(handle target) :
    iterator{}
{
    if (target)
    {
        children_ = get_children(target);
    }
}

iterator::iterator(std::string_view path) :
    iterator{}
{
    if (const handle target{get(path)})
    {
        children_ = get_children(target);
    }
}

iterator& iterator::operator++()
{
    ++index_;
    return *this;
}

iterator iterator::operator++(int)
{
    iterator tmp{*this};
    ++*this;
    return tmp;
}

iterator::value_type iterator::operator*() const
{
    const handle target{children_.at(index_)};
    return {get_name(target), target};
}

bool iterator::operator==(const iterator& other) const
{
    if (children_ != other.children_)
    {
        return false;
    }
    if (index_ != other.index_)
    {
        return false;
    }
    return true;
}

iterator iterator::begin() const
{
    return *this;
}

iterator iterator::end() const
{
    iterator it{*this};
    it.index_ = it.children_.size();
    return it;
}
} // namespace nori::resource
