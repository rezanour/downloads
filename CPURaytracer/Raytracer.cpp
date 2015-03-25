#include "Precomp.h"
#include "Raytracer.h"
#include "Debug.h"

#include <wincodec.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#if defined (_DEBUG)
#define DISABLE_MULTITHREADED_RENDERING
#endif

Raytracer* Raytracer::Create(HWND window)
{
    Raytracer* raytracer = new (std::nothrow) Raytracer(window);
    if (raytracer)
    {
        if (!raytracer->Initialize())
        {
            delete raytracer;
            raytracer = nullptr;
        }
    }
    return raytracer;
}

Raytracer::Raytracer(HWND hwnd)
    : Window(hwnd)
    , BackBufferDC(nullptr)
    , Pixels(nullptr)
    , Accum(nullptr)
    , hFov(0.f)
    , DistToProjPlane(0.f)
    , Vertices(nullptr)
    , TexCoords(nullptr)
    , SurfaceProps(nullptr)
    , NumVertices(0)
    , NumTriangles(0)
    , Threads(nullptr)
    , StartEvent(nullptr)
    , FinishEvent(nullptr)
    , RenderJobs(nullptr)
    , NumRenderJobs(0)
    , ShutdownEvent(nullptr)
    , NumThreads(0)
    , Textures(nullptr)
    , NumTextures(0)
{
    assert(Window);

    RECT clientRect = {};
    GetClientRect(Window, &clientRect);

    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;
    HalfWidth = Width * 0.5f;
    HalfHeight = Height * 0.5f;
}

Raytracer::~Raytracer()
{
    if (ShutdownEvent)
    {
        SetEvent(ShutdownEvent);
        WaitForMultipleObjects(NumThreads, Threads, TRUE, INFINITE);
        CloseHandle(ShutdownEvent);
        ShutdownEvent = nullptr;
    }

    delete[] RenderJobs;
    RenderJobs = nullptr;

    for (int i = 0; i < NumThreads; ++i)
    {
        if (Threads && Threads[i])
        {
            CloseHandle(Threads[i]);
        }
    }
    delete[] Threads;
    Threads = nullptr;

    if (StartEvent)
    {
        CloseHandle(StartEvent);
        StartEvent = nullptr;
    }

    if (FinishEvent)
    {
        CloseHandle(FinishEvent);
        FinishEvent = nullptr;
    }

    Pixels = nullptr;

    delete[] Accum;
    Accum = nullptr;

    if (BackBufferDC)
    {
        DeleteDC(BackBufferDC);
        BackBufferDC = nullptr;
    }

    delete[] Vertices;
    Vertices = nullptr;

    delete[] TexCoords;
    TexCoords = nullptr;

    delete[] SurfaceProps;
    SurfaceProps = nullptr;

    delete[] Textures;
    Textures = nullptr;
}

void Raytracer::SetFOV(float horizFovRadians)
{
    hFov = horizFovRadians;
    float denom = tanf(hFov * 0.5f);
    assert(!isnan(denom) && fabsf(denom) > 0.00001f);
    DistToProjPlane = (Width * 0.5f) / denom;
}

void Raytracer::Invalidate()
{
    Clear();
}

bool Raytracer::Render(FXMMATRIX cameraWorldTransform)
{
    int numJobs = 0;
    for (int y = 0; y < Height; y += TileSize)
    {
        for (int x = 0; x < Width; x += TileSize)
        {
            XMStoreFloat4x4(&RenderJobs[numJobs].CameraWorld, cameraWorldTransform);
            RenderJobs[numJobs].minX = x;
            RenderJobs[numJobs].maxX = x + TileSize;
            RenderJobs[numJobs].minY = y;
            RenderJobs[numJobs].maxY = y + TileSize;
            ++numJobs;
        }
    }
    NumRenderJobs = numJobs;
    SetEvent(StartEvent);
    WaitForSingleObject(FinishEvent, INFINITE);
    ResetEvent(StartEvent);

    return Present();
}

