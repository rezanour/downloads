#pragma once

// A single light slab, which consists of a camera and focal plane.
// At uniform sampling frequency along the camera plane, skewed perspective
// renders of the focal plane are rendered and stored in a 2D array of 'slices'.
struct LightSlab
{
    uint32_t ID;

    // The technique for rendering the light slab is as follows:
    //   - Render the stQuad, storing the values into the stRTV
    //      using r,g to store the s,t values, and b to identify the
    //      light slab ID (0 means no light slab, empty space).
    //   - Bind the stRTV as an stSRV and then render the uvQuad.
    //   - For each pixel in the uvQuad, sample the stSRV to see if
    //      there is data for it. If so, we compute the filtered (u,v,s,t)
    //      sample from the Slices array and illuminate the pixel.

    // matrices to transform these quads to their location in the world
    XMFLOAT4X4 uvQuadWorld;
    XMFLOAT4X4 stQuadWorld;

    // 2D array of light field slices on uvPlane.
    ComPtr<ID3D11ShaderResourceView> Slices;
};

// A light field is a set of light slabs all from the same scene with
// static configuration and lighting. They can be combined to form the
// final image.
struct LightField
{
    std::vector<LightSlab> LightSlabs;
};

// Light Field Renderer
class Renderer
{
public:
    static std::unique_ptr<Renderer> Create(HWND window);
    ~Renderer();

    // Creates a simple scene of a colored cube, and generates a
    // 4 slab outside-in lightfield around it.
    bool CreateSimpleOutsideInLightField(LightField* lightField);

    // Set the active light field scene
    void SetLightField(const LightField& lightField);

    // Render the scene with the camera view provided
    bool Render(FXMMATRIX cameraView, FXMMATRIX cameraProjection, bool vsync);

private:
    Renderer(HWND window);

    // No copy
    Renderer(const Renderer&);
    Renderer& operator= (const Renderer&);

    bool Initialize();

    void Clear();
    bool Present(bool vsync);

private:
    HWND Window;
    ComPtr<ID3D11Device> Device;
    ComPtr<ID3D11DeviceContext> Context;
    ComPtr<IDXGISwapChain> SwapChain;
    ComPtr<ID3D11RenderTargetView> RenderTargetView;
    ComPtr<ID3D11DepthStencilView> DepthView;
    D3D11_VIEWPORT RenderVP;

    // Current scene
    LightField Scene;

    // For rendering the light field
    struct SlabVertex
    {
        XMFLOAT3 Position;
        XMFLOAT2 TexCoord;
    };

    ComPtr<ID3D11InputLayout> RenderQuadIL;
    ComPtr<ID3D11VertexShader> RenderQuadVS;
    ComPtr<ID3D11PixelShader> RenderSTPS;
    ComPtr<ID3D11PixelShader> RenderUVPS;
    ComPtr<ID3D11Buffer> RenderQuadCB;
    ComPtr<ID3D11RenderTargetView> stRTV;
    ComPtr<ID3D11ShaderResourceView> stSRV;
    ComPtr<ID3D11SamplerState> LinearSampler;
    ComPtr<ID3D11Buffer> QuadVB;    // For rendering camera & focal planes

    struct Constants
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 ViewProjection;
        uint32_t LightSlabID;
        XMFLOAT3 Padding;
    };
};
