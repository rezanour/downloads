#include "Precomp.h"
#include "Debug.h"
#include "Raytracer.h"

// Constants
static const wchar_t ClassName[] = L"GPU Raytracer Test Application";
static const uint32_t ScreenWidth = 640;
static const uint32_t ScreenHeight = 480;
static bool VSyncEnabled = false;

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
    cameraWorldTransform.r[3] = XMVectorSet(0.f, 0.f, -5.f, 1.f);

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

            lastTime = currTime;
            float frameRate = 1.0f / (float)timeStep;

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                // Exit
                break;
            }

            float moveForward = 0.f;
            float moveRight = 0.f;
            float rotateX = 0.f;
            // Input
            if (GetAsyncKeyState('D') & 0x8000)
            {
                moveRight += 0.125f;
            }
            if (GetAsyncKeyState('A') & 0x8000)
            {
                moveRight -= 0.125f;
            }
            if (GetAsyncKeyState('W') & 0x8000)
            {
                moveForward += 0.125f;
            }
            if (GetAsyncKeyState('S') & 0x8000)
            {
                moveForward -= 0.125f;
            }
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                rotateX -= 0.05f;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                rotateX += 0.05f;
            }

            if (fabsf(moveForward) > 0.001f || fabsf(moveRight) > 0.001f)
            {
                cameraWorldTransform.r[3] += cameraWorldTransform.r[0] * moveRight + cameraWorldTransform.r[2] * moveForward;
            }

            if (fabsf(rotateX) > 0.001f)
            {
                XMMATRIX rotate = XMMatrixRotationY(rotateX);
                XMMATRIX noTranslate = cameraWorldTransform;
                XMVECTOR translate = noTranslate.r[3];
                noTranslate.r[3] = XMVectorSet(0, 0, 0, 1);
                cameraWorldTransform = noTranslate * rotate;
                cameraWorldTransform.r[3] = translate;
            }

            raytracer->Render(cameraWorldTransform, VSyncEnabled);

            swprintf_s(caption, L"GPU Raytracer: Resolution: %dx%d, FPS: %3.2f", ScreenWidth, ScreenHeight, frameRate);
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
