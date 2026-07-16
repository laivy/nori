#pragma once
#include <nori/core/console_application.hpp>
#include <nori/resource.hpp>

class nori_application : public nori::core::console_application
{
public:
    explicit nori_application(const specification& spec);
    ~nori_application() override;

    void run() override;
    void quit(nori::core::application::exit_code exit_code) override;

    [[nodiscard]]
    int exit_code() const;

private:
    nori::resource::runtime resource_;
    int argc_;
    char** argv_;
    nori::core::application::exit_code exit_code_;
};
