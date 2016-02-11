//==============================================================================
// Subsequent passes of compute shader based ray tracer.
// Reza Nourai
//==============================================================================

#include "ShaderCommon.hlsli"

StructuredBuffer<AabbNode> Nodes;

RWTexture2D<float4> RenderTarget;
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

    // Fetch current task for this pixel
    int iTask = TaskHead[pixelCoord];

    if (iTask < 0)
    {
        // Nothing to do for this pixel
        return;
    }

    // Get local ray for this pixel
    float3 localRay = LocalRayFromPixelCoord(pixelCoord, HalfViewportSize, DistToProjPlane);

    // Transform to world space
    float3 rayDir = mul((float3x3)CameraWorldTransform, normalize(localRay));

    // Use camera position as ray start position
    float3 rayStart = CameraWorldTransform._m03_m13_m23;
        
    // Process task
    Task t = Tasks[iTask];

    // Update head to point to next task
    TaskHead[pixelCoord] = t.Next >= 0 ? t.Next : -1;

    if (t.Node >= 0)
    {
        // It's another node in the hierarchy
        AabbNode node = Nodes[t.Node];

        // Ray trace against node's children.
        // If no hit, we're done.
        // If either side hits, append to Tasks list and update head index for this pixel
        if (RayAabbIntersect(rayStart, rayDir, node.LeftMin, node.LeftMax))
        {
            Task t;
            t.Node = node.LeftIndex;
            t.Next = -1;
            uint i = Tasks.IncrementCounter();
            int iNext = TaskHead[pixelCoord];
            if (iNext >= 0)
            {
                t.Next = iNext;
            }
            Tasks[i] = t;
            TaskHead[pixelCoord] = i;
        }
        if (RayAabbIntersect(rayStart, rayDir, node.RightMin, node.RightMax))
        {
            Task t;
            t.Node = node.RightIndex;
            t.Next = -1;
            uint i = Tasks.IncrementCounter();
            int iNext = TaskHead[pixelCoord];
            if (iNext >= 0)
            {
                t.Next = iNext;
            }
            Tasks[i] = t;
            TaskHead[pixelCoord] = i;
        }
    }
    else
    {
        // TODO: intersect with object and update RenderTarget[pixelCoord].
        // For now, we'll just assume that each leaf object is just a filled in solid AABB
        RenderTarget[pixelCoord] = float4(abs(rayDir), 1);
    }
}
