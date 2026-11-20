#include <nori/graphics/shader/text.hlsli>

cbuffer effect_constants : register(b1)
{
    float4 effect_color;
}

float4 main(nori_text_pixel_input input) : SV_Target0
{
    return nori_text_glyph_color(input, effect_color.rgb, effect_color.a);
}
