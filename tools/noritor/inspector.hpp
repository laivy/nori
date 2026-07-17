#pragma once
#include "context.hpp"

class noritor_application;

class inspector
{
private:
    struct file_dialog_context
    {
        nori::resource::handle handle;
    };

public:
    explicit inspector(noritor_application& app);

    inspector(const inspector&) = delete;
    inspector(inspector&&) = delete;
    inspector& operator=(const inspector&) = delete;
    inspector& operator=(inspector&&) = delete;

    void render();

private:
    void render_image_file_dialog();

private:
    noritor_application& app_;
    file_dialog_context file_dialog_context_;
};
