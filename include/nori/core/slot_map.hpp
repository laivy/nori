#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "handle.hpp"

namespace nori::core
{
template <typename Handle, typename T>
class slot_map;

template <typename Tag, typename T>
requires std::move_constructible<T>
class slot_map<handle<Tag>, T>
{
public:
    using handle_type = handle<Tag>;
    using value_type = std::pair<const handle_type, T>;
    using size_type = std::uint32_t;

    template <bool IsConst>
    class basic_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = typename slot_map::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<IsConst, const value_type&, value_type&>;
        using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;

    private:
        using map_type = std::conditional_t<IsConst, const slot_map, slot_map>;

    public:
        basic_iterator() = default;

        basic_iterator(map_type* map, std::size_t index) :
            map_{map},
            index_{index}
        {
            skip_empty();
        }

        [[nodiscard]]
        reference operator*() const
        {
            return *map_->slots_[index_].value;
        }

        [[nodiscard]]
        pointer operator->() const
        {
            return &**this;
        }

        basic_iterator& operator++()
        {
            ++index_;
            skip_empty();
            return *this;
        }

        basic_iterator operator++(int)
        {
            basic_iterator result{*this};
            ++*this;
            return result;
        }

        bool operator==(const basic_iterator&) const = default;

    private:
        void skip_empty()
        {
            if (!map_)
            {
                return;
            }
            while (index_ < map_->slots_.size() && !map_->slots_[index_].value)
            {
                ++index_;
            }
        }

    private:
        map_type* map_{};
        std::size_t index_{};
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

private:
    struct slot
    {
        slot() :
            generation{1},
            next_free{0}
        {
        }

        template <typename... Args>
        explicit slot(handle_type handle, Args&&... args) :
            value{
                std::in_place,
                std::piecewise_construct,
                std::forward_as_tuple(handle),
                std::forward_as_tuple(std::forward<Args>(args)...)
            },
            generation{1},
            next_free{0}
        {
        }

        std::optional<value_type> value;
        std::uint32_t generation;
        size_type next_free;
    };

public:
    slot_map() :
        slots_{1},
        free_head_{0},
        size_{0}
    {
    }

    template <typename... Args>
    handle_type emplace(Args&&... args)
    {
        if (free_head_ != 0)
        {
            const size_type index{free_head_};
            auto& entry{slots_[index]};
            entry.value.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(handle_type{index, entry.generation}),
                std::forward_as_tuple(std::forward<Args>(args)...)
            );
            free_head_ = entry.next_free;
            entry.next_free = 0;
            ++size_;
            return entry.value->first;
        }

        if (slots_.size() > std::numeric_limits<size_type>::max())
        {
            throw std::overflow_error{"slot_map handle index exhausted."};
        }

        const auto index{static_cast<size_type>(slots_.size())};
        const handle_type handle{index, 1};
        slots_.emplace_back(handle, std::forward<Args>(args)...);
        ++size_;
        return handle;
    }

    void erase(handle_type handle)
    {
        auto& entry{lookup(handle)};
        if (entry.generation == std::numeric_limits<size_type>::max())
        {
            throw std::overflow_error{"slot_map handle generation exhausted."};
        }
        entry.value.reset();
        ++entry.generation;
        entry.next_free = free_head_;
        free_head_ = handle.index;
        --size_;
    }

    [[nodiscard]]
    bool contains(handle_type handle) const noexcept
    {
        if (handle.index == 0 || handle.index >= slots_.size())
        {
            return false;
        }
        auto& entry{slots_[handle.index]};
        if (!entry.value)
        {
            return false;
        }
        if (entry.generation != handle.generation)
        {
            return false;
        }
        return true;
    }

    [[nodiscard]]
    T& get(handle_type handle)
    {
        return lookup(handle).value->second;
    }

    [[nodiscard]]
    const T& get(handle_type handle) const
    {
        return lookup(handle).value->second;
    }

    [[nodiscard]]
    size_type size() const noexcept
    {
        return size_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]]
    iterator begin() noexcept
    {
        return iterator{this, 1};
    }

    [[nodiscard]]
    iterator end() noexcept
    {
        return iterator{this, slots_.size()};
    }

    [[nodiscard]]
    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    [[nodiscard]]
    const_iterator end() const noexcept
    {
        return cend();
    }

    [[nodiscard]]
    const_iterator cbegin() const noexcept
    {
        return const_iterator{this, 1};
    }

    [[nodiscard]]
    const_iterator cend() const noexcept
    {
        return const_iterator{this, slots_.size()};
    }

private:
    [[nodiscard]]
    decltype(auto) lookup(this auto& self, handle_type handle)
    {
        if (!self.contains(handle))
        {
            throw std::out_of_range{"Invalid slot_map handle."};
        }
        return (self.slots_[handle.index]);
    }

private:
    std::vector<slot> slots_;
    size_type free_head_;
    size_type size_;
};
} // namespace nori::core
