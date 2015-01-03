Texture2D SpriteTexture;
sampler Sampler;

float4 main(float4 Position : SV_POSITION,  float2 TexCoord : TEXCOORD) : SV_TARGET
{
    float4 color = SpriteTexture.Sample(Sampler, TexCoord);
    clip(color.a - 0.25f);  // Clip near-transparent pixels completely
    return color;
}