#include "Precomp.h"
#include "Debug.h"
#include "Renderer.h"

// Constants
static const wchar_t ClassName[] = L"Light Field Rendering Test Application";
static const uint32_t ScreenWidth = 1280;
static const uint32_t ScreenHeight = 720;
static bool VSyncEnabled = true;

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

    std::unique_ptr<Renderer> renderer(Renderer::Create(Window));
    if (!renderer)
    {
        assert(false);
        return -2;
    }

    ShowWindow(Window, SW_SHOW);
    UpdateWindow(Window);

    // Create a simple light field scene
    LightField lightField;
    if (!renderer->CreateSimpleOutsideInLightField(&lightField))
    {
        assert(false);
        return -3;
    }

    // Set the scene as the active scene to render
    renderer->SetLightField(lightField);

    // Timing info
    LARGE_INTEGER lastTime = {};
    LARGE_INTEGER currTime = {};
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    // Camera info
    XMVECTOR cameraPosition = XMVectorSet(0.f, 1.f, -5.f, 1.f);
    XMVECTOR cameraForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    XMVECTOR cameraUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    XMVECTOR right = XMVector3Cross(cameraUp, cameraForward);
    XMVECTOR movement;
    float yaw;
    float pitch;
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), ScreenWidth / (float)ScreenHeight, 0.05f, 100.f);

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

            movement = XMVectorZero();
            yaw = 0.f;
            pitch = 0.f;

            // Input
            if (GetAsyncKeyState('D') & 0x8000)
            {
                movement += right;
            }
            if (GetAsyncKeyState('A') & 0x8000)
            {
                movement -= right;
            }
            if (GetAsyncKeyState('W') & 0x8000)
            {
                movement += cameraForward;
            }
            if (GetAsyncKeyState('S') & 0x8000)
            {
                movement -= cameraForward;
            }

            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                yaw -= 0.05f;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                yaw += 0.05f;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000)
            {
                movement += cameraUp;
                //pitch -= 0.05f;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            {
                movement -= cameraUp;
                //pitch += 0.05f;
            }

            movement = XMVector3Normalize(movement) * 0.125f;
            cameraPosition += movement;

            // Keep the camera at least 4 units from the center to avoid clipping into the light field. Only in xz plane
            float y = XMVectorGetY(cameraPosition);
            cameraPosition = XMVectorSetY(cameraPosition, 0.f);
            if (XMVectorGetX(XMVector3LengthEst(cameraPosition)) < 4.5f)
            {
                cameraPosition = XMVector3Normalize(cameraPosition) * 4.5f;
            }
            cameraPosition = XMVectorSetY(cameraPosition, y);

            cameraForward = XMVector3Normalize(XMVector3TransformNormal(cameraForward, XMMatrixRotationY(yaw)));
            cameraUp = XMVector3Normalize(XMVector3TransformNormal(cameraUp, XMMatrixRotationX(pitch)));

            // Orthonormalize
            right = XMVector3Cross(cameraUp, cameraForward);
            cameraForward = XMVector3Cross(right, cameraUp);
            cameraUp = XMVector3Cross(cameraForward, right);

            if (!renderer->Render(XMMatrixLookToLH(cameraPosition, cameraForward, cameraUp), projection, VSyncEnabled))
            {
                assert(false);
            }

            swprintf_s(caption, L"Light Field Renderer: Resolution: %dx%d, FPS: %3.2f", ScreenWidth, ScreenHeight, frameRate);
            SetWindowText(Window, caption);
        }
    }

    renderer.reset();
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
