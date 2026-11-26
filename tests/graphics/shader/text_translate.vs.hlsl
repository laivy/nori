#include <nori/graphics/shader/text.hlsli>

cbuffer effect_constants : register(b1)
{
    float4 translation;
}

nori_text_pixel_input main(nori_text_vertex_input input)
{
    return nori_text_vertex(input, input.position + translation.xy);
}
