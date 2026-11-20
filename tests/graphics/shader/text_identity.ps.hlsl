#include <nori/graphics/shader/text.hlsli>

float4 main(nori_text_pixel_input input) : SV_Target0
{
    return nori_text_glyph_color(input, input.color.rgb, 1.0f);
}
