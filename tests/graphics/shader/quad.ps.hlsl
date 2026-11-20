#include "quad.hlsli"

float4 main(pixel_input input) :
    SV_Target
{
    const float alpha = color.a * coverage_texture.Sample(coverage_sampler, input.uv);
    return float4(color.rgb * alpha, alpha);
}
