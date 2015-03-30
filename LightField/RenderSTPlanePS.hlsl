struct OutVertex
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint SlabID : TEXCOORD1;
};

float4 main(OutVertex input) : SV_TARGET
{
    return float4(input.TexCoord.xy, (float)input.SlabID, 1.f);
}
