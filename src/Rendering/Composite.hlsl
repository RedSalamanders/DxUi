struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex VSMain(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    Vertex output;
    output.uv = uv;
    output.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
Texture2D surface : register(t0);
SamplerState linearClamp : register(s0);
float4 PSMain(Vertex input) : SV_TARGET { return surface.Sample(linearClamp, input.uv); }
