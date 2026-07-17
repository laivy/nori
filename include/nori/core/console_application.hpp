#pragma once
#include "application.hpp"

namespace nori::core
{
class console_application : public application
{
public:
    struct specification
    {
        int argc;
        char** argv;
    };

public:
    console_application(const specification& spec);
};
} // namespace nori::core
