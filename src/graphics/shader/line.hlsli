#pragma once

cbuffer line_constants : register(b0)
{
    float2 target_size;
    float2 half_size;
    float4 color;
}

struct vertex_input
{
    float2 position : POSITION;
    float2 local : TEXCOORD0;
};

struct pixel_input
{
    float4 position : SV_Position;
    float2 local : TEXCOORD0;
};
