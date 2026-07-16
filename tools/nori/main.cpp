#include "nori_application.hpp"

int main(int argc, char** argv)
{
    nori_application app{{argc, argv}};
    app.run();
    return app.exit_code();
}
