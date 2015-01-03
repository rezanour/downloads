cbuffer Constants
{
    uint    NumInstances;   // Total instances being drawn. Sprite's z value = 1 - (zIndex / (NumInstances + 1))
    float4  Viewport;       // 2D viewport (center, size)
};

struct SpriteVertex
{
    float2 Position     : POSITION0;
    float2 TexCoord     : TEXCOORD0;
};

struct SpriteInstance
{
    float4 SourceRect   : POSITION1;
    float2 DestPosition : POSITION2;
    float2 Size         : TEXCOORD1;
    float  Rotation     : TEXCOORD2;
    uint   zIndex       : TEXCOORD3;  // Relative index (order to draw things, used to compute z)
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
    float2x2 transform = float2x2(cosA, -sinA, sinA, cosA);

    // Append scale
    transform = mul(transform, float2x2(instance.Size.x, 0, 0, instance.Size.y));

    // Transform vertex
    output.Position.xy = mul(transform, vertex.Position);

    // Add translation
    output.Position.xy += instance.DestPosition;

    // Transform into viewport
    output.Position.xy = (output.Position.xy - Viewport.xy) / (Viewport.zw * 0.5f);

    // Depth is based on order it was drawn
    output.Position.z = 1.0f - (((float)instance.zIndex + 1.0f) / ((float)NumInstances + 1.0f));

    // w is always 1
    output.Position.w = 1.0f;

    // Compute normalized texture coordinates
    output.TexCoord = vertex.TexCoord * (instance.SourceRect.zw - instance.SourceRect.xy) + instance.SourceRect.xy;

    return output;
}
