#pragma once
#include <nori/core/application.hpp>

namespace nori::core
{
class console_application : public application
{
public:
    using application::application;

    [[nodiscard]] int run() override final;

private:
    void on_platform_begin_frame() override final;
};
} // namespace nori::core
