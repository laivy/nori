#include "quad.hlsli"

pixel_input main(vertex_input input)
{
    pixel_input output;
    const float2 pixel = input.position + offset;
    output.position = float4(pixel.x / target_size.x * 2.0f - 1.0f, 1.0f - pixel.y / target_size.y * 2.0f, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}
