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

    bool IsBlurEnabled() const { return BlurEnabled; }
    void EnableBlur(bool enabled) { BlurEnabled = enabled; }

    void Clear();
    bool Render(FXMMATRIX cameraWorldTransform);

private:
    Raytracer(HWND hwnd);

    // Don't allow copy
    Raytracer(const Raytracer&);
    Raytracer& operator= (const Raytracer&);

    bool Initialize();
    bool Present();

    static DWORD CALLBACK RenderThreadProc(PVOID data);
    void ProcessRenderJob(long index);

    // Create a test scene
    bool GenerateTestScene();

    //
    // Tracing
    //
    struct RayIntersection
    {
        float Dist;             // Distance along ray before hit
        XMFLOAT3 Point;         // Point on triangle
        XMFLOAT3 Normal;        // Normal at contact
        float wA, wB, wC;       // Barycentric weights, used for interpolating
        int StartIndex;         // Start index of the triangle it hit
    };

    // Trace a ray through the scene until it hits something. Return information about what it hit.
    bool TraceRay(FXMVECTOR start, FXMVECTOR dir, RayIntersection* intersection);
    bool RayTriangleIntersect(FXMVECTOR start, FXMVECTOR dir, int startVertex, RayIntersection* intersection);

    // Compute shading for a given point
    XMVECTOR ComputeRadiance(FXMVECTOR dir, const RayIntersection& intersection, int depth = 0);
    XMVECTOR PickRandomVectorInHemisphere(FXMVECTOR normal);
    uint32_t ConvertColorToUint(FXMVECTOR color);

private:
    static const int NumBounces = 3;
    static const int TileSize = 4;

    // Basic rendering/buffer
    HWND Window;
    HDC BackBufferDC;
    int Width;
    int Height;
    uint32_t* Pixels;
    std::unique_ptr<XMFLOAT4[]> Accum; // RGB + numSamples

    // For computing eye rays
    float HalfWidth;
    float HalfHeight;
    float hFov;
    float DistToProjPlane;

    // Simple scene as triangles for testing currently
    std::unique_ptr<XMFLOAT3[]> Vertices;
    std::unique_ptr<XMFLOAT2[]> TexCoords;
    int NumVertices; // Must be multiple of 3
    int NumTriangles;

    // Triangle-wide properties
    struct SurfaceProp
    {
        XMFLOAT3 Color;
        XMFLOAT3 Emission;
        int Texture; // -1 means no texture
    };
    std::unique_ptr<SurfaceProp[]> SurfaceProps;

    struct Texture
    {
        int Width;
        int Height;
        std::unique_ptr<uint32_t[]> Pixels;
    };
    std::unique_ptr<Texture[]> Textures;
    int NumTextures;

    // Multithreading
    struct RenderRequest
    {
        XMFLOAT4X4 CameraWorld;
        int minX, maxX;
        int minY, maxY;
    };

    Event StartEvent;
    Event FinishEvent;
    Event ShutdownEvent;
    std::unique_ptr<HANDLE[]> Threads;
    int NumThreads;
    std::unique_ptr<RenderRequest[]> RenderJobs;
    volatile long NumRenderJobs;

    // Blur
    bool BlurEnabled;
};
