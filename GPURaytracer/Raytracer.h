#pragma once

/// GPU based ray tracer
class Raytracer
{
public:
    static std::unique_ptr<Raytracer> Create(HWND window);
    ~Raytracer();

    void SetFOV(float horizFovRadians);
    bool Render(FXMMATRIX cameraWorldTransform, bool vsync);

private:
    Raytracer(HWND hwnd);

    // Don't allow copy
    Raytracer(const Raytracer&);
    Raytracer& operator= (const Raytracer&);

    bool Initialize();
    void Clear();
    bool Present(bool vsync);

    // Create a test scene
    bool GenerateTestScene();

private:
    // Basic rendering/buffer
    HWND Window;
    int Width;
    int Height;
    ComPtr<ID3D11Device> Device;
    ComPtr<ID3D11DeviceContext> Context;
    ComPtr<IDXGISwapChain> SwapChain;
    ComPtr<ID3D11UnorderedAccessView> RenderTargetUAV;
    ComPtr<ID3D11ComputeShader> ComputeShader;

    // For computing eye rays
    float HalfWidth;
    float HalfHeight;
    float hFov;
    float DistToProjPlane;

    // Constants to pass shader
    struct CameraData
    {
        XMFLOAT4X4 CameraWorldTransform;
        XMFLOAT2 HalfSize;
        float DistToProjPlane;
        float Padding;
    };
    ComPtr<ID3D11Buffer> CameraDataCB;

    // Environment cube map
    ComPtr<ID3D11Texture2D> EnvTexture;
    ComPtr<ID3D11ShaderResourceView> EnvMapSRV;
    ComPtr<ID3D11SamplerState> EnvMapSampler;

    // Simple scene initially as spheres for testing
    struct SphereObject
    {
        XMFLOAT3 Center;
        float Radius;
        XMFLOAT3 Color;
        float Reflectiveness;
    };

    struct PointLight
    {
        XMFLOAT3 Position;
        XMFLOAT3 Color;
        float Radius;
    };

    ComPtr<ID3D11Buffer> SphereObjects;
    ComPtr<ID3D11ShaderResourceView> SphereObjectsSRV;

    ComPtr<ID3D11Buffer> PointLights;
    ComPtr<ID3D11ShaderResourceView> PointLightsSRV;
};
