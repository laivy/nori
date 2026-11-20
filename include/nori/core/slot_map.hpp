#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/core/handle.hpp>

namespace nori::core
{
template <typename Handle, typename T>
class slot_map;

template <typename Tag, typename T>
requires std::destructible<T>
class slot_map<basic_handle<Tag>, T>
{
public:
    using handle_type = basic_handle<Tag>;
    using value_type = std::pair<const handle_type, T>;
    using size_type = std::uint32_t;

    template <bool Const>
    class basic_iterator
    {
        template <bool>
        friend class basic_iterator;

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = typename slot_map::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const value_type&, value_type&>;
        using pointer = std::conditional_t<Const, const value_type*, value_type*>;

        basic_iterator() noexcept :
            map_{nullptr},
            index_{0}
        {
        }

        basic_iterator(std::conditional_t<Const, const slot_map, slot_map>* map, std::size_t index) noexcept :
            map_{map},
            index_{index}
        {
            skip_empty();
        }

        template <bool OtherConst>
        requires(Const && !OtherConst)
        basic_iterator(const basic_iterator<OtherConst>& other) noexcept :
            map_{other.map_},
            index_{other.index_}
        {
        }

        [[nodiscard]] reference operator*() const noexcept
        {
            return *map_->slots_[index_].value;
        }

        [[nodiscard]] pointer operator->() const noexcept
        {
            return &**this;
        }

        basic_iterator& operator++() noexcept
        {
            ++index_;
            skip_empty();
            return *this;
        }

        basic_iterator operator++(int) noexcept
        {
            basic_iterator result{*this};
            ++*this;
            return result;
        }

        bool operator==(const basic_iterator&) const noexcept = default;

    private:
        using map_type = std::conditional_t<Const, const slot_map, slot_map>;

        void skip_empty() noexcept
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

        map_type* map_;
        std::size_t index_;
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    slot_map() :
        slots_{1},
        free_head_{0},
        size_{0}
    {
    }

    slot_map(const slot_map&)
    requires std::is_copy_constructible_v<T>
    = default;

    slot_map(slot_map&& other) noexcept :
        slot_map{}
    {
        swap(other);
    }

    slot_map& operator=(const slot_map& other)
    requires std::is_copy_constructible_v<T>
    {
        if (this != &other)
        {
            slot_map copy{other};
            swap(copy);
        }
        return *this;
    }

    slot_map& operator=(slot_map&& other) noexcept
    {
        if (this != &other)
        {
            slot_map moved{std::move(other)};
            swap(moved);
        }
        return *this;
    }

    [[nodiscard]] iterator begin() noexcept
    {
        return iterator{this, 1};
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return cbegin();
    }

    [[nodiscard]] iterator end() noexcept
    {
        return iterator{this, slots_.size()};
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
        return cend();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return const_iterator{this, 1};
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
        return const_iterator{this, slots_.size()};
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] decltype(auto) operator[](this auto&& self, handle_type handle) noexcept
    {
        return (self.slots_[handle.index].value->second);
    }

    [[nodiscard]] decltype(auto) at(this auto&& self, handle_type handle) noexcept
    {
        return (self.lookup(handle).value->second);
    }

    template <typename... Args>
    requires std::constructible_from<T, Args...>
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
            core::abort("slot_map handle index exhausted");
        }

        const auto index{static_cast<size_type>(slots_.size())};
        const handle_type handle{index, 1};
        slots_.emplace_back(handle, std::forward<Args>(args)...);
        ++size_;
        return handle;
    }

    void erase(handle_type handle) noexcept
    {
        auto& entry{lookup(handle)};
        if (entry.generation == std::numeric_limits<size_type>::max())
        {
            core::abort("slot_map handle generation exhausted");
        }
        entry.value.reset();
        ++entry.generation;
        entry.next_free = free_head_;
        free_head_ = handle.index;
        --size_;
    }

    void clear() noexcept
    {
        for (const auto& entry : *this)
        {
            erase(entry.first);
        }
    }

    [[nodiscard]] bool contains(handle_type handle) const noexcept
    {
        if (handle.index == 0 || handle.index >= slots_.size())
        {
            return false;
        }
        const auto& entry{slots_[handle.index]};
        return entry.value.has_value() && entry.generation == handle.generation;
    }

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

    [[nodiscard]] decltype(auto) lookup(this auto& self, handle_type handle) noexcept
    {
        return (self.slots_[handle.index]);
    }

    void swap(slot_map& other) noexcept
    {
        slots_.swap(other.slots_);
        std::swap(free_head_, other.free_head_);
        std::swap(size_, other.size_);
    }

    std::deque<slot> slots_;
    size_type free_head_;
    size_type size_;
};
} // namespace nori::core
