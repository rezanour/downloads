#include "Precomp.h"
#include "Raytracer.h"

static HWND Window;
static HINSTANCE Instance;
static uint32_t Width = 1920;
static uint32_t Height = 1080;
static const wchar_t ClassName[] = L"CSRaytracer";

static bool Initialize();
static LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    Instance = instance;

    if (!Initialize())
    {
        assert(false);
        return -1;
    }

    std::unique_ptr<D3D11CSRaytracer> raytracer(new D3D11CSRaytracer(Window));

    if (!raytracer->Initialize())
    {
        assert(false);
        return -2;
    }

    ShowWindow(Window, SW_SHOW);
    UpdateWindow(Window);

    XMMATRIX cameraWorld = XMMatrixIdentity();
    cameraWorld.r[3] = XMVectorSet(1.5, 1, -10, 1);

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            raytracer->Render(cameraWorld, XMConvertToRadians(60.0f));
        }
    }

    raytracer = nullptr;

    DestroyWindow(Window);

    return 0;
}

bool Initialize()
{
    WNDCLASSEX wcx{};
    wcx.cbSize = sizeof(wcx);
    wcx.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wcx.hInstance = Instance;
    wcx.lpfnWndProc = WinProc;
    wcx.lpszClassName = ClassName;

    if (!RegisterClassEx(&wcx))
    {
        OutputDebugString(L"Failed to register window class.\n");
        assert(false);
        return false;
    }

    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    RECT size{};
    size.right = Width;
    size.bottom = Height;
    AdjustWindowRect(&size, style, FALSE);

    Window = CreateWindow(ClassName, ClassName, style, CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left,
        size.bottom - size.top, nullptr, nullptr, Instance, nullptr);

    if (!Window)
    {
        OutputDebugString(L"Failed to create window.\n");
        assert(false);
        return false;
    }

    return true;
}

LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
