#include "Precomp.h"
#include "Debug.h"
#include "Renderer.h"
#include "SimpleVS.h"
#include "SimplePS.h"
#include "RenderPlaneVS.h"
#include "RenderSTPlanePS.h"
#include "RenderUVPlanePS.h"

//#define WIREFRAME_SCENE

std::unique_ptr<Renderer> Renderer::Create(HWND window)
{
    std::unique_ptr<Renderer> renderer(new Renderer(window));
    if (renderer)
    {
        if (!renderer->Initialize())
        {
            LogError(L"Failed to initialize renderer.");
            return nullptr;
        }

        return renderer;
    }
    return nullptr;
}

Renderer::Renderer(HWND window)
    : Window(window)
{
    ZeroMemory(&RenderVP, sizeof(RenderVP));
}

Renderer::~Renderer()
{
}

void Renderer::SetLightField(const LightField& lightField)
{
    // Copy scene over
    Scene = lightField;
}

bool Renderer::Render(FXMMATRIX cameraView, FXMMATRIX cameraProjection, bool vsync)
{
    UNREFERENCED_PARAMETER(cameraView);
    UNREFERENCED_PARAMETER(cameraProjection);

    Clear();

    Context->RSSetViewports(1, &RenderVP);

    static const uint32_t stride = sizeof(SlabVertex);
    static const uint32_t offset = 0;
    Context->IASetVertexBuffers(0, 1, QuadVB.GetAddressOf(), &stride, &offset);
    Context->IASetInputLayout(RenderQuadIL.Get());

    Context->VSSetShader(RenderQuadVS.Get(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, RenderQuadCB.GetAddressOf());

    Constants constants;
    XMStoreFloat4x4(&constants.ViewProjection, cameraView * cameraProjection);

    for (auto slab : Scene.LightSlabs)
    {
        constants.LightSlabID = slab.ID;
        XMStoreFloat4x4(&constants.World, XMLoadFloat4x4(&slab.stQuadWorld));

        Context->PSSetShader(RenderSTPS.Get(), nullptr, 0);

        Context->UpdateSubresource(RenderQuadCB.Get(), 0, nullptr, &constants, sizeof(constants), 0);

        ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
        Context->PSSetShaderResources(0, _countof(nullSRVs), nullSRVs);

        static const float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
        Context->ClearRenderTargetView(stRTV.Get(), clearColor);
        Context->OMSetRenderTargets(1, stRTV.GetAddressOf(), nullptr);

        Context->Draw(6, 0);

        Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), nullptr);
        Context->PSSetShader(RenderUVPS.Get(), nullptr, 0);

        ID3D11ShaderResourceView* srvs[] = { stSRV.Get(), slab.Slices.Get() };
        Context->PSSetShaderResources(0, _countof(srvs), srvs);

        ID3D11SamplerState* samplers[] = { PointSampler.Get(), LinearSampler.Get() };
        Context->PSSetSamplers(0, _countof(samplers), samplers);

        XMStoreFloat4x4(&constants.World, XMLoadFloat4x4(&slab.uvQuadWorld));

        Context->UpdateSubresource(RenderQuadCB.Get(), 0, nullptr, &constants, sizeof(constants), 0);

        Context->Draw(6, 0);
    }

    return Present(vsync);
}

