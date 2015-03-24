#pragma once

/// Currently implemented as a CPU ray tracer. May shuffle things around later
/// to allow alternate implementations, like GPU or Compute.
/// Creates and maintains all rendering resources required internally.
class Raytracer
{
public:
    static Raytracer* Create(HWND window);
    ~Raytracer();

    int GetNumThreads() const { return NumThreads; }

    void SetFOV(float horizFovRadians);
    bool Render(FXMMATRIX cameraWorldTransform);

private:
    Raytracer(HWND hwnd);

    // Don't allow copy
    Raytracer(const Raytracer&);
    Raytracer& operator= (const Raytracer&);

    bool Initialize();
    void Clear();
    bool Present();

    static DWORD CALLBACK RenderThreadProc(PVOID data);
    void ProcessRenderJob(int index);

    // Create a test scene
    bool GenerateTestScene();

    //
    // Tracing
    //
    struct RayIntersection
    {
        enum class IntersectionType { Triangle };
        IntersectionType Type;  // Type of intersection
        float Dist;             // Distance along ray before hit
        XMFLOAT3 Point;         // Point on triangle
        XMFLOAT3 Normal;        // Normal at contact
        float wA, wB, wC;       // Barycentric weights, used for interpolating
        int StartIndex;         // Start index of the triangle it hit
    };

    // Trace a ray through the scene until it hits something. Return information about what it hit.
    bool TraceRay(FXMVECTOR start, FXMVECTOR dir, /* optional */ RayIntersection* intersection);
    bool RayTriangleIntersect(FXMVECTOR start, FXMVECTOR dir, int startVertex, /* optional */ RayIntersection* intersection);

    // Compute shading for a given point
    XMVECTOR ShadePoint(FXMVECTOR dir, const RayIntersection& intersection, int depth = 0);
    uint32_t ConvertColorToUint(FXMVECTOR color);

private:
    // Basic rendering/buffer
    HWND Window;
    HDC BackBufferDC;
    int Width;
    int Height;
    uint32_t* Pixels;

    // For computing eye rays
    float HalfWidth;
    float HalfHeight;
    float hFov;
    float DistToProjPlane;

    // Simple scene as triangles for testing currently
    XMFLOAT3* Vertices;
    XMFLOAT2* TexCoords;
    int NumVertices; // Must be multiple of 3
    int NumTriangles;

    // Triangle-wide properties
    struct SurfaceProp
    {
        float Reflectiveness;
        XMFLOAT3 Color;
        int Texture; // -1 means no texture
    };
    SurfaceProp* SurfaceProps;

    struct Texture
    {
        int Width;
        int Height;
        uint32_t* Pixels;
    };
    Texture* Textures;
    int NumTextures;

    XMFLOAT3 AmbientLight;

    struct PointLight
    {
        XMFLOAT3 Position;
        XMFLOAT3 Color;
        float Radius;
    };

    PointLight* PointLights;
    int NumPointLights;

    // Passes/frames
    static const int NumBounces = 2;

    // Multithreading
    struct ThreadStartInfo
    {
        Raytracer* This;
        int ThreadIndex;
    };

    struct RenderRequest
    {
        XMFLOAT4X4 CameraWorld;
        int StartRow;
        int NumRows;
    };

    HANDLE* Threads;
    HANDLE* StartEvents;
    HANDLE* FinishEvents;
    RenderRequest* RenderJobs;
    HANDLE ShutdownEvent;
    int NumThreads;
};
