#pragma once

class Texture;

struct RectF
{
    RectF() : left(0), top(0), right(0), bottom(0)
    {
    }

    explicit RectF(float left, float top, float right, float bottom)
        : left(left), top(top), right(right), bottom(bottom)
    {
    }

    float left;
    float top;
    float right;
    float bottom;
};


//
// World space (with viewport support), 2D rendering. Positive Y is up, etc...
// For rendering 2D scenes.
//
class SpriteBatch2
{
public:
    SpriteBatch2(ID3D11Device* device);

    void Begin(const Vector2& viewportCenter, const Vector2& viewportSize);
    void End();

    void Draw(const std::shared_ptr<Texture>& texture, const Vector2& position);
    void Draw(const std::shared_ptr<Texture>& texture, const Vector2& position, float rotation);
    void Draw(const std::shared_ptr<Texture>& texture, const RECT* source, const Vector2& position, const Vector2& size, float rotation);

    // Graphics access
    ID3D11Device* GetDevice() { return _device.Get(); }

private:
    // Prevent copy
    SpriteBatch2(const SpriteBatch2&);
    SpriteBatch2& operator= (const SpriteBatch2&);

    // Create graphics resources used by the sprite batch
    void CreateResources();

private:
    static const uint32_t MaxInstancesPerTexture = 65536;

    Microsoft::WRL::ComPtr<ID3D11Device> _device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> _context;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _inputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _instanceBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampler;

    struct SpriteInstance
    {
        RectF SourceRect;       // Source rectangle in texture, in normalized coords (0 -> 1)
        Vector2 DestPosition;   // Destination position
        Vector2 Size;
        float Rotation;
        unsigned int zIndex;                                    // Relative index (order to draw things, used to compute z)
    };

    struct Constants
    {
        uint32_t _numInstances;
        uint32_t _pad[3];
        Vector2 ViewportCenter;
        Vector2 ViewportSize;
    };

    bool _inBatch;
    Constants _constants;

    std::map<std::shared_ptr<Texture>, std::vector<SpriteInstance>> _instances;
};
