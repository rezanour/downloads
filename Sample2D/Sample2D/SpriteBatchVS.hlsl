cbuffer Constants
{
    uint NumInstances;              // Total instances being drawn. Sprite's z value = 1 - (zIndex / (NumInstances + 1))
};

struct SpriteVertex
{
    float2 Offset : POSITION0;      // Quad's position/texturecoord in normalized coordinates (0 -> 1)
};

struct SpriteInstance
{
    float4 SourceRect : POSITION1;  // Source rectangle in texture, in normalized coordinates (0 -> 1)
    float4 DestRect   : POSITION2;  // Destination rectangle on screen, in normalized coordinates (0 -> 1)
    float Rotation    : TEXCOORD0;  // Rotation to apply to sprite, in radians
    uint zIndex       : TEXCOORD1;  // Relative index (order to draw things, used to compute z)
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(SpriteVertex vertex, SpriteInstance instance)
{
    VSOutput output;

    // Compute 2x2 rotation matrix
    float cosA, sinA;
    sincos(instance.Rotation, sinA, cosA);
    float2x2 rotate = float2x2(cosA, -sinA, sinA, cosA);

    // Get initial normalized position
    output.Position.xy = vertex.Offset * (instance.DestRect.zw - instance.DestRect.xy) + instance.DestRect.xy;
    // Flip Y, since viewport Y goes from -1 -> 1 (with 1 being up)
    output.Position.y = 1.0f - output.Position.y;
    // Transform to viewport coordinates (-1 -> 1)
    output.Position.xy = output.Position.xy * 2 - 1;
    // Apply rotation matrix computed above
    output.Position.xy = mul(output.Position.xy, rotate);

    // Depth is based on order it was drawn
    output.Position.z = 1.0f - (((float)instance.zIndex + 1.0f) / ((float)NumInstances + 1.0f));

    // w is always 1
    output.Position.w = 1.0f;

    // Compute normalized texture coordinates
    output.TexCoord = vertex.Offset * (instance.SourceRect.zw - instance.SourceRect.xy) + instance.SourceRect.xy;

    return output;
}
