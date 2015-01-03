#include "Precomp.h"
#include "SpriteBatch.h"
#include "Texture.h"
#include "SpriteBatchVS.h"
#include "SpriteBatchPS.h"

using namespace Microsoft::WRL;

struct SpriteVertex
{
    float OffsetX, OffsetY;     // Quad's position/texturecoord in normalized coordinates (0 -> 1)
};

SpriteBatch::SpriteBatch(ID3D11Device* device)
    : _device(device)
    , _inBatch(false)
    , _invWidth(0)
    , _invHeight(0)
{
    ZeroMemory(&_constants, sizeof(_constants));

    _device->GetImmediateContext(&_context);
    CreateResources();
}

void SpriteBatch::Begin()
{
    D3D11_VIEWPORT viewports[16] = {};
    UINT numViewports = _countof(viewports);
    _context->RSGetViewports(&numViewports, viewports);

    _invWidth = 1.0f / viewports[0].Width;
    _invHeight = 1.0f / viewports[0].Height;

    _inBatch = true;
}

void SpriteBatch::End()
{
    // TODO: Save off old state?

    // Set up the graphics pipeline
    ID3D11Buffer* buffers[] = { _vertexBuffer.Get(), _instanceBuffer.Get() };
    uint32_t strides[] = { sizeof(SpriteVertex), sizeof(SpriteInstance) };
    uint32_t offsets[] = { 0, 0 };

    _context->IASetInputLayout(_inputLayout.Get());
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _context->IASetVertexBuffers(0, _countof(buffers), buffers, strides, offsets);
    _context->VSSetShader(_vertexShader.Get(), nullptr, 0);
    _context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
    _context->PSSetShader(_pixelShader.Get(), nullptr, 0);
    _context->PSSetSamplers(0, 1, _sampler.GetAddressOf());

    _context->UpdateSubresource(_constantBuffer.Get(), 0, nullptr, &_constants, sizeof(_constants), 0);

    for (auto pair = _instances.begin(); pair != _instances.end(); ++pair)
    {
        D3D11_BOX box = {};
        UINT instancesCompleted = 0;
        box.bottom = 1;
        box.back = 1;
        _context->PSSetShaderResources(0, 1, pair->first->GetSRVAddress());

        while (instancesCompleted < pair->second.size())
        {
            UINT instances = min(pair->second.size() - instancesCompleted, MaxInstancesPerTexture);
            box.right = sizeof(SpriteInstance) * instances;

            _context->UpdateSubresource(_instanceBuffer.Get(), 0, &box, pair->second.data(), box.right, 0);
            _context->DrawInstanced(6, instances, 0, 0);

            instancesCompleted += instances;
        }
    }

    // TODO: Restore state?

    _instances.clear();
    _constants._numInstances = 0;
    _inBatch = false;
}

void SpriteBatch::Draw(const std::shared_ptr<Texture>& texture, int x, int y)
{
    RECT dest = { x, y, x + texture->GetWidth(), y + texture->GetHeight() };
    Draw(texture, nullptr, &dest, 0);
}

void SpriteBatch::Draw(const std::shared_ptr<Texture>& texture, const RECT* dest)
{
    Draw(texture, nullptr, dest, 0);
}

void SpriteBatch::Draw(const std::shared_ptr<Texture>& texture, const RECT* source, const RECT* dest, float angle)
{
    if (!_inBatch)
    {
        throw std::exception("SpriteBatch Draw called outside of Begin/End().");
    }

    RECT src = { 0, 0, texture->GetWidth(), texture->GetHeight() };
    if (source)
    {
        src = *source;
    }

    SpriteInstance instance = {};
    instance.SourceLeft = src.left / (float)texture->GetWidth();
    instance.SourceTop = src.top / (float)texture->GetHeight();
    instance.SourceRight = src.right / (float)texture->GetWidth();
    instance.SourceBottom = src.bottom / (float)texture->GetHeight();

    instance.DestLeft = dest->left * _invWidth;
    instance.DestTop = dest->top * _invHeight;
    instance.DestRight = dest->right * _invWidth;
    instance.DestBottom = dest->bottom * _invHeight;

    instance.Rotation = angle;
    instance.zIndex = _constants._numInstances++;

    if (_instances.find(texture) == _instances.end())
    {
        _instances[texture] = std::vector<SpriteInstance>();
    }

    _instances[texture].push_back(instance);
}

void SpriteBatch::CreateResources()
{
    // Describe vertex shader input
    D3D11_INPUT_ELEMENT_DESC elems[5] = {};
    elems[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    elems[0].SemanticName = "POSITION";

    elems[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    elems[1].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[1].InputSlot = 1;
    elems[1].InstanceDataStepRate = 1;
    elems[1].SemanticName = "POSITION";
    elems[1].SemanticIndex = 1;

    elems[2].AlignedByteOffset = sizeof(float) * 4;
    elems[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    elems[2].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[2].InputSlot = 1;
    elems[2].InstanceDataStepRate = 1;
    elems[2].SemanticName = "POSITION";
    elems[2].SemanticIndex = 2;

    elems[3].AlignedByteOffset = sizeof(float) * 8;
    elems[3].Format = DXGI_FORMAT_R32_FLOAT;
    elems[3].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[3].InputSlot = 1;
    elems[3].InstanceDataStepRate = 1;
    elems[3].SemanticName = "TEXCOORD";

    elems[4].AlignedByteOffset = sizeof(float) * 9;
    elems[4].Format = DXGI_FORMAT_R32_UINT;
    elems[4].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[4].InputSlot = 1;
    elems[4].InstanceDataStepRate = 1;
    elems[4].SemanticName = "TEXCOORD";
    elems[4].SemanticIndex = 1;

    CHECKHR(_device->CreateVertexShader(SpriteBatchVS, sizeof(SpriteBatchVS), nullptr, &_vertexShader));
    CHECKHR(_device->CreateInputLayout(elems, _countof(elems), SpriteBatchVS, sizeof(SpriteBatchVS), &_inputLayout));

    CHECKHR(_device->CreatePixelShader(SpriteBatchPS, sizeof(SpriteBatchPS), nullptr, &_pixelShader));

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = sizeof(uint32_t) * 4;
    desc.StructureByteStride = desc.ByteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;

    CHECKHR(_device->CreateBuffer(&desc, nullptr, &_constantBuffer));
    
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.ByteWidth = sizeof(SpriteVertex) * 6;
    desc.StructureByteStride = sizeof(SpriteVertex);

    SpriteVertex quad[6] = 
    {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 },
        { 0, 0 },
        { 1, 1 },
        { 0, 1 },
    };

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = quad;
    init.SysMemPitch = sizeof(quad);

    CHECKHR(_device->CreateBuffer(&desc, &init, &_vertexBuffer));

    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.ByteWidth = sizeof(SpriteInstance) * MaxInstancesPerTexture;
    desc.StructureByteStride = sizeof(SpriteInstance);

    CHECKHR(_device->CreateBuffer(&desc, nullptr, &_instanceBuffer));

    D3D11_SAMPLER_DESC sd = {};
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    CHECKHR(_device->CreateSamplerState(&sd, &_sampler));
}
