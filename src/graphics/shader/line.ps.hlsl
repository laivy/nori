#include "line.hlsli"

float4 main(pixel_input input) : SV_Target
{
    const float2 pixel_width = fwidth(input.local);
    const float2 coverage = saturate((input.local + half_size) / pixel_width + 0.5f) - saturate((input.local - half_size) / pixel_width + 0.5f);
    const float alpha = color.a * coverage.x * coverage.y;
    return float4(color.rgb * alpha, alpha);
}
