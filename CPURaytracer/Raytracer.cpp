#include "Precomp.h"
#include "Raytracer.h"
#include "Debug.h"
#include <time.h>

#if defined (_DEBUG)
//#define DISABLE_MULTITHREADED_RENDERING
#endif

//#define USE_SINGLE_BOX

Raytracer* Raytracer::Create(HWND window)
{
    srand((int)time(nullptr));

    Raytracer* raytracer = new Raytracer(window);
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
    , hFov(0.f)
    , DistToProjPlane(0.f)
    , NumVertices(0)
    , NumTriangles(0)
    , NumRenderJobs(0)
    , NumThreads(0)
    , NumTextures(0)
    , BlurEnabled(true)
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
    if (ShutdownEvent.IsValid())
    {
        SetEvent(ShutdownEvent.Get());
        WaitForMultipleObjects(NumThreads, Threads.get(), TRUE, INFINITE);
    }

    for (int i = 0; i < NumThreads; ++i)
    {
        if (Threads[i])
        {
            CloseHandle(Threads[i]);
        }
    }

    Pixels = nullptr;

    if (BackBufferDC)
    {
        DeleteDC(BackBufferDC);
        BackBufferDC = nullptr;
    }
}

void Raytracer::SetFOV(float horizFovRadians)
{
    hFov = horizFovRadians;
    float denom = tanf(hFov * 0.5f);
    assert(!isnan(denom) && fabsf(denom) > 0.00001f);
    DistToProjPlane = (Width * 0.5f) / denom;
}

