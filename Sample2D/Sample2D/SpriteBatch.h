#pragma once

class Texture;

//
// Pixel space sprite batch. Great for drawing sprites by specifying where in pixels
//
class SpriteBatch
{
public:
    SpriteBatch(ID3D11Device* device);

    // Passing non-null viewport gives a world space of positive Y up
    // All draw calls are then treated as though in that world space.
    // Passing null treats draw calls as pixel-space (positive Y down)
    void Begin();
    void End();

    void Draw(const std::shared_ptr<Texture>& texture, int x, int y);
    void Draw(const std::shared_ptr<Texture>& texture, const RECT* dest);
    void Draw(const std::shared_ptr<Texture>& texture, const RECT* source, const RECT* dest, float angle);

    // Graphics access
    ID3D11Device* GetDevice() { return _device.Get(); }

private:
    // Prevent copy
    SpriteBatch(const SpriteBatch&);
    SpriteBatch& operator= (const SpriteBatch&);

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
        float SourceLeft, SourceTop, SourceRight, SourceBottom; // Source rectangle in texture, in normalized coords (0 -> 1)
        float DestLeft, DestTop, DestRight, DestBottom;         // Destination rectangle on screen, in normalized coords (0 -> 1)
        float Rotation;                                         // Rotation to apply to sprite, in radians
        unsigned int zIndex;                                    // Relative index (order to draw things, used to compute z)
    };

    struct Constants
    {
        uint32_t _numInstances;
        uint32_t _pad[3];
    };

    bool _inBatch;
    float _invWidth, _invHeight;
    Constants _constants;

    std::map<std::shared_ptr<Texture>, std::vector<SpriteInstance>> _instances;
};