bool Renderer::Initialize()
{
    UINT d3dFlag = 0;
#if defined(_DEBUG)
    d3dFlag = D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    RECT clientRect = {};
    GetClientRect(Window, &clientRect);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = clientRect.right - clientRect.left;
    scd.BufferDesc.Height = clientRect.bottom - clientRect.top;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = Window;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        d3dFlag, &featureLevel, 1, D3D11_SDK_VERSION, &scd, &SwapChain, &Device, nullptr,
        &Context);
    if (FAILED(hr))
    {
        LogError(L"Failed to create graphics device and swapchain.");
        return false;
    }

    // Get back buffer
    ComPtr<ID3D11Texture2D> texture;
    hr = SwapChain->GetBuffer(0, IID_PPV_ARGS(&texture));
    if (FAILED(hr))
    {
        LogError(L"Failed to get backbuffer.");
        return false;
    }

    // Create render target view to it
    hr = Device->CreateRenderTargetView(texture.Get(), nullptr, &RenderTargetView);
    if (FAILED(hr))
    {
        LogError(L"Failed to create render target view.");
        return false;
    }

    // Get texture info & create compatible depth texture
    D3D11_TEXTURE2D_DESC td;
    texture->GetDesc(&td);

    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    td.Format = DXGI_FORMAT_D32_FLOAT;

    hr = Device->CreateTexture2D(&td, nullptr, texture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        LogError(L"Failed to create depth texture.");
        return false;
    }

    // Create depth stencil view to it
    hr = Device->CreateDepthStencilView(texture.Get(), nullptr, &DepthView);
    if (FAILED(hr))
    {
        LogError(L"Failed to create depth stencil view.");
        return false;
    }

    // Create the ST texture to store/read st (focal plane) samples from
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = Device->CreateTexture2D(&td, nullptr, texture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        LogError(L"Failed to create ST texture.");
        return false;
    }

    // Create both render target & shader resource views to it
    hr = Device->CreateRenderTargetView(texture.Get(), nullptr, &stRTV);
    if (FAILED(hr))
    {
        LogError(L"Failed to create ST render target view.");
        return false;
    }

    hr = Device->CreateShaderResourceView(texture.Get(), nullptr, &stSRV);
    if (FAILED(hr))
    {
        LogError(L"Failed to create ST shader resource view.");
        return false;
    }

    // Create shaders and input layout for rendering quads
    hr = Device->CreateVertexShader(RenderPlaneVS, sizeof(RenderPlaneVS), nullptr, &RenderQuadVS);
    if (FAILED(hr))
    {
        LogError(L"Failed to create quad render vertex shader.");
        return false;
    }

    hr = Device->CreatePixelShader(RenderSTPlanePS, sizeof(RenderSTPlanePS), nullptr, &RenderSTPS);
    if (FAILED(hr))
    {
        LogError(L"Failed to create quad render pixel shader.");
        return false;
    }

    hr = Device->CreatePixelShader(RenderUVPlanePS, sizeof(RenderUVPlanePS), nullptr, &RenderUVPS);
    if (FAILED(hr))
    {
        LogError(L"Failed to create quad render pixel shader.");
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC elems[2] = {};
    elems[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    elems[0].SemanticName = "POSITION";
    elems[1].AlignedByteOffset = sizeof(float) * 3;
    elems[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[1].SemanticName = "TEXCOORD";

    hr = Device->CreateInputLayout(elems, _countof(elems), RenderPlaneVS, sizeof(RenderPlaneVS), &RenderQuadIL);
    if (FAILED(hr))
    {
        LogError(L"Failed to create quad render input layout.");
        return false;
    }

    // Create the quad we use for rendering the slab planes
    SlabVertex quadVertices[] =
    {
        { XMFLOAT3(-0.5f, 0.5f, 0.f), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f, 0.5f, 0.f), XMFLOAT2(1, 0) },
        { XMFLOAT3(0.5f, -0.5f, 0.f), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f, 0.5f, 0.f), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f, -0.5f, 0.f), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f, -0.5f, 0.f), XMFLOAT2(0, 1) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(quadVertices);
    bd.StructureByteStride = sizeof(SlabVertex);
    bd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = quadVertices;
    init.SysMemPitch = sizeof(quadVertices);
    init.SysMemSlicePitch = init.SysMemPitch;

    hr = Device->CreateBuffer(&bd, &init, &QuadVB);
    if (FAILED(hr))
    {
        LogError(L"Failed to create quad vb.");
        return false;
    }

    // Create constant buffer
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(Constants);
    bd.StructureByteStride = bd.ByteWidth;

    hr = Device->CreateBuffer(&bd, nullptr, &RenderQuadCB);
    if (FAILED(hr))
    {
        LogError(L"Failed to create constant buffer.");
        return false;
    }

    // Create samplers
    D3D11_SAMPLER_DESC sd = {};
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

    hr = Device->CreateSamplerState(&sd, &PointSampler);
    if (FAILED(hr))
    {
        LogError(L"Failed to create sampler.");
        return false;
    }

    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    hr = Device->CreateSamplerState(&sd, &LinearSampler);
    if (FAILED(hr))
    {
        LogError(L"Failed to create sampler.");
        return false;
    }

    // Create main rendering viewport
    RenderVP.Width = (float)td.Width;
    RenderVP.Height = (float)td.Height;
    RenderVP.MaxDepth = 1.f;

    return true;
}

