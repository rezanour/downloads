struct OutVertex
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint SlabID : TEXCOORD1;
};

Texture2D stPlane;

Texture2DArray Slices;

SamplerState PointSampler : s0;
SamplerState LinearSampler : s1;

float4 main(OutVertex input) : SV_TARGET
{
    uint width, height, numLevels;
    stPlane.GetDimensions(0, width, height, numLevels);

    float4 stSample = stPlane.Sample(LinearSampler, (input.Position.xy / float2(width, height)));

    if (stSample.z == 0)
    {
        clip(-1);
    }

#if 1

    uint y0 = (uint)(input.TexCoord.y * 16);
    uint y1 = y0 + 1;
    if (y1 >= 16)
    {
        y1 = 15;
    }

    uint x0 = (uint)(input.TexCoord.x * 16);
    uint x1 = x0 + 1;
    if (x1 >= 16)
    {
        x1 = 15;
    }

    float yLerp = (input.TexCoord.y * 16) - (float)y0;
    float xLerp = (input.TexCoord.x * 16) - (float)x0;

    float4 sample0 = Slices.Sample(LinearSampler, float3(stSample.xy, (float)(y0 * 16 + x0)));
    float4 sample1 = Slices.Sample(LinearSampler, float3(stSample.xy, (float)(y0 * 16 + x1)));
    float4 sample2 = Slices.Sample(LinearSampler, float3(stSample.xy, (float)(y1 * 16 + x0)));
    float4 sample3 = Slices.Sample(LinearSampler, float3(stSample.xy, (float)(y1 * 16 + x1)));

    float4 sample01 = lerp(sample0, sample1, xLerp);
    float4 sample23 = lerp(sample2, sample3, xLerp);
    return float4(lerp(sample01, sample23, yLerp).xyz, 1.f);

#else

    uint x = (uint)(input.TexCoord.x * 16);
    uint y = (uint)(input.TexCoord.y * 16);

    return float4(Slices.Sample(LinearSampler, float3(stSample.xy, (float)(y * 16 + x))).xyz, 1);

#endif
}
