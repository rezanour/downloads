//==============================================================================
// Simple GPU ComputeShader based ray tracer.
// Reza Nourai
//==============================================================================

// Used to compute primary eye rays
cbuffer CameraData
{
    float4x4 CameraWorldTransform;
    float2 HalfSize;
    float DistToProjPlane;
};

struct SphereObject
{
    float3 Center;
    float Radius;
    float3 Color;
    float Reflectiveness;
};

struct PointLight
{
    float3 Position;
    float3 Color;
    float Radius;
};

struct RayIntersection
{
    float3 Position;
    float3 Normal;
    float  Dist;
    SphereObject Sphere;
};

// Global environment map
TextureCube EnvTexture;
SamplerState EnvSampler;

// Scene objects
StructuredBuffer<SphereObject> SphereObjects;
StructuredBuffer<PointLight> PointLights;

// Output RenderTarget (not marked globally coherent because we're only writing
// from the thread groups, and also writing to unique locations)
RWTexture2D<float4> RenderTarget;

//==============================================================================
// Ray/Sphere Test - Find intersection (if any) of Ray & Sphere
//==============================================================================

bool RaySphereIntersect(
    float3 start, float3 dir,
    SphereObject sphere,
    out RayIntersection intersection)
{
    // Rule out rays pointing away from sphere
    if (dot(sphere.Center - start, dir) <= 0)
    {
        return false;
    }

    // Find distance from sphere center to the ray.

    // Start by taking vector from start to sphere center, and crossing
    // dir by that, then crossing the result back with dir to get a normal
    // in the direction of the sphere center
    float3 axis = cross(dir, sphere.Center - start);
    if (length(axis) < 0.01f)
    {
        // hit, center is in dir already
        intersection.Position = sphere.Center - dir * sphere.Radius;
        intersection.Normal = normalize(intersection.Position - sphere.Center);
        intersection.Dist = length(intersection.Position - start);
        intersection.Sphere = sphere;
        return true;
    }

    float3 norm = normalize(cross(axis, dir));
    float d = dot(norm, sphere.Center - start);
    if (d < sphere.Radius)
    {
        float x = sqrt(sphere.Radius * sphere.Radius - d * d);
        intersection.Position = sphere.Center - norm * d - dir * x;
        intersection.Normal = normalize(intersection.Position - sphere.Center);
        intersection.Dist = length(intersection.Position - start);
        intersection.Sphere = sphere;
        return true;
    }

    return false;
}

//==============================================================================
// RayTrace - Find nearest intersection along ray
//==============================================================================

bool RayTrace(float3 start, float3 dir, out RayIntersection intersection)
{
    uint numSpheres;
    uint stride;
    SphereObjects.GetDimensions(numSpheres, stride);

    RayIntersection closest;
    RayIntersection test;
    uint closestIndex = numSpheres;

    for (uint i = 0; i < numSpheres; ++i)
    {
        if (RaySphereIntersect(start, dir, SphereObjects[i], test))
        {
            if (closestIndex == numSpheres || test.Dist < closest.Dist)
            {
                closestIndex = i;
                closest = test;
            }
        }
    }

    if (closestIndex != numSpheres)
    {
        intersection = closest;
        return true;
    }

    return false;
}

//==============================================================================
// Compute light exiting back along viewDir
//==============================================================================

float3 ComputeIrradiance(float3 viewDir, RayIntersection intersection)
{
    // Push position out slightly to avoid self-intersection
    float3 position = intersection.Position;
    float3 normal = intersection.Normal;
    position += normal * 0.001f;

    RayIntersection test;
    float3 totalLight = (float3)0;
    float3 reflection = (float3)0;

    float reflectiveness = intersection.Sphere.Reflectiveness;

    //
    // Direct lighting
    //

    // Point Lights
    uint numPointLights;
    uint stride;
    PointLights.GetDimensions(numPointLights, stride);
    for (uint i = 0; i < numPointLights; ++i)
    {
        PointLight light = PointLights[i];
        float3 lightDir = normalize(light.Position - position);

        // Check for obstruction
        if (!RayTrace(position, lightDir, test))
        {
            // Diffuse
            float nDotL = saturate(dot(normal, lightDir));
            totalLight += light.Color * nDotL;

            if (reflectiveness > 0.5f)
            {
                // Specular
                float3 lightRefl = reflect(-lightDir, normal);
                float vDotR = dot(-viewDir, lightRefl);
                if (vDotR > 0)
                {
                    totalLight += 20 * reflectiveness * light.Color * pow(vDotR, 20 * reflectiveness);
                }
            }
        }
    }

    //
    // Reflection
    //
    float3 reflDir = reflect(viewDir, normal);
    if (reflectiveness > 0.5f && RayTrace(position, reflDir, test))
    {
        // TODO: Determine other object's properly shaded color
        reflection = test.Sphere.Color;
    }
    else
    {
        // Reflect env
        reflection = EnvTexture.SampleLevel(EnvSampler, reflDir, 8 * (1.f - reflectiveness));
    }

    // TODO: Was testing with hardcoded coefficients of diffuse vs reflectance. Should ideally be:
    // (1 - sphere.Reflectiveness) * totalLight + sphere.Reflectiveness * reflection
    return intersection.Sphere.Color * (0.125f * totalLight + 0.875f * reflection * dot(reflDir, normal));
}

//==============================================================================
// Main entry point
//==============================================================================

// 4x4 pixel blocks
[numthreads(4, 4, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 GroupID : SV_GroupID)
{
    // Determine which pixel we correspond to:
    // Each thread group is 4x4, and GroupID tells us what tile we are
    // GroupThreadID tells us which element in the 4x4 we are
    uint2 pixelCoord = uint2(GroupID.x * 4 + GroupThreadID.x, GroupID.y * 4 + GroupThreadID.y);

    // TODO: The ray for each pixel is constant for a given resolution, fov, and DistToPlane.
    // Might be worth building a lookup texture to use here instead of the math below, though it's
    // not clear whether that's any faster or not, especially since the multiply by CameraWorldTransform
    // has to happen anyways.

    // Compute ray through this pixel (locally, first)
    float3 rayDir = float3((float)pixelCoord.x - HalfSize.x, HalfSize.y - (float)pixelCoord.y, DistToProjPlane);

    // Transform the local ray to the camera's world orientation
    rayDir = mul((float3x3)CameraWorldTransform, normalize(rayDir));

    // Pick the ray start position to be camera's position
    float3 rayStart = CameraWorldTransform._m03_m13_m23;

    RayIntersection intersection;
    if (RayTrace(rayStart, rayDir, intersection))
    {
        RenderTarget[pixelCoord] = float4(ComputeIrradiance(rayDir, intersection), 1);
    }
    else
    {
        RenderTarget[pixelCoord] = EnvTexture.SampleLevel(EnvSampler, rayDir, 0);
    }
}