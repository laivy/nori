#include <nori/physics2d.hpp>

int main()
{
    nori::physics2d::scene scene;
    const auto body{scene.create_body({
        .type = nori::physics2d::body_type::dynamic_body,
        .transform = {{0.0f, 3.0f}, 0.0f}
    })};
    if (!scene.contains(body))
    {
        return 1;
    }
    scene.step(1.0f / 60.0f);
    const auto pose{scene.get_transform(body)};
    return pose && pose->position.y < 3.0f ? 0 : 1;
}
