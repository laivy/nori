#pragma once
#include <memory>
#include <utility>

namespace nori::core
{
template <class T>
class singleton
{
public:
    singleton() = default;

    template <class... Args>
    static T* initialize(Args&&... args)
    {
        if (!s_instance)
        {
            s_instance = std::make_unique<T>(std::forward<Args>(args)...);
        }
        return s_instance.get();
    }

    [[nodiscard]]
    static T* instance()
    {
        return s_instance.get();
    }

    static void reset()
    {
        s_instance.reset();
    }

private:
    singleton(singleton&&) = delete;
    singleton(const singleton&) = delete;
    singleton& operator=(singleton&&) = delete;
    singleton& operator=(const singleton&) = delete;

private:
    static inline std::unique_ptr<T> s_instance;
};
} // namespace nori::core
