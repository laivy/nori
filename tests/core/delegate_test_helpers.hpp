#pragma once

#include <functional>
#include <utility>

namespace
{
class callback_target
{
public:
    callback_target();
    int increment(int amount);
    int value() const;

private:
    int value_;
};

callback_target::callback_target() :
    value_{0}
{
}

int callback_target::increment(int amount)
{
    value_ += amount;
    return value_;
}

int callback_target::value() const
{
    return value_;
}

class destructor_action
{
public:
    explicit destructor_action(std::move_only_function<void()> func);
    ~destructor_action();

private:
    std::move_only_function<void()> func_;
};

destructor_action::destructor_action(std::move_only_function<void()> func) :
    func_{std::move(func)}
{
}

destructor_action::~destructor_action()
{
    func_();
}

int double_value(int value)
{
    return value * 2;
}
} // namespace
