//==============================================================================
// Subsequent passes of compute shader based ray tracer.
// Reza Nourai
//==============================================================================

#include "ShaderCommon.hlsli"

StructuredBuffer<Vertex> Vertices;
StructuredBuffer<Triangle> Triangles;
StructuredBuffer<AabbNode> Nodes;
StructuredBuffer<Object> Objects;
RWStructuredBuffer<Task> Tasks;
RWTexture2D<int> TaskHead;
RWTexture2D<float4> RenderTarget;

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
            Tasks[i] = t;
            int iNext = TaskHead[pixelCoord];
            if (iNext >= 0)
            {
                t.Next = iNext;
                TaskHead[pixelCoord] = i;
            }
            TaskHead[pixelCoord] = i;
        }
        if (RayAabbIntersect(rayStart, rayDir, node.RightMin, node.RightMax))
        {
            Task t;
            t.Node = node.RightIndex;
            t.Next = -1;
            uint i = Tasks.IncrementCounter();
            Tasks[i] = t;
            int iNext = TaskHead[pixelCoord];
            if (iNext >= 0)
            {
                t.Next = iNext;
                TaskHead[pixelCoord] = i;
            }
            TaskHead[pixelCoord] = i;
        }
    }
    else
    {
        // It's an object
        Object object = Objects[-(t.Node + 1)];

        // TODO: intersect with object and update RenderTarget[pixelCoord].
    }
}
