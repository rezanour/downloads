#pragma once

class D3D11CSRaytracer
{
public:
    D3D11CSRaytracer(HWND window);
    virtual ~D3D11CSRaytracer();

    bool Initialize();

    bool Render(FXMMATRIX cameraWorld, float horizFovRadians);

private:
    HWND                                Window;
    uint32_t                            Width;
    uint32_t                            Height;
    ComPtr<ID3D11Device>                Device;
    ComPtr<ID3D11DeviceContext>         Context;
    ComPtr<IDXGISwapChain>              SwapChain;
    ComPtr<ID3D11UnorderedAccessView>   RenderTargetUAV;

    ComPtr<ID3D11Buffer>                NodesBuffer;
    ComPtr<ID3D11ShaderResourceView>    NodesSRV;

    ComPtr<ID3D11Buffer>                TasksBuffer;
    ComPtr<ID3D11UnorderedAccessView>   TasksUAV;
    ComPtr<ID3D11ShaderResourceView>    TasksSRV;

    ComPtr<ID3D11Texture2D>             TaskHeadsTexture;
    ComPtr<ID3D11UnorderedAccessView>   TaskHeadsUAV;

    ComPtr<ID3D11ComputeShader>         FirstPassCS;
    ComPtr<ID3D11ComputeShader>         TaskPassCS;

    struct CameraData
    {
        XMFLOAT4X4 CameraWorldTransform;
        XMFLOAT2 HalfViewportSize;
        float DistToProjPlane;
        float Pad;
    };
    ComPtr<ID3D11Buffer>                ConstantBuffer;

    struct AabbNode
    {
        XMFLOAT3 LeftMin, LeftMax;
        XMFLOAT3 RightMin, RightMax;
        int LeftIndex, RightIndex;
    };
    std::unique_ptr<AabbNode[]>         Nodes;
    static const uint32_t               MaxNodes = 0xffff;
    int                                 NumNodes;

    struct Task
    {
        int Node;
        int Next;
    };
    std::unique_ptr<Task[]>             Tasks;
    static const uint32_t               MaxTasks = 0xffff;
    int                                 NumTasks;
};
