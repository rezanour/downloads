//==============================================================================
// First pass of compute shader based ray tracer.
// Reza Nourai
//==============================================================================

#include "ShaderCommon.hlsli"

StructuredBuffer<AabbNode> Nodes;

RWTexture2D<int> TaskHead;
RWStructuredBuffer<Task> Tasks;

//==============================================================================
// Main entry point
//==============================================================================

static const uint NUM_PIXELS_PER_GROUP_X = 4;
static const uint NUM_PIXELS_PER_GROUP_Y = 4;

[numthreads(NUM_PIXELS_PER_GROUP_X, NUM_PIXELS_PER_GROUP_Y, 1)]
void main(
    uint3 GroupThreadID : SV_GroupThreadID,
    uint3 GroupID : SV_GroupID,
    uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Determine which pixel we are
    uint2 pixelCoord = GroupID.xy * uint2(NUM_PIXELS_PER_GROUP_X, NUM_PIXELS_PER_GROUP_Y) + GroupThreadID.xy;

    // Get local ray for this pixel
    float3 localRay = LocalRayFromPixelCoord(pixelCoord, HalfViewportSize, DistToProjPlane);

    // Transform to world space
    float3 rayDir = mul((float3x3)CameraWorldTransform, normalize(localRay));

    // Use camera position as ray start position
    float3 rayStart = CameraWorldTransform._m03_m13_m23;

    // Ray trace against root node's children.
    // If no hit, we're done.
    // If either side hits, append to Tasks list and update head index for this pixel
    AabbNode node = Nodes[0];
    uint i = 0xffff;
    if (RayAabbIntersect(rayStart, rayDir, node.LeftMin, node.LeftMax))
    {
        Task t;
        t.Node = node.LeftIndex;
        t.Next = -1;
        i = Tasks.IncrementCounter();
        Tasks[i] = t;
        TaskHead[pixelCoord] = i;
    }
    if (RayAabbIntersect(rayStart, rayDir, node.RightMin, node.RightMax))
    {
        Task t;
        t.Node = node.RightIndex;
        t.Next = -1;
        uint j = Tasks.IncrementCounter();
        if (i != 0xffff)
        {
            t.Next = i;
        }
        Tasks[j] = t;
        TaskHead[pixelCoord] = j;
    }
}
