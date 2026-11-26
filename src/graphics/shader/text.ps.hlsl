#include <nori/graphics/shader/text.hlsli>

float4 main(nori_text_pixel_input input) :
    SV_Target
{
    if (input.mode == 0u)
    {
        return nori_text_glyph_color(input, input.color.rgb, 1.0f);
    }
    const float4 sampled = nori_text_atlas.Sample(nori_text_sampler, input.uv);
    const float4 tinted = sampled * nori_text_tint * input.color;
    return float4(tinted.rgb * tinted.a, tinted.a);
}
