#include <nori/graphics/shader/text.hlsli>

nori_text_pixel_input main(nori_text_vertex_input input)
{
    return nori_text_vertex(input, input.position);
}
