cbuffer Constants
{
    float4x4 World;
    float4x4 ViewProjection;
    uint SlabID;
};

struct Vertex
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct OutVertex
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint SlabID : TEXCOORD1;
};

OutVertex main(Vertex input)
{
    OutVertex output;
    output.Position = mul(ViewProjection, mul(World, float4(input.Position, 1)));
    output.TexCoord = input.TexCoord;
    output.SlabID = SlabID;
    return output;
}
