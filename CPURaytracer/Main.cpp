#include "Precomp.h"
#include "Debug.h"
#include "Raytracer.h"
#include <memory>

// Constants
static const wchar_t ClassName[] = L"Raytracer Test Application";
static const uint32_t ScreenWidth = 640;
static const uint32_t ScreenHeight = 480;
// Max FPS (expressed as 1/FPS). Prevent app from running faster than this, set to 0.f for no throttle
static const float TargetFrameRate = 0.f;// 1.f / 60.f;

// Application variables
static HINSTANCE Instance;
static HWND Window;

// Local methods
static bool Initialize();
static void Shutdown();
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Entry point
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    Instance = instance;
    if (!Initialize())
    {
        assert(false);
        return -1;
    }

    std::unique_ptr<Raytracer> raytracer(Raytracer::Create(Window));
    if (!raytracer)
    {
        assert(false);
        return -2;
    }

    ShowWindow(Window, SW_SHOW);
    UpdateWindow(Window);

    // Timing info
    LARGE_INTEGER lastTime = {};
    LARGE_INTEGER currTime = {};
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    raytracer->SetFOV(XMConvertToRadians(60.f));

    // Camera at the origin, looking along Z
    XMMATRIX cameraWorldTransform = XMMatrixIdentity();
    // Move camera back along -Z so that it's looking at the origin
    cameraWorldTransform.r[3] = XMVectorSet(0.001f, 0, -5, 1);

    wchar_t caption[200] = {};

    // Main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Idle

            QueryPerformanceCounter(&currTime);

            if (lastTime.QuadPart == 0)
            {
                lastTime.QuadPart = currTime.QuadPart;
                continue;
            }

            double timeStep = (double)(currTime.QuadPart - lastTime.QuadPart) / (double)frequency.QuadPart;

            // Throttle app
            if (TargetFrameRate > 0.0001f && timeStep < TargetFrameRate)
            {
                continue;
            }

            lastTime = currTime;
            float frameRate = 1.0f / (float)timeStep;

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                // Exit
                break;
            }

            // Input
            bool invalidate = false;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                cameraWorldTransform.r[3] = XMVectorAdd(cameraWorldTransform.r[3], XMVectorSet(0.125f, 0.f, 0.f, 0.f));
                invalidate = true;
            }
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                cameraWorldTransform.r[3] = XMVectorAdd(cameraWorldTransform.r[3], XMVectorSet(-0.125f, 0.f, 0.f, 0.f));
                invalidate = true;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000)
            {
                cameraWorldTransform.r[3] = XMVectorAdd(cameraWorldTransform.r[3], XMVectorSet(0.f, 0.f, 0.125f, 0.f));
                invalidate = true;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            {
                cameraWorldTransform.r[3] = XMVectorAdd(cameraWorldTransform.r[3], XMVectorSet(0.f, 0.f, -0.125f, 0.f));
                invalidate = true;
            }

            if (invalidate)
            {
                raytracer->Invalidate();
            }
            raytracer->Render(cameraWorldTransform);

            swprintf_s(caption, L"CPU Raytracer: Resolution: %dx%d, Threads: %d, FPS: %3.2f", ScreenWidth, ScreenHeight, raytracer->GetNumThreads(), frameRate);
            SetWindowText(Window, caption);
        }
    }

    raytracer.reset();
    Shutdown();
    return 0;
}

bool Initialize()
{
    WNDCLASSEX wcx = {};
    wcx.cbSize = sizeof(wcx);
    wcx.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wcx.hInstance = Instance;
    wcx.lpfnWndProc = WndProc;
    wcx.lpszClassName = ClassName;

    if (!RegisterClassEx(&wcx))
    {
        LogError(L"Failed to initialize window class.");
        return false;
    }

    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    RECT rc = {};
    rc.right = ScreenWidth;
    rc.bottom = ScreenHeight;
    AdjustWindowRect(&rc, style, FALSE);

    Window = CreateWindow(ClassName, ClassName, style,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, Instance, nullptr);

    if (!Window)
    {
        LogError(L"Failed to create window.");
        return false;
    }

    return true;
}

void Shutdown()
{
    DestroyWindow(Window);
    Window = nullptr;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
