#include "blit.hlsli"

float4 main(vertex_output input) : SV_Target0
{
    return source_texture.Sample(source_sampler, source_offset + input.uv * source_scale);
}