void Renderer::Clear()
{
    // Clear all the rendering views for a new frame
    static const float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
    Context->ClearRenderTargetView(RenderTargetView.Get(), clearColor);
    Context->ClearDepthStencilView(DepthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
    Context->ClearRenderTargetView(stRTV.Get(), clearColor);
}

bool Renderer::Present(bool vsync)
{
    HRESULT hr = SwapChain->Present(vsync ? 1 : 0, 0);
    if (FAILED(hr))
    {
        LogError(L"Present failed.");
        return false;
    }

    return true;
}

bool Renderer::CreateSimpleOutsideInLightField(LightField* lightField)
{
    lightField->LightSlabs.clear();

    // Create VB & IB for a colored cube
    struct CubeVertex
    {
        XMFLOAT3 Position;
        XMFLOAT3 Color;
    };

    CubeVertex vertices[] =
    {
        // Front
        { XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT3(1.f, 0.f, 0.f) },
        { XMFLOAT3(0.5f, 0.5f, -0.5f), XMFLOAT3(0.f, 1.f, 0.f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0.f, 0.f, 1.f) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.f, 1.f, 1.f) },
        // Back
        { XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(1.f, 0.f, 0.f) },
        { XMFLOAT3(-0.5f, 0.5f, 0.5f), XMFLOAT3(0.f, 1.f, 0.f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT3(0.f, 0.f, 1.f) },
        { XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT3(0.f, 1.f, 1.f) },
    };

    uint32_t indices[] =
    {
        0, 1, 2, 0, 2, 3,   // Front
        4, 5, 6, 4, 6, 7,   // Back
        5, 0, 3, 5, 3, 6,   // Left
        1, 4, 7, 1, 7, 2,   // Right
        5, 4, 1, 5, 1, 0,   // Top
        3, 2, 7, 3, 7, 6,   // Bottom
    };

    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(vertices);
    bd.StructureByteStride = sizeof(CubeVertex);
    bd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = vertices;
    init.SysMemPitch = sizeof(vertices);
    init.SysMemSlicePitch = init.SysMemPitch;

    ComPtr<ID3D11Buffer> vb;
    HRESULT hr = Device->CreateBuffer(&bd, &init, &vb);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test cube vertices.");
        return false;
    }

    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = sizeof(indices);
    bd.StructureByteStride = sizeof(uint32_t);

    init.pSysMem = indices;
    init.SysMemPitch = sizeof(indices);
    init.SysMemSlicePitch = init.SysMemPitch;

    ComPtr<ID3D11Buffer> ib;
    hr = Device->CreateBuffer(&bd, &init, &ib);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test cube indices.");
        return false;
    }

    // Create simple shaders to render it with
    ComPtr<ID3D11VertexShader> vertexShader;
    hr = Device->CreateVertexShader(SimpleVS, sizeof(SimpleVS), nullptr, &vertexShader);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test vertex shader.");
        return false;
    }

    ComPtr<ID3D11PixelShader> pixelShader;
    hr = Device->CreatePixelShader(SimplePS, sizeof(SimplePS), nullptr, &pixelShader);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test pixel shader.");
        return false;
    }

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC elems[2] = {};
    elems[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    elems[0].SemanticName = "POSITION";
    elems[1].AlignedByteOffset = sizeof(float) * 3;
    elems[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    elems[1].SemanticName = "COLOR";

    ComPtr<ID3D11InputLayout> inputLayout;
    hr = Device->CreateInputLayout(elems, _countof(elems), SimpleVS, sizeof(SimpleVS), &inputLayout);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test input layout.");
        return false;
    }

    // Create constant buffer to hold viewpoints to render from
    struct TestConstants
    {
        XMFLOAT4X4 ViewProjection;
    };

    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(TestConstants);
    bd.StructureByteStride = sizeof(TestConstants);

    ComPtr<ID3D11Buffer> cb;
    hr = Device->CreateBuffer(&bd, nullptr, &cb);
    if (FAILED(hr))
    {
        LogError(L"Failed to create test constant buffer.");
        return false;
    }

    // Create scratch render target for rendering views into
    D3D11_TEXTURE2D_DESC td = {};
    td.ArraySize = 1;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.Width = 512;
    td.Height = 512;
    td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;

    ComPtr<ID3D11Texture2D> scratch;
    hr = Device->CreateTexture2D(&td, nullptr, &scratch);
    if (FAILED(hr))
    {
        LogError(L"Failed to create scratch texture.");
        return false;
    }

    ComPtr<ID3D11RenderTargetView> rtv;
    hr = Device->CreateRenderTargetView(scratch.Get(), nullptr, &rtv);
    if (FAILED(hr))
    {
        LogError(L"Failed to scratch render target.");
        return false;
    }

#if defined(WIREFRAME_SCENE)
    ComPtr<ID3D11RasterizerState> rasterizerState;
    D3D11_RASTERIZER_DESC rd = {};
    rd.AntialiasedLineEnable = TRUE;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FillMode = D3D11_FILL_WIREFRAME;
    hr = Device->CreateRasterizerState(&rd, &rasterizerState);
    if (FAILED(hr))
    {
        LogError(L"Failed to create rasterizer state.");
        return false;
    }
    Context->RSSetState(rasterizerState.Get());
#endif

    // Configure the pipeline for rendering
    static const uint32_t stride = sizeof(CubeVertex);
    static const uint32_t offset = 0;
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->IASetInputLayout(inputLayout.Get());
    Context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
    Context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    Context->VSSetShader(vertexShader.Get(), nullptr, 0);
    Context->PSSetShader(pixelShader.Get(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    Context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)td.Width;
    vp.Height = (float)td.Height;
    vp.MaxDepth = 1.f;
    Context->RSSetViewports(1, &vp);

    // Create 4 light slabs, one from each direction pointing in
    XMFLOAT3 directions[] = 
    {
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(-1.f, 0.f, 0.f),
    };

    uint32_t id = 1;
    for (int i = 0; i < _countof(directions); ++i)
    {
        LightSlab slab;
        slab.ID = id++;

        // focal plane is inside of object, and small. (-0.5 -> 0.5) on plane tangent to the view direction, placed at origin
        // camera plane is same orientation as the focal plane, but placed out at 1.f unit along -direction, and is (-2.f -> 2.f)

        // We take 5x5 samples on the uv (camera) plane, and render from each of those perspectives. We need to define a skew projection matrix
        // such that it maps the st (focal) plane fully onto the camera location

        float zDist = 3.f;
        float nearZ = 2.f;
        float farZ = 6.f;
        float uvStart = -3.f;
        float uvEnd = 3.f;
        float stStart = -1.f;
        float stEnd = 1.f;

        float zRatio = nearZ / zDist;

        float uvStep = (uvEnd - uvStart) / 15.f;

        td.ArraySize = 16 * 16;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> sliceArray;
        hr = Device->CreateTexture2D(&td, nullptr, &sliceArray);
        if (FAILED(hr))
        {
            LogError(L"Failed to create slice array.");
            return false;
        }

        XMVECTOR forward = XMLoadFloat3(&directions[i]);
        XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
        XMVECTOR right = XMVector3Cross(up, forward);

        for (int y = 0; y < 16; ++y)
        {
            float uvy = uvEnd - (uvStep * y);

            for (int x = 0; x < 16; ++x)
            {
                static const float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
                Context->ClearRenderTargetView(rtv.Get(), clearColor);

                float uvx = uvStart + (uvStep * x);

                XMMATRIX projection = XMMatrixPerspectiveOffCenterLH(
                    zRatio * (stStart - uvx), zRatio * (stEnd - uvx),
                    zRatio * (stStart - uvy), zRatio * (stEnd - uvy),
                    nearZ,
                    farZ);

                XMMATRIX view = XMMatrixLookToLH(
                    right * uvx + up * uvy + forward * -zDist,
                    forward,
                    up);

                TestConstants constants;
                XMStoreFloat4x4(&constants.ViewProjection, view * projection);

                Context->UpdateSubresource(cb.Get(), 0, nullptr, &constants, sizeof(constants), 0);

                Context->DrawIndexed(_countof(indices), 0, 0);

                // Copy output to the appropriate slice
                Context->CopySubresourceRegion(sliceArray.Get(), D3D11CalcSubresource(0, y * 16 + x, 1), 0, 0, 0, scratch.Get(), 0, nullptr);
            }
        }

        hr = Device->CreateShaderResourceView(sliceArray.Get(), nullptr, &slab.Slices);
        if (FAILED(hr))
        {
            LogError(L"Failed to create light slab srv.");
            return false;
        }

        // Store the 2 world matrices of the slab planes
        XMMATRIX worldMatrix;
        worldMatrix.r[0] = right * (stEnd - stStart);
        worldMatrix.r[1] = up * (stEnd - stStart);
        worldMatrix.r[2] = forward;
        worldMatrix.r[3] = XMVectorSetW(XMVectorZero(), 1.f);
        XMStoreFloat4x4(&slab.stQuadWorld, worldMatrix);

        // For uv, move position back & scale right * up by 4
        XMVECTOR position = forward * -zDist;
        worldMatrix.r[0] = right * (uvEnd - uvStart);
        worldMatrix.r[1] = up * (uvEnd - uvStart);
        worldMatrix.r[3] = XMVectorSetW(position, 1.f);
        XMStoreFloat4x4(&slab.uvQuadWorld, worldMatrix);

        lightField->LightSlabs.push_back(slab);
    }

    Context->RSSetState(nullptr);

    return true;
}
