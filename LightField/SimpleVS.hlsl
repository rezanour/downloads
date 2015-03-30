cbuffer Constants
{
    float4x4 ViewProjection;
};

struct Vertex
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct OutVertex
{
    float4 Position : SV_POSITION;
    float3 Color : COLOR;
};

OutVertex main(Vertex input)
{
    OutVertex output;
    output.Position = mul(ViewProjection, float4(input.Position, 1));
    output.Color = input.Color;
    return output;
}
