//==============================================================================
// Common structs and helper functions.
// Reza Nourai
//==============================================================================

//==============================================================================
// Data structures
//==============================================================================

// Used to compute primary eye rays
cbuffer CameraData
{
    float4x4 CameraWorldTransform;
    float2 HalfViewportSize;
    float DistToProjPlane;
};

// Common vertex type
struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 Color;
};

// Indices of a triangle
struct Triangle
{
    int Vertices[3];
};

// An object descriptor
struct Object
{
    int NumTriangles;
    int FirstTriangle;
};

// A node in the AABB bounding volume hierarchy
struct AabbNode
{
    float3 LeftMin, LeftMax;    // AABB of left child
    float3 RightMin, RightMax;  // AABB of right child

    // Index of left & right child
    // >= 0 means node
    // < 0 means object, compute object index by -(i+1)
    int LeftIndex, RightIndex;
};

// Task node in per-pixel linked list
struct Task
{
    int Node;
    int Next;
};

//==============================================================================
// Helper functions
//==============================================================================

float3 LocalRayFromPixelCoord(uint2 pixelCoord, float2 halfViewportSize, float distToProjPlane)
{
    return float3((float)pixelCoord.x - halfViewportSize.x, halfViewportSize.y - (float)pixelCoord.y, distToProjPlane);
}

bool RayAabbIntersect(float3 rayStart, float3 rayDir, float3 aabbMin, float3 aabbMax)
{
    float h;
    float3 p;

    // Check x
    if (rayStart.x < aabbMin.x && rayDir.x > 0)
    {
        h = (aabbMin.x - rayStart.x) / dot(float3(1, 0, 0), rayDir);
        p = rayStart + rayDir * h;
        if (p.y >= aabbMin.y && p.y <= aabbMax.y && p.z >= aabbMin.z && p.z <= aabbMax.z)
        {
            return true;
        }
    }
    else if (rayStart.x > aabbMax.x && rayDir.x < 0)
    {
        h = (rayStart.x - aabbMax.x) / dot(float3(-1, 0, 0), rayDir);
        p = rayStart + rayDir * h;
        if (p.y >= aabbMin.y && p.y <= aabbMax.y && p.z >= aabbMin.z && p.z <= aabbMax.z)
        {
            return true;
        }
    }

    // Check y
    if (rayStart.y < aabbMin.y && rayDir.y > 0)
    {
        h = (aabbMin.y - rayStart.y) / dot(float3(0, 1, 0), rayDir);
        p = rayStart + rayDir * h;
        if (p.x >= aabbMin.x && p.x <= aabbMax.x && p.z >= aabbMin.z && p.z <= aabbMax.z)
        {
            return true;
        }
    }
    else if (rayStart.y > aabbMax.y && rayDir.y < 0)
    {
        h = (rayStart.y - aabbMax.y) / dot(float3(0, -1, 0), rayDir);
        p = rayStart + rayDir * h;
        if (p.x >= aabbMin.x && p.x <= aabbMax.x && p.z >= aabbMin.z && p.z <= aabbMax.z)
        {
            return true;
        }
    }

    // Check z
    if (rayStart.z < aabbMin.z && rayDir.z > 0)
    {
        h = (aabbMin.z - rayStart.z) / dot(float3(0, 0, 1), rayDir);
        p = rayStart + rayDir * h;
        if (p.x >= aabbMin.x && p.x <= aabbMax.x && p.y >= aabbMin.y && p.y <= aabbMax.y)
        {
            return true;
        }
    }
    else if (rayStart.z > aabbMax.z && rayDir.z < 0)
    {
        h = (rayStart.z - aabbMax.z) / dot(float3(0, 0, -1), rayDir);
        p = rayStart + rayDir * h;
        if (p.x >= aabbMin.x && p.x <= aabbMax.x && p.y >= aabbMin.y && p.y <= aabbMax.y)
        {
            return true;
        }
    }

    return false;
}
