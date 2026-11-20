#pragma once

cbuffer rect_constants : register(b0)
{
    float2 target_size;
    float2 offset;
    float4 tint;
}

struct vertex_input
{
    float2 position : POSITION;
    float4 color : COLOR0;
};

struct pixel_input
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};
