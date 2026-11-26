cbuffer sample_constants : register(b0)
{
    float2 source_offset;
    float2 source_scale;
}

Texture2D source_texture : register(t0);
SamplerState source_sampler : register(s0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    return source_texture.Sample(source_sampler, source_offset + uv * source_scale);
}