bool Raytracer::Initialize()
{
    //
    // Create the back buffer & pixel memory
    //
    BITMAPINFO bmi = {};
    HBITMAP bitmap = nullptr;

    HDC hdc = GetDC(Window);
    if (!hdc)
    {
        LogError(L"Failed to obtain window DC.");
        return false;
    }

    BackBufferDC = CreateCompatibleDC(hdc);
    if (!BackBufferDC)
    {
        LogError(L"Failed to create compatible DC.");
        ReleaseDC(Window, hdc);
        return false;
    }

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = Width;
    bmi.bmiHeader.biHeight = -Height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection(BackBufferDC, &bmi, DIB_RGB_COLORS, (PVOID*)&Pixels, nullptr, 0);
    if (!bitmap)
    {
        LogError(L"Failed to create DIB section.");
        ReleaseDC(Window, hdc);
        return false;
    }

    // Select the bitmap (this takes a reference on it)
    SelectObject(BackBufferDC, bitmap);

    // Delete the object (the DC still has a reference)
    DeleteObject(bitmap);

    ReleaseDC(Window, hdc);

    // Create accum buffer
    Accum = new (std::nothrow) XMFLOAT4[Width * Height];
    if (!Accum)
    {
        LogError(L"Failed to allocate accumulation buffer.");
        return false;
    }
    ZeroMemory(Accum, sizeof(XMFLOAT4) * Width * Height);

    if (!GenerateTestScene())
    {
        LogError(L"Failed to create test scene.");
        return false;
    }

    //
    // Create render threads
    //

    RenderJobs = new (std::nothrow) RenderRequest[(Width / TileSize) * (Height / TileSize)];
    if (!RenderJobs)
    {
        LogError(L"Failed to allocate render job list.");
        return false;
    }

    StartEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!StartEvent)
    {
        LogError(L"Failed to create start event.");
        return false;
    }

    FinishEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!FinishEvent)
    {
        LogError(L"Failed to create finish event.");
        return false;
    }

    ShutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ShutdownEvent)
    {
        LogError(L"Failed to create shutdown event.");
        return false;
    }

    // Determine how many processors/cores the machine has
#if !defined(DISABLE_MULTITHREADED_RENDERING)
    DWORD procCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    DWORD procCount = 1;
#endif

    // Since our rendering doesn't stall on I/O other than memory loads, more
    // threads than cores won't help.
    NumThreads = procCount;

    Threads = new (std::nothrow) HANDLE[NumThreads];
    if (!Threads)
    {
        LogError(L"Failed to allocate thread list.");
        return false;
    }
    ZeroMemory(Threads, sizeof(HANDLE) * NumThreads);

    for (int i = 0; i < NumThreads; ++i)
    {
        ThreadStartInfo* startInfo = new (std::nothrow) ThreadStartInfo;
        if (!startInfo)
        {
            LogError(L"Failed to allocate thread start info.");
            return false;
        }

        startInfo->This = this;
        startInfo->ThreadIndex = i;
        Threads[i] = CreateThread(nullptr, 0, RenderThreadProc, startInfo, 0, nullptr);
        if (!Threads[i])
        {
            LogError(L"Failed to create thread.");
            delete startInfo;
            return false;
        }
    }

    return true;
}

void Raytracer::Clear()
{
    // Clear out the buffer
//    ZeroMemory(Pixels, Width * Height * sizeof(uint32_t));
    ZeroMemory(Accum, Width * Height * sizeof(XMFLOAT4));
}

