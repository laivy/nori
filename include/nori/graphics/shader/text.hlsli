#pragma once

cbuffer nori_text_constants : register(b0)
{
    float2 nori_text_target_size;
    float2 nori_text_offset;
    float4 nori_text_tint;
    float2 nori_text_layout_size;
    uint nori_text_outline_pass;
    float nori_text_padding;
}

Texture2D<float4> nori_text_atlas : register(t0);
SamplerState nori_text_sampler : register(s0);

struct nori_text_vertex_input
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 outline_color : COLOR1;
    uint mode : TEXCOORD1;
    float cluster : TEXCOORD2;
    float2 pivot : TEXCOORD3;
};

struct nori_text_pixel_input
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 outline_color : COLOR1;
    nointerpolation uint mode : TEXCOORD1;
    nointerpolation float cluster : TEXCOORD2;
    nointerpolation float2 pivot : TEXCOORD3;
    float2 local_position : TEXCOORD4;
    float fill_alpha : TEXCOORD5;
};

nori_text_pixel_input nori_text_vertex(nori_text_vertex_input input, float2 position)
{
    nori_text_pixel_input output;
    const float2 pixel = position + nori_text_offset;
    output.position = float4(
        pixel.x / nori_text_target_size.x * 2.0f - 1.0f,
        1.0f - pixel.y / nori_text_target_size.y * 2.0f,
        0.0f,
        1.0f
    );
    output.uv = input.uv;
    output.color = nori_text_outline_pass != 0u ? input.outline_color : input.color;
    output.outline_color = input.outline_color;
    output.mode = input.mode;
    output.cluster = input.cluster;
    output.pivot = input.pivot;
    output.local_position = input.position;
    output.fill_alpha = input.color.a;
    return output;
}

float nori_text_fill_coverage(nori_text_pixel_input input)
{
    return nori_text_atlas.Sample(nori_text_sampler, input.uv).r;
}

float nori_text_outline_coverage(nori_text_pixel_input input)
{
    return nori_text_atlas.Sample(nori_text_sampler, input.uv).g;
}

float4 nori_text_glyph_color(nori_text_pixel_input input, float3 rgb, float effect_alpha)
{
    const float coverage = nori_text_outline_pass != 0u ? nori_text_outline_coverage(input) : nori_text_fill_coverage(input);
    float alpha = coverage * input.color.a * nori_text_tint.a * effect_alpha;
    if (nori_text_outline_pass != 0u)
    {
        const float fill_alpha = nori_text_fill_coverage(input) * input.fill_alpha * nori_text_tint.a * effect_alpha;
        alpha = saturate(alpha / max(1.0f - fill_alpha, 0.000001f));
    }
    return float4(rgb * nori_text_tint.rgb * alpha, alpha);
}
