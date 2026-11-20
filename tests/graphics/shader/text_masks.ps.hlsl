#include <nori/graphics/shader/text.hlsli>

float4 main(nori_text_pixel_input input) : SV_Target0
{
    return float4(nori_text_fill_coverage(input), nori_text_outline_coverage(input), 0.0f, 1.0f);
}