bool Raytracer::Present()
{
    // Resolve Accum buffer
    for (int i = 0; i < Width * Height; ++i)
    {
        XMVECTOR color = XMLoadFloat4(&Accum[i]) / Accum[i].w;
        Pixels[i] = ConvertColorToUint(color);
    }

    HDC hdc = GetDC(Window);
    if (!hdc)
    {
        LogError(L"Failed to get DC.");
        return false;
    }

    RECT clientRect = {};
    GetClientRect(Window, &clientRect);

    int winWidth = clientRect.right - clientRect.left;
    int winHeight = clientRect.bottom - clientRect.top;

    // If the window has changed to a different size, use a stretch blt
    if (winWidth != Width || winHeight != Height)
    {
        if (!StretchBlt(hdc, 0, 0, winWidth, winHeight, BackBufferDC, 0, 0, Width, Height, SRCCOPY))
        {
            LogError(L"Failed to blt render target to display.");
            ReleaseDC(Window, hdc);
            return false;
        }
    }
    else
    {
        if (!BitBlt(hdc, 0, 0, Width, Height, BackBufferDC, 0, 0, SRCCOPY))
        {
            LogError(L"Failed to blt render target to display.");
            ReleaseDC(Window, hdc);
            return false;
        }
    }

    ReleaseDC(Window, hdc);
    return true;
}

static int AddQuad(
    FXMVECTOR a, FXMVECTOR b,
    FXMVECTOR c, FXMVECTOR d,
    XMFLOAT3* vertices, XMFLOAT2* texCoords)
{
    XMStoreFloat3(&vertices[0], a);
    XMStoreFloat3(&vertices[1], b);
    XMStoreFloat3(&vertices[2], c);
    XMStoreFloat3(&vertices[3], a);
    XMStoreFloat3(&vertices[4], c);
    XMStoreFloat3(&vertices[5], d);

    texCoords[0] = XMFLOAT2(0, 0);
    texCoords[1] = XMFLOAT2(1, 0);
    texCoords[2] = XMFLOAT2(1, 1);
    texCoords[3] = XMFLOAT2(0, 0);
    texCoords[4] = XMFLOAT2(1, 1);
    texCoords[5] = XMFLOAT2(0, 1);

    return 6;
}

// Expressed as a point and 3 scaled vectors defining spans from that point.
static int AddCube(
    FXMVECTOR p, FXMVECTOR u,
    FXMVECTOR v, FXMVECTOR w,
    XMFLOAT3* vertices, XMFLOAT2* texCoords)
{
    int numVertices = 0;

    // Front
    numVertices += AddQuad(p, p + u, p + u + v, p + v, vertices + numVertices, texCoords + numVertices);

    // Back
    numVertices += AddQuad(p + u + w, p + w, p + w + v, p + u + v + w, vertices + numVertices, texCoords + numVertices);

    // Right
    numVertices += AddQuad(p + u, p + u + w, p + u + w + v, p + u + v, vertices + numVertices, texCoords + numVertices);

    // Left
    numVertices += AddQuad(p + w, p, p + v, p + w + v, vertices + numVertices, texCoords + numVertices);

    // Top
    numVertices += AddQuad(p, p + w, p + w + u, p + u, vertices + numVertices, texCoords + numVertices);

    // Bottom
    numVertices += AddQuad(p + v, p + v + u, p + v + u + w, p + v + w, vertices + numVertices, texCoords + numVertices);

    return numVertices;
}