void Raytracer::Clear()
{
    // Clear out the buffer
    ZeroMemory(Accum.get(), Width * Height * sizeof(XMFLOAT4));
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
    SetEvent(StartEvent.Get());
    WaitForSingleObject(FinishEvent.Get(), INFINITE);
    ResetEvent(StartEvent.Get());

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
    Accum.reset(new XMFLOAT4[Width * Height]);
    if (!Accum)
    {
        LogError(L"Failed to allocate accumulation buffer.");
        return false;
    }
    Clear();

    if (!GenerateTestScene())
    {
        LogError(L"Failed to create test scene.");
        return false;
    }

    //
    // Create render threads
    //

    RenderJobs.reset(new RenderRequest[(Width / TileSize) * (Height / TileSize)]);
    if (!RenderJobs)
    {
        LogError(L"Failed to allocate render job list.");
        return false;
    }

    StartEvent.Attach(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    if (!StartEvent.IsValid())
    {
        LogError(L"Failed to create start event.");
        return false;
    }

    FinishEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (!FinishEvent.IsValid())
    {
        LogError(L"Failed to create finish event.");
        return false;
    }

    ShutdownEvent.Attach(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    if (!ShutdownEvent.IsValid())
    {
        LogError(L"Failed to create shutdown event.");
        return false;
    }

    // Determine how many processors/cores the machine has
#if !defined(DISABLE_MULTITHREADED_RENDERING)
    // Since our rendering doesn't stall on I/O other than memory loads, more
    // threads than cores won't help.
    NumThreads = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    NumThreads = 1;
#endif

    Threads.reset(new HANDLE[NumThreads]);
    if (!Threads)
    {
        LogError(L"Failed to allocate thread list.");
        return false;
    }

    for (int i = 0; i < NumThreads; ++i)
    {
        Threads[i] = CreateThread(nullptr, 0, RenderThreadProc, this, 0, nullptr);
        if (!Threads[i])
        {
            LogError(L"Failed to create thread.");
            return false;
        }
    }

    return true;
}

bool Raytracer::Present()
{
    // Resolve Accum buffer
    for (int y = 0; y < Height; ++y)
    {
        for (int x = 0; x < Width; ++x)
        {
            int i = y * Width + x;
            XMVECTOR color = XMLoadFloat4(&Accum[i]) / max(Accum[i].w, 1.f);

            if (BlurEnabled)
            {
                int iRight = y * Width + min(x + 1, Width - 1);
                int iBottom = min(y + 1, Height - 1) * Width + x;
                int iRightBottom = min(y + 1, Height - 1) * Width + min(x + 1, Width - 1);
                XMVECTOR right = XMLoadFloat4(&Accum[iRight]) / max(Accum[iRight].w, 1.f);
                XMVECTOR bottom = XMLoadFloat4(&Accum[iBottom]) / max(Accum[iBottom].w, 1.f);
                XMVECTOR rightBottom = XMLoadFloat4(&Accum[iRightBottom]) / max(Accum[iRightBottom].w, 1.f);
                color = (color + right + bottom + rightBottom) * 0.25f;
            }

            Pixels[i] = ConvertColorToUint(color);
        }
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

    // Bottom (leave off since they're never visible in our test scene)
    //numVertices += AddQuad(p + v, p + v + u, p + v + u + w, p + v + w, vertices + numVertices, texCoords + numVertices);

    return numVertices;
}

bool Raytracer::GenerateTestScene()
{
    // Load sample texture
    NumTextures = 1;
    Textures.reset(new Texture[NumTextures]);
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
    Textures[0].Pixels.reset(new uint32_t[width * height]);
    if (!Textures[0].Pixels)
    {
        CoUninitialize();
        LogError(L"Failed to allocate texture.");
        return false;
    }

    hr = destBitmap->CopyPixels(nullptr, width * sizeof(uint32_t), width * height * sizeof(uint32_t), (BYTE*)Textures[0].Pixels.get());
    if (FAILED(hr))
    {
        CoUninitialize();
        LogError(L"Failed to extract image.");
        return false;
    }

    CoUninitialize();

    //
    // Create Cornell box test scene. 6 walls (6 verts each), 2 cubes (30 verts each)
    //

#if defined(USE_SINGLE_BOX)
    NumVertices = 72;
#else
    NumVertices = 102;
#endif
    NumTriangles = NumVertices / 3;

    Vertices.reset(new XMFLOAT3[NumVertices]);
    if (!Vertices)
    {
        LogError(L"Failed to allocate vertices for scene.");
        return false;
    }

    TexCoords.reset(new XMFLOAT2[NumVertices]);
    if (!TexCoords)
    {
        LogError(L"Failed to allocate texture coords for scene.");
        return false;
    }

    SurfaceProps.reset(new SurfaceProp[NumTriangles]);
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
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 0.f, 0.f);
        ++numTris;
    }

    // right green wall
    numVerts += AddQuad(XMVectorSet(2.5f, 2.5f, 5.f, 1.f), XMVectorSet(2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 0.f, 1.f), XMVectorSet(2.5f, -2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(0.f, 1.f, 0.f);
        ++numTris;
    }

    // back white wall
    numVerts += AddQuad(XMVectorSet(-2.5f, 2.5f, 5.f, 1.f), XMVectorSet(2.5f, 2.5f, 5.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 5.f, 1.f), XMVectorSet(-2.5f, -2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // front white wall
    numVerts += AddQuad(XMVectorSet(2.5f, 2.5f, 0.f, 1.f), XMVectorSet(-2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(-2.5f, -2.5f, 0.f, 1.f), XMVectorSet(2.5f, -2.5f, 0.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // bottom white floor
    numVerts += AddQuad(XMVectorSet(-2.5f, -2.5f, 5.f, 1.f), XMVectorSet(2.5f, -2.5f, 5.f, 1.f),
        XMVectorSet(2.5f, -2.5f, 0.f, 1.f), XMVectorSet(-2.5f, -2.5f, 0.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // top white ceiling
    numVerts += AddQuad(XMVectorSet(-2.5f, 2.5f, 0.f, 1.f), XMVectorSet(2.5f, 2.5f, 0.f, 1.f),
        XMVectorSet(2.5f, 2.5f, 5.f, 1.f), XMVectorSet(-2.5f, 2.5f, 5.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

    // top ceiling light
    numVerts += AddQuad(XMVectorSet(-0.75f, 2.495f, 1.75f, 1.f), XMVectorSet(0.75f, 2.495f, 1.75f, 1.f),
        XMVectorSet(0.75f, 2.495f, 3.25f, 1.f), XMVectorSet(-0.75f, 2.495f, 3.25f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 2; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        SurfaceProps[numTris].Emission = XMFLOAT3(1.f, 0.836f, 0.664f); // warm room light
        ++numTris;
    }

#if defined (USE_SINGLE_BOX)

    // tall box in the back, rotated facing 1, 0, -1
    numVerts += AddCube(XMVectorSet(0.f, -1.f, 1.f, 1.f), XMVectorSet(1.f, 0.f, 1.f, 1.f),
        XMVectorSet(0.f, -1.5f, 0.f, 1.f), XMVectorSet(-1.f, 0.f, 1.f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 10; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

#else

    // tall box in the back, rotated facing 1, 0, -1
    numVerts += AddCube(XMVectorSet(-1.2f, 0.5f, 2.25f, 1.f), XMVectorSet(1.5f, 0.f, 0.8f, 1.f),
        XMVectorSet(0.f, -3.f, 0.f, 1.f), XMVectorSet(-0.8f, 0.f, 1.5f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 10; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        //SurfaceProps[numTris].Texture = 0;
        ++numTris;
    }

    // small box in the front, rotated facing -1, 0, 1
    numVerts += AddCube(XMVectorSet(-0.5f, -1.0f, 1.f, 1.f), XMVectorSet(1.5f, 0.f, -0.8f, 1.f),
        XMVectorSet(0.f, -1.5f, 0.f, 1.f), XMVectorSet(0.8f, 0.f, 1.5f, 1.f),
        &Vertices[numVerts], &TexCoords[numVerts]);

    for (int i = 0; i < 10; ++i)
    {
        SurfaceProps[numTris].Color = XMFLOAT3(1.f, 1.f, 1.f);
        ++numTris;
    }

#endif

    assert(numVerts == NumVertices);
    assert(numTris == NumTriangles);

    return true;
}

DWORD CALLBACK Raytracer::RenderThreadProc(PVOID data)
{
    Raytracer* This = (Raytracer*)data;

    HANDLE handles[] = { This->ShutdownEvent.Get(), This->StartEvent.Get() };
    for (;;)
    {
        if (WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE) == WAIT_OBJECT_0)
        {
            // shutdown
            break;
        }

        // Grab a tile
        long jobIndex = InterlockedDecrement(&This->NumRenderJobs);
        if (jobIndex < 0)
        {
            // All the jobs are gone, nothing to do
            continue;
        }

        This->ProcessRenderJob(jobIndex);

        if (jobIndex == 0)
        {
            // Was the last job, signal finish
            SetEvent(This->FinishEvent.Get());
        }
    }

    return 0;
}

void Raytracer::ProcessRenderJob(long index)
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
                XMVECTOR newSample = ComputeRadiance(dir, intersection);
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

// Get random normalized float [-1, 1]
static float randf()
{
    return (float)rand() / (float)RAND_MAX * 2.f - 1.f;
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
    XMVECTOR newDir = (normal * fabsf(randf())) + (tangent * randf() * randf()) + (bitangent * randf() * randf());

    return XMVector3Normalize(newDir);
}

XMVECTOR Raytracer::ComputeRadiance(FXMVECTOR dir, const RayIntersection& intersection, int depth)
{
    if (depth == NumBounces)
    {
        return XMVectorZero();
    }

    // dir is useful for computing specular or other view dependent factors. Not currently used
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

    // Try up to 4 times to see if we can get a valid sample
    int numTries = 4;

    // If surface has emission (gives off light), then no need to try multiple times
    if (XMVectorGetX(XMVector3LengthEst(emission)) > 0.0001f)
    {
        numTries = 1;
    }

    for (int i = 0; i < numTries; ++i)
    {
        // Pick a random direction to bounce and compute contribution from that direction
        XMVECTOR newDir = PickRandomVectorInHemisphere(normal);

        RayIntersection test;
        if (TraceRay(p, newDir, &test))
        {
            XMVECTOR radiance = ComputeRadiance(newDir, test, depth + 1);
            radiance = radiance * XMVectorGetX(XMVector3Dot(-newDir, XMLoadFloat3(&test.Normal)));

            float nDotL = XMVectorGetX(XMVector3Dot(newDir, normal));
            return emission + baseColor * (radiance * nDotL);
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
    // TODO: Accelerate with some sort of spatial data structure (eg. kd tree)
    bool hitSomething = false;
    int numTriangles = NumVertices / 3;
    float nearest = FLT_MAX;
    RayIntersection test;

    for (int i = 0; i < numTriangles; ++i)
    {
        if (RayTriangleIntersect(start, dir, i * 3, &test))
        {
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

bool Raytracer::RayTriangleIntersect(FXMVECTOR start, FXMVECTOR dir, int startVertex, RayIntersection* intersection)
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

    float invNLen = 1.0f / XMVectorGetX(XMVector3Length(n));
    assert(!_isnanf(invNLen));
    intersection->Dist = hyp;
    XMStoreFloat3(&intersection->Point, p);
    XMStoreFloat3(&intersection->Normal, XMVector3Normalize(n));
    intersection->StartIndex = startVertex;
    intersection->wA = XMVectorGetX(XMVector3Length(wA)) * invNLen;
    intersection->wB = XMVectorGetX(XMVector3Length(wB)) * invNLen;
    intersection->wC = XMVectorGetX(XMVector3Length(wC)) * invNLen;

    return true;
}
