#pragma once

cbuffer quad_constants : register(b0)
{
    float2 target_size;
    float2 offset;
    float4 color;
}

Texture2D<float> coverage_texture : register(t0);
SamplerState coverage_sampler : register(s0);

struct vertex_input
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct pixel_input
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