bool Raytracer::GenerateTestScene()
{
    // Load sample texture
    NumTextures = 1;
    Textures = new (std::nothrow) Texture[NumTextures];
    if (!Textures)
    {
        LogError(L"Failed to allocate textures.");
        return false;
    }

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
    {
        LogError(L"Failed to init COM.");
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to create WIC imaging factory.");
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(L"brick.jpg", nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to create WIC decoder.");
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> sourceBitmap;
    hr = decoder->GetFrame(0, &sourceBitmap);
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to get WIC frame.");
        return false;
    }

    ComPtr<IWICBitmapSource> destBitmap;
    hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, sourceBitmap.Get(), &destBitmap);
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to convert bitmap.");
        return false;
    }

    UINT width, height;
    destBitmap->GetSize(&width, &height);
    Textures[0].Width = width;
    Textures[0].Height = height;
    Textures[0].Pixels = new (std::nothrow) uint32_t[width * height];
    if (!Textures[0].Pixels)
    {
        CoUninitialize();
        LogError(L"Failed to allocate texture.");
        return false;
    }

    hr = destBitmap->CopyPixels(nullptr, width * sizeof(uint32_t), width * height * sizeof(uint32_t), (BYTE*)Textures[0].Pixels);
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to extract image.");
        return false;
    }

    CoUninitialize();

    //
    // Create Cornell box test scene. 5 walls (6 verts each), 2 cubes (36 verts each)
    //
    NumVertices = 114;
    NumTriangles = NumVertices / 3;

    Vertices = new (std::nothrow) XMFLOAT3[NumVertices];
    if (!Vertices)
    {
        LogError(L"Failed to allocate vertices for scene.");
        return false;
    }

    TexCoords = new (std::nothrow) XMFLOAT2[NumVertices];
    if (!TexCoords)
    {
        LogError(L"Failed to allocate texture coords for scene.");
        return false;
    }

    SurfaceProps = new (std::nothrow) SurfaceProp[NumTriangles];
    if (!SurfaceProps)
    {
        LogError(L"Failed to create surface properties for scene.");
        return false;
    }
    for (int i = 0; i < NumTriangles; ++i)
    {
        SurfaceProps[i].Texture = -1;
        SurfaceProps[i].Emission = XMFLOAT3(0.f, 0.f, 0.f);
    }

    int numVerts = 0;
    int numTris = 0;

    // left red wall
    numVerts += AddQuad(XMVectorSet(-2.5f, 2.5f, 0.f, 1.f), XMVectorSet(-2.5f, 2.5f, 5.f, 1.f),
        XMVectorSet(-2.5f, -2.5f, 5.f, 1.f), XMVectorSet(-2.5f, -2.5f, 0.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 1.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 0.f, 0.f);
        ++numTris;
    }

    // right green wall
    numVerts += AddQuad(XMVectorSet(2.5f, 2.5f, 5.f, 1.f), XMVectorSet(2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 0.f, 1.f), XMVectorSet(2.5f, -2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(0.f, 1.f, 0.f);
        ++numTris;
    }

    // back white wall
    numVerts += AddQuad(XMVectorSet(-2.5f, 2.5f, 5.f, 1.f), XMVectorSet(2.5f, 2.5f, 5.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 5.f, 1.f), XMVectorSet(-2.5f, -2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // front white wall
    numVerts += AddQuad(XMVectorSet(2.5f, 2.5f, 0.f, 1.f), XMVectorSet(-2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(-2.5f, -2.5f, 0.f, 1.f), XMVectorSet(2.5f, -2.5f, 0.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // bottom white floor
    numVerts += AddQuad(XMVectorSet(-2.5f, -2.5f, 5.f, 1.f), XMVectorSet(2.5f, -2.5f, 5.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 0.f, 1.f), XMVectorSet(-2.5f, -2.5f, 0.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // top white ceiling
    numVerts += AddQuad(XMVectorSet(-2.5f, 2.5f, 0.f, 1.f), XMVectorSet(2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(2.5f, 2.5f, 5.f, 1.f), XMVectorSet(-2.5f, 2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // top ceiling light
    numVerts += AddQuad(XMVectorSet(-0.5f, 2.495f, 1.5f, 1.f), XMVectorSet(0.5f, 2.495f, 1.5f, 1.f),
        XMVectorSet(0.5f, 2.495f, 3.5f, 1.f), XMVectorSet(-0.5f, 2.495f, 3.5f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        SurfaceProps[numTris].Emission = XMFLOAT3(1.f, 0.836f, 0.664f); // warm room light
        XMStoreFloat3(&SurfaceProps[numTris].Emission, XMLoadFloat3(&SurfaceProps[numTris].Emission) * 1.5f); // Pump up the light
        ++numTris;
    }

    // tall box in the back, rotated facing 1, 0, -1
    numVerts += AddCube(XMVectorSet(-1.2f, 0.5f, 2.25f, 1.f), XMVectorSet(1.5f, 0.f, 0.4f, 1.f),
        XMVectorSet(0.f, -3.f, 0.f, 1.f), XMVectorSet(-0.4f, 0.f, 1.5f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 12; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.3f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        SurfaceProps[numTris].Texture = 0;
        ++numTris;
    }

    // small box in the front, rotated facing -1, 0, 1
    numVerts += AddCube(XMVectorSet(0.f, -1.0f, 0.5f, 1.f), XMVectorSet(1.25f, 0.f, -0.4f, 1.f),
        XMVectorSet(0.f, -1.5f, 0.f, 1.f), XMVectorSet(0.4f, 0.f, 1.25f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 12; ++i)
    {
        SurfaceProps[numTris].Reflectiveness = 0.f;
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    assert(numVerts == NumVertices);
    assert(numTris == NumTriangles);

    return true;
}

DWORD CALLBACK Raytracer::RenderThreadProc(PVOID data)
{
    ThreadStartInfo* startInfo = (ThreadStartInfo*)data;

    HANDLE handles[] = { startInfo->This->ShutdownEvent, startInfo->This->StartEvent };
    for (;;)
    {
        if (WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE) == WAIT_OBJECT_0)
        {
            // shutdown
            break;
        }

        // Grab a tile
        long jobIndex = InterlockedDecrement(&startInfo->This->NumRenderJobs);
        if (jobIndex < 0)
        {
            // All the jobs are gone, nothing to do
            continue;
        }

        startInfo->This->ProcessRenderJob(jobIndex);

        if (jobIndex == 0)
        {
            // Was the last job, signal finish
            SetEvent(startInfo->This->FinishEvent);
        }
    }

    delete startInfo;
    return 0;
}

void Raytracer::ProcessRenderJob(int index)
{
    RenderRequest& request = RenderJobs[index];

    int endX = min(request.maxX, Width);
    int endY = min(request.maxY, Height);

    XMMATRIX cameraWorldTransform = XMLoadFloat4x4(&request.CameraWorld);

    for (int y = request.minY; y < endY; ++y)
    {
        for (int x = request.minX; x < endX; ++x)
        {
            // Compute ray direction
            XMVECTOR dir = XMVectorScale(cameraWorldTransform.r[2], DistToProjPlane);
            dir = XMVectorAdd(dir, XMVectorScale(cameraWorldTransform.r[0], (float)x - HalfWidth));
            dir = XMVectorAdd(dir, XMVectorScale(cameraWorldTransform.r[1], HalfHeight - (float)y));
            dir = XMVector3Normalize(dir);

            RayIntersection intersection;
            if (TraceRay(cameraWorldTransform.r[3], dir, &intersection))
            {
                XMVECTOR newSample = ShadePoint(dir, intersection);
                if (XMVectorGetX(XMVector3LengthEst(newSample)) > 0.0001f)
                {
                    newSample = XMVectorSetW(newSample, 1.f);
                    XMVECTOR curSample = XMLoadFloat4(&Accum[y * Width + x]);
                    XMStoreFloat4(&Accum[y * Width + x], curSample + newSample);
                }
            }
        }
    }
}

XMVECTOR Raytracer::PickRandomVectorInHemisphere(FXMVECTOR normal)
{
    // TODO: Use BRDF to drive distribution
    XMVECTOR tangent = XMVector3Cross(normal, XMVectorSet(0, 1, 0, 0));
    if (XMVectorGetX(XMVector3LengthEst(tangent)) < 0.0001f)
    {
        tangent = XMVector3Cross(normal, XMVectorSet(1, 0, 0, 0));
    }
    XMVECTOR bitangent = XMVector3Cross(tangent, normal);
    XMVECTOR newDir = 0.75f * normal + (rand() / (float)RAND_MAX * 2.f - 1.f) * tangent +
        (rand() / (float)RAND_MAX * 2.f - 1.f) * bitangent;

    return XMVector3Normalize(newDir);
}

XMVECTOR Raytracer::ShadePoint(FXMVECTOR dir, const RayIntersection& intersection, int depth)
{
    if (depth == NumBounces)
    {
        return XMVectorZero();
    }

    UNREFERENCED_PARAMETER(dir);

    // Compute base color
    const SurfaceProp& props = SurfaceProps[intersection.StartIndex / 3];
    XMVECTOR baseColor = XMLoadFloat3(&props.Color);

    if (props.Texture >= 0)
    {
        XMVECTOR t0 = XMLoadFloat2(&TexCoords[intersection.StartIndex]) * intersection.wA;
        XMVECTOR t1 = XMLoadFloat2(&TexCoords[intersection.StartIndex + 1]) * intersection.wB;
        XMVECTOR t2 = XMLoadFloat2(&TexCoords[intersection.StartIndex + 2]) * intersection.wC;
        XMVECTOR texCoords = t0 + t1 + t2;

        Texture& tex = Textures[props.Texture];
        uint32_t sample = tex.Pixels[(int)(XMVectorGetY(texCoords) * tex.Height) * tex.Width + (int)(XMVectorGetX(texCoords) * tex.Width)];

        baseColor = XMVectorSet(((sample >> 16) & 0xFF) / 255.f, ((sample >> 8) & 0xFF) / 255.f, (sample & 0xFF) / 255.f, 0.f);
    }

    // Basic info about the point we're shading
    XMVECTOR p = XMLoadFloat3(&intersection.Point);
    XMVECTOR normal = XMLoadFloat3(&intersection.Normal);

    // Move p out slight from surface to avoid self-intersection
    p += normal * 0.001f;

    XMVECTOR emission = XMLoadFloat3(&props.Emission);

    // Try up to 5 times to pick a direction that actually contributes light
    for (int iTry = 0; iTry < 5; ++iTry)
    {
        // Pick a random direction to bounce and compute contribution from that direction
        XMVECTOR newDir = PickRandomVectorInHemisphere(normal);

        RayIntersection test;
        if (TraceRay(p, newDir, &test))
        {
            XMVECTOR irradiance = ShadePoint(newDir, test, depth + 1);
            irradiance = irradiance * XMVectorGetX(XMVector3Dot(-newDir, XMLoadFloat3(&test.Normal)));
            float nDotL = XMVectorGetX(XMVector3Dot(newDir, normal));
            return emission + baseColor * (irradiance * nDotL);
        }
        else if (XMVectorGetX(XMVector3LengthEst(emission)) > 0.0001f)
        {
            // If it has emission, just use that instead of trying 5 times
            break;
        }
    }

    return emission;
}

uint32_t Raytracer::ConvertColorToUint(FXMVECTOR color)
{
    return 0xFF000000 |
        (min(255, (int)(XMVectorGetX(color) * 255.0f)) << 16) |
        (min(255, (int)(XMVectorGetY(color) * 255.0f)) << 8) |
        min(255, (int)(XMVectorGetZ(color) * 255.0f));
}


bool Raytracer::TraceRay(FXMVECTOR start, FXMVECTOR dir, RayIntersection* intersection)
{
    // TODO: Improve with some sort of spatial data structure (eg. kd tree)
    bool hitSomething = false;
    int numTriangles = NumVertices / 3;
    float nearest = FLT_MAX;
    RayIntersection test;

    for (int i = 0; i < numTriangles; ++i)
    {
        if (RayTriangleIntersect(start, dir, i * 3, &test))
        {
            // If not asking for intersection info, then first hit is
            // enough to know we hit something, return
            if (!intersection)
            {
                return true;
            }

            if (test.Dist < nearest)
            {
                nearest = test.Dist;
                *intersection = test;
            }
            hitSomething = true;
        }
    }

    return hitSomething;
}

bool Raytracer::RayTriangleIntersect(FXMVECTOR start, FXMVECTOR dir, int startVertex, /* optional */ RayIntersection* intersection)
{
    XMVECTOR a = XMLoadFloat3(&Vertices[startVertex]);
    XMVECTOR b = XMLoadFloat3(&Vertices[startVertex + 1]);
    XMVECTOR c = XMLoadFloat3(&Vertices[startVertex + 2]);
    XMVECTOR ab = XMVectorSubtract(b, a);
    XMVECTOR ac = XMVectorSubtract(c, a);

    // TODO: Check for degenerecy. Also, note that n is NOT normalized
    XMVECTOR n = XMVector3Cross(ab, ac);

    // Less than 0 means behind the triangle
    float dist = XMVectorGetX(XMVector3Dot(XMVectorSubtract(start, a), n));
    if (dist <= 0.f)
    {
        return false;
    }

    // Less than 0 means dir is moving away from triangle face.
    float cosA = XMVectorGetX(XMVector3Dot(XMVectorNegate(n), dir));
    if (cosA <= 0.f)
    {
        return false;
    }

#if 0
    //
    // Tetrahedron-based boolean ray/triangle collision test.
    // Doesn't compute contact point or barycentric coordinates for interpolating.
    //

    // Form 3 new faces of tetrahedron by combining start with each edge of triangle.
    // For each face, we just need it's outward facing normal. Note that we don't
    // need the normals to be normalized, just the direction is enough.
    XMVECTOR sa = XMVectorSubtract(a, start);
    XMVECTOR sb = XMVectorSubtract(b, start);
    XMVECTOR sc = XMVectorSubtract(c, start);
    XMVECTOR n0 = XMVector3Cross(sa, sb);
    XMVECTOR n1 = XMVector3Cross(sb, sc);
    XMVECTOR n2 = XMVector3Cross(sc, sa);

    // If the direction vector hits the triangle, then it must point into
    // the halfspace regions within pseudo-tetrahedron (inside the 3 normals above)
    if (XMVectorGetX(XMVector3Dot(dir, n0)) > 0)
    {
        // Outside of tetrahedron
        return false;
    }

    if (XMVectorGetX(XMVector3Dot(dir, n1)) > 0)
    {
        // Outside of tetrahedron
        return false;
    }

    if (XMVectorGetX(XMVector3Dot(dir, n2)) > 0)
    {
        // Outside of tetrahedron
        return false;
    }

    // ray intersects triangle somewhere
    return true;

#else
    //
    // Standard barycentric approach: Find the intersection, P, with plane of triangle
    // then compute barycentric coordinates of the triangle. If they are all positive
    // signed areas (don't need to divide all the way through to get normalized barycentrics)
    // then the point is inside of the triangle.
    //

    // Find intersection point, p, with plane
    float hyp = dist / cosA;
    XMVECTOR p = XMVectorAdd(start, XMVectorScale(dir, hyp));

    // Find barycentric coordinates of P (wA, wB, wC), however
    // due to not dividing through by n, the weights are scaled by
    // 2*TriArea, but that's okay for the purpose of determining inside
    // triangle or not. Once we want to use for interpolating, we either need
    // to scale all vertex attributes by 1/2*TriArea, or divide through here
    XMVECTOR bc = XMVectorSubtract(c, b);
    XMVECTOR ap = XMVectorSubtract(p, a);
    XMVECTOR bp = XMVectorSubtract(p, b);

    XMVECTOR wC = XMVector3Cross(ab, ap);
    if (XMVectorGetX(XMVector3Dot(wC, n)) < 0.f)
    {
        return false;
    }

    XMVECTOR wB = XMVector3Cross(ap, ac);
    if (XMVectorGetX(XMVector3Dot(wB, n)) < 0.f)
    {
        return false;
    }

    XMVECTOR wA = XMVector3Cross(bc, bp);
    if (XMVectorGetX(XMVector3Dot(wA, n)) < 0.f)
    {
        return false;
    }

    if (intersection)
    {
        float invNLen = 1.0f / XMVectorGetX(XMVector3Length(n));
        assert(!_isnanf(invNLen));
        intersection->Type = RayIntersection::IntersectionType::Triangle;
        intersection->Dist = hyp;
        XMStoreFloat3(&intersection->Point, p);
        XMStoreFloat3(&intersection->Normal, XMVector3Normalize(n));
        intersection->StartIndex = startVertex;
        intersection->wA = XMVectorGetX(XMVector3Length(wA)) * invNLen;
        intersection->wB = XMVectorGetX(XMVector3Length(wB)) * invNLen;
        intersection->wC = XMVectorGetX(XMVector3Length(wC)) * invNLen;
    }

    return true;
#endif
}
