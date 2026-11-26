#pragma once

cbuffer blit_constants : register(b0)
{
    float2 source_offset;
    float2 source_scale;
}

Texture2D source_texture : register(t0);
SamplerState source_sampler : register(s0);

struct vertex_output
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
