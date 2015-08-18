#include "Precomp.h"
#include "Raytracer.h"
#include "RaytraceFirstPass.h"
#include "RaytraceTaskPass.h"

D3D11CSRaytracer::D3D11CSRaytracer(HWND window)
    : Window(window)
{
}

D3D11CSRaytracer::~D3D11CSRaytracer()
{
}

bool D3D11CSRaytracer::Initialize()
{
    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create DXGI factory.\n");
        assert(false);
        return false;
    }

    uint32_t flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, &featureLevel, 1, D3D11_SDK_VERSION, &Device, nullptr, &Context);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create D3D11 device\n");
        assert(false);
        return false;
    }

    RECT clientRect{};
    GetClientRect(Window, &clientRect);

    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = Width;
    scd.BufferDesc.Height = Height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
    scd.OutputWindow = Window;
    scd.SampleDesc.Count = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.Windowed = TRUE;

    hr = factory->CreateSwapChain(Device.Get(), &scd, &SwapChain);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create DXGI swapchain.\n");
        assert(false);
        return false;
    }

    ComPtr<ID3D11Texture2D> texture;
    hr = SwapChain->GetBuffer(0, IID_PPV_ARGS(&texture));
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to obtain back buffer texture.\n");
        assert(false);
        return false;
    }

    hr = Device->CreateUnorderedAccessView(texture.Get(), nullptr, &RenderTargetUAV);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create render target UAV.\n");
        assert(false);
        return false;
    }

    Nodes.reset(new AabbNode[MaxNodes]);
    Nodes[0].LeftIndex = -1;
    Nodes[0].RightIndex = -1;
    Nodes[0].LeftMin = XMFLOAT3(-1, -1, -1);
    Nodes[0].LeftMax = XMFLOAT3(0, 0, 0);
    Nodes[0].RightMin = XMFLOAT3(0, 0, 0);
    Nodes[0].RightMax = XMFLOAT3(1, 1, 1);

    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.ByteWidth = MaxNodes * sizeof(AabbNode);
    bd.StructureByteStride = sizeof(AabbNode);
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.Usage = D3D11_USAGE_DEFAULT;
    hr = Device->CreateBuffer(&bd, nullptr, &NodesBuffer);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create nodes buffer.\n");
        assert(false);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.NumElements = MaxNodes;
    hr = Device->CreateShaderResourceView(NodesBuffer.Get(), &srvd, &NodesSRV);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create nodes UAV.\n");
        assert(false);
        return false;
    }

    Context->UpdateSubresource(NodesBuffer.Get(), 0, nullptr, Nodes.get(), sizeof(AabbNode) * MaxNodes, sizeof(AabbNode) * MaxNodes);

    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bd.ByteWidth = MaxTasks * sizeof(Task);
    bd.StructureByteStride = sizeof(Task);
    hr = Device->CreateBuffer(&bd, nullptr, &TasksBuffer);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create tasks buffer.\n");
        assert(false);
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
    uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavd.Buffer.NumElements = MaxTasks;
    uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    hr = Device->CreateUnorderedAccessView(TasksBuffer.Get(), &uavd, &TasksUAV);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create tasks UAV.\n");
        assert(false);
        return false;
    }

    D3D11_TEXTURE2D_DESC td{};
    td.ArraySize = 1;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    td.Format = DXGI_FORMAT_R32_SINT;
    td.Width = Width;
    td.Height = Height;
    td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    hr = Device->CreateTexture2D(&td, nullptr, texture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create TaskHead texture.\n");
        assert(false);
        return false;
    }

    hr = Device->CreateUnorderedAccessView(texture.Get(), nullptr, &TaskHeadsUAV);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create TaskHeads UAV.\n");
        assert(false);
        return false;
    }

    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(CameraData);
    bd.StructureByteStride = bd.ByteWidth;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    hr = Device->CreateBuffer(&bd, nullptr, &ConstantBuffer);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create constant buffer.\n");
        assert(false);
        return false;
    }

    hr = Device->CreateComputeShader(RaytraceFirstPass, sizeof(RaytraceFirstPass), nullptr, &FirstPassCS);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create FirstPass compute shader.\n");
        assert(false);
        return false;
    }

    hr = Device->CreateComputeShader(RaytraceTaskPass, sizeof(RaytraceTaskPass), nullptr, &TaskPassCS);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create TaskPass compute shader.\n");
        assert(false);
        return false;
    }

    return true;
}

bool D3D11CSRaytracer::Render(FXMMATRIX cameraWorld, float horizFovRadians)
{
    static const uint32_t clearValue[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    Context->ClearUnorderedAccessViewUint(TaskHeadsUAV.Get(), clearValue);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = Context->Map(ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to map the constant buffer.\n");
        assert(false);
        return false;
    }

    float denom = tanf(horizFovRadians * 0.5f);
    assert(!isnan(denom) && fabsf(denom) > 0.00001f);

    CameraData* pConstants = (CameraData*)mapped.pData;
    XMStoreFloat4x4(&pConstants->CameraWorldTransform, cameraWorld);
    pConstants->HalfViewportSize = XMFLOAT2(Width * 0.5f, Height * 0.5f);
    pConstants->DistToProjPlane = Width * 0.5f / denom;

    Context->Unmap(ConstantBuffer.Get(), 0);

    Context->CSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());

    ID3D11ShaderResourceView* nullSRVs[]{ nullptr, nullptr, nullptr, nullptr };
    ID3D11UnorderedAccessView* nullUAVs[]{ nullptr, nullptr, nullptr, nullptr };
    uint32_t counts[]{ 0, 0, 0, 0 };
    Context->CSSetShaderResources(0, _countof(nullSRVs), nullSRVs);
    Context->CSSetUnorderedAccessViews(0, _countof(nullUAVs), nullUAVs, nullptr);

    ID3D11ShaderResourceView* firstPassSRVs[]{ NodesSRV.Get() };
    ID3D11UnorderedAccessView* firstPassUAVs[]{ TaskHeadsUAV.Get(), TasksUAV.Get() };
    Context->CSSetShader(FirstPassCS.Get(), nullptr, 0);
    Context->CSSetShaderResources(0, _countof(firstPassSRVs), firstPassSRVs);
    Context->CSSetUnorderedAccessViews(0, _countof(firstPassUAVs), firstPassUAVs, counts);
    Context->Dispatch(Width / 4, Height / 4, 1);

    Context->CSSetShaderResources(0, _countof(nullSRVs), nullSRVs);
    Context->CSSetUnorderedAccessViews(0, _countof(nullUAVs), nullUAVs, nullptr);

    ID3D11ShaderResourceView* taskPassSRVs[]{ NodesSRV.Get() };
    ID3D11UnorderedAccessView* taskPassUAVs[]{ RenderTargetUAV.Get(), TaskHeadsUAV.Get(), TasksUAV.Get() };
    Context->CSSetShader(FirstPassCS.Get(), nullptr, 0);
    Context->CSSetShaderResources(0, _countof(taskPassSRVs), taskPassSRVs);
    Context->CSSetUnorderedAccessViews(0, _countof(taskPassUAVs), taskPassUAVs, counts);
    Context->CSSetShader(TaskPassCS.Get(), nullptr, 0);

    // Do 10 iterations for now
    for (int i = 0; i < 10; ++i)
    {
        Context->Dispatch(Width / 4, Height / 4, 1);
    }

    Context->CSSetUnorderedAccessViews(0, _countof(nullUAVs), nullUAVs, nullptr);

    hr = SwapChain->Present(1, 0);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to present.\n");
        assert(false);
        return false;
    }

    return true;
}
