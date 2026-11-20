#include "line.hlsli"

pixel_input main(vertex_input input)
{
    pixel_input output;
    output.position = float4(input.position.x / target_size.x * 2.0f - 1.0f, 1.0f - input.position.y / target_size.y * 2.0f, 0.0f, 1.0f);
    output.local = input.local;
    return output;
}
