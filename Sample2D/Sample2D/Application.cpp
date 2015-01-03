#include "Precomp.h"
#include "Application.h"
#include "SpriteBatch.h"
#include "SpriteBatch2.h"
#include "Texture.h"
#include "Game.h"

using namespace Microsoft::WRL;

//
// Main entrypoint
//
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        return hr;
    }

    // Can't allow exceptions to be thrown past WinMain
    try
    {
        std::unique_ptr<Application> application(new Application(1280, 720));

        application->Show(true);

        application->Run();
    }
    catch (const std::exception& e)
    {
        OutputDebugString(L"\n** UNHANDLED EXCEPTION **\n");
        OutputDebugStringA(e.what());
        OutputDebugString(L"\n****\n");

        DebugBreak();
    }

    CoUninitialize();

    return 0;
}

//
// Implementation of Application class
//
const wchar_t Application::WindowClassName[] = L"Sample2D";

Application::Application(int width, int height)
    : _window(nullptr)
{
    QueryPerformanceCounter(&_prevTime);
    QueryPerformanceFrequency(&_frequency);

    Initialize(width, height);
    InitializeGraphics(width, height);

    _game.reset(new Game(_spriteBatch));
}

Application::~Application()
{
    if (_window)
    {
        DestroyWindow(_window);
        _window = nullptr;
    }
}

void Application::Show(bool visible)
{
    ShowWindow(_window, visible ? SW_SHOW : SW_HIDE);
    UpdateWindow(_window);
}

void Application::Exit()
{
    // May not always be called from the same thread as window,
    // so post message to be safe (instead of using PostQuitMessage)
    PostMessage(_window, WM_QUIT, 0, 0);
}

void Application::Run()
{
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
            float elapsedSeconds = ComputeElapsedSeconds();
            Update(elapsedSeconds);
            Draw();
        }
    }
}

void Application::Initialize(int width, int height)
{
    WNDCLASSEX wcx = {};
    wcx.cbSize = sizeof(wcx);
    wcx.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcx.hInstance = GetModuleHandle(nullptr);
    wcx.lpfnWndProc = s_WindowProc;
    wcx.lpszClassName = WindowClassName;

    if (!RegisterClassEx(&wcx))
    {
        throw std::exception("Failed to register window class.");
    }

    // Style is a standard overlapped window, but removing the thick (resizeable) frame & maximize button
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    // Determine how big to make the window so that the client area is width x height
    RECT windowBounds = { 0, 0, width, height };
    AdjustWindowRect(&windowBounds, style, FALSE);

    _window = CreateWindow(WindowClassName, WindowClassName, style, CW_USEDEFAULT, CW_USEDEFAULT,
        windowBounds.right - windowBounds.left, windowBounds.bottom - windowBounds.top,
        nullptr, nullptr, wcx.hInstance, nullptr);

    if (!_window)
    {
        throw std::exception("Failed to create window.");
    }

    // Store a pointer to our class in the window itself
    SetWindowLongPtr(_window, GWLP_USERDATA, (LONG_PTR)this);
}

void Application::InitializeGraphics(int width, int height)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = _window;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_10_0;

    // Try and create a debug device. If that fails (maybe they don't have SDK installed), then fall back to non-debug device
    UINT flags = D3D11_CREATE_DEVICE_DEBUG;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &featureLevel, 1,
        D3D11_SDK_VERSION, &scd, &_swapChain, &_device, nullptr, &_context)))
    {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;

        CHECKHR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &featureLevel, 1,
            D3D11_SDK_VERSION, &scd, &_swapChain, &_device, nullptr, &_context));
    }

    ComPtr<ID3D11Texture2D> texture;
    CHECKHR(_swapChain->GetBuffer(0, IID_PPV_ARGS(&texture)));
    CHECKHR(_device->CreateRenderTargetView(texture.Get(), nullptr, &_backBufferRT));

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.Format = DXGI_FORMAT_D32_FLOAT;

    CHECKHR(_device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf()));
    CHECKHR(_device->CreateDepthStencilView(texture.Get(), nullptr, &_depthBuffer));

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MaxDepth = 1.0f;

    _context->RSSetViewports(1, &vp);
    _context->OMSetRenderTargets(1, _backBufferRT.GetAddressOf(), _depthBuffer.Get());

    _spriteBatch = std::make_shared<SpriteBatch2>(_device.Get());
}

float Application::ComputeElapsedSeconds()
{
#if defined(USE_FIXED_TIMESTEP)
    return 1.0f / 60.0f;
#else
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    float elapsedSeconds = (float)(now.QuadPart - _prevTime.QuadPart) / (float)_frequency.QuadPart;
    _prevTime = now;

    return elapsedSeconds;
#endif
}

void Application::Update(float elapsedSeconds)
{
    _game->Update(elapsedSeconds);
}

void Application::Draw()
{
    static float clearColor[] = { 0, 0, 0.25f, 1 };
    _context->ClearRenderTargetView(_backBufferRT.Get(), clearColor);
    _context->ClearDepthStencilView(_depthBuffer.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    _game->Draw();

    _swapChain->Present(1, 0);
}

LRESULT Application::WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        // If user clicked X to close window, exit application
        PostQuitMessage(0);
        break;
    }

    // If fell through to here, then forward to default message handler as well
    return DefWindowProc(_window, msg, wParam, lParam);
}

LRESULT Application::s_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Try and retrieve our Application pointer from the incoming window
    Application* app = (Application*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (app)
    {
        // Call our member message processing function
        return app->WindowProc(msg, wParam, lParam);
    }

    // Otherwise, just use default handler for messages
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
