#include "rect.hlsli"

float4 main(pixel_input input) : SV_Target
{
    const float4 color = input.color * tint;
    return float4(color.rgb * color.a, color.a);
}
