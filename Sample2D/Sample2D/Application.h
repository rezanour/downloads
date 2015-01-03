#pragma once

class SpriteBatch;
class SpriteBatch2;
class Texture;
class Game;

class Application
{
public:
    Application(int width, int height);
    ~Application();

    // Show or hide the window
    void Show(bool visible);

    // Request the application exit
    void Exit();

    // Runs message loop until application exits (blocking call)
    void Run();

private:
    // Prevent copy
    Application(const Application&);
    Application& operator= (const Application&);

    // Initialize application & window
    void Initialize(int width, int height);

    // Initialize D3D11 graphics
    void InitializeGraphics(int width, int height);

    // Handle update & draw
    float ComputeElapsedSeconds();
    void Update(float elapsedSeconds);
    void Draw();

    // Member window message processing function called from static version
    LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam);

    // Static callback, unpacks instance and forwards call
    static LRESULT CALLBACK s_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    static const wchar_t WindowClassName[];

    // Application window
    HWND _window;

    // Frame timing
    LARGE_INTEGER _prevTime;
    LARGE_INTEGER _frequency;

    // Game object
    std::unique_ptr<Game> _game;

    // Basic graphics resources
    Microsoft::WRL::ComPtr<ID3D11Device> _device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> _context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> _swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _backBufferRT;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> _depthBuffer;
    std::shared_ptr<SpriteBatch2> _spriteBatch;
};
