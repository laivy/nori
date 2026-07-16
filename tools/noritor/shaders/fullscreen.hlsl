// Fullscreen triangle shared by the scene-composition shaders.
struct vertex_output
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D source_texture : register(t0);
SamplerState source_sampler : register(s0);

vertex_output VSMain(uint vertex_id : SV_VertexID)
{
    vertex_output output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 PSMain(vertex_output input) : SV_Target0
{
    return source_texture.Sample(source_sampler, input.uv);
}
