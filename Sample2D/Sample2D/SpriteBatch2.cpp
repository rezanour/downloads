#include "Precomp.h"
#include "SpriteBatch2.h"
#include "Texture.h"
#include "SpriteBatch2VS.h"
#include "SpriteBatchPS.h"

using namespace Microsoft::WRL;

struct SpriteVertex2
{
    Vector2 Position;
    Vector2 TexCoord;
};

SpriteBatch2::SpriteBatch2(ID3D11Device* device)
    : _device(device)
    , _inBatch(false)
{
    ZeroMemory(&_constants, sizeof(_constants));

    _device->GetImmediateContext(&_context);
    CreateResources();
}

void SpriteBatch2::Begin(const Vector2& viewportCenter, const Vector2& viewportSize)
{
    _constants.ViewportCenter = viewportCenter;
    _constants.ViewportSize = viewportSize;
    _inBatch = true;
}

void SpriteBatch2::End()
{
    // TODO: Save off old state?

    // Set up the graphics pipeline
    ID3D11Buffer* buffers[] = { _vertexBuffer.Get(), _instanceBuffer.Get() };
    uint32_t strides[] = { sizeof(SpriteVertex2), sizeof(SpriteInstance) };
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

void SpriteBatch2::Draw(const std::shared_ptr<Texture>& texture, const Vector2& position)
{
    Draw(texture, nullptr, position, Vector2(texture->GetWidth(), texture->GetHeight()), 0.0f);
}

void SpriteBatch2::Draw(const std::shared_ptr<Texture>& texture, const Vector2& position, float rotation)
{
    Draw(texture, nullptr, position, Vector2(texture->GetWidth(), texture->GetHeight()), rotation);
}

void SpriteBatch2::Draw(const std::shared_ptr<Texture>& texture, const RECT* source, const Vector2& position, const Vector2& size, float rotation)
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
    instance.SourceRect.left= src.left / (float)texture->GetWidth();
    instance.SourceRect.top = src.top / (float)texture->GetHeight();
    instance.SourceRect.right = src.right / (float)texture->GetWidth();
    instance.SourceRect.bottom = src.bottom / (float)texture->GetHeight();
    instance.DestPosition = position;
    instance.Size = size;
    instance.Rotation = rotation;
    instance.zIndex = _constants._numInstances++;

    if (_instances.find(texture) == _instances.end())
    {
        _instances[texture] = std::vector<SpriteInstance>();
    }

    _instances[texture].push_back(instance);
}

void SpriteBatch2::CreateResources()
{
    // Describe vertex shader input
    D3D11_INPUT_ELEMENT_DESC elems[7] = {};
    elems[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    elems[0].SemanticName = "POSITION";

    elems[1].AlignedByteOffset = sizeof(Vector2);
    elems[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    elems[1].SemanticName = "TEXCOORD";

    elems[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    elems[2].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[2].InputSlot = 1;
    elems[2].InstanceDataStepRate = 1;
    elems[2].SemanticName = "POSITION";
    elems[2].SemanticIndex = 1;

    elems[3].AlignedByteOffset = sizeof(float) * 4;
    elems[3].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[3].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[3].InputSlot = 1;
    elems[3].InstanceDataStepRate = 1;
    elems[3].SemanticName = "POSITION";
    elems[3].SemanticIndex = 2;

    elems[4].AlignedByteOffset = sizeof(float) * 4 + sizeof(Vector2);
    elems[4].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[4].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[4].InputSlot = 1;
    elems[4].InstanceDataStepRate = 1;
    elems[4].SemanticName = "TEXCOORD";
    elems[4].SemanticIndex = 1;

    elems[5].AlignedByteOffset = sizeof(float) * 4 + sizeof(Vector2) * 2;
    elems[5].Format = DXGI_FORMAT_R32_FLOAT;
    elems[5].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[5].InputSlot = 1;
    elems[5].InstanceDataStepRate = 1;
    elems[5].SemanticName = "TEXCOORD";
    elems[5].SemanticIndex = 2;

    elems[6].AlignedByteOffset = sizeof(float) * 4 + sizeof(Vector2) * 2 + sizeof(float);
    elems[6].Format = DXGI_FORMAT_R32_UINT;
    elems[6].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    elems[6].InputSlot = 1;
    elems[6].InstanceDataStepRate = 1;
    elems[6].SemanticName = "TEXCOORD";
    elems[6].SemanticIndex = 3;

    CHECKHR(_device->CreateVertexShader(SpriteBatch2VS, sizeof(SpriteBatch2VS), nullptr, &_vertexShader));
    CHECKHR(_device->CreateInputLayout(elems, _countof(elems), SpriteBatch2VS, sizeof(SpriteBatch2VS), &_inputLayout));

    CHECKHR(_device->CreatePixelShader(SpriteBatchPS, sizeof(SpriteBatchPS), nullptr, &_pixelShader));

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = sizeof(Constants);
    desc.StructureByteStride = desc.ByteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;

    CHECKHR(_device->CreateBuffer(&desc, nullptr, &_constantBuffer));

    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.ByteWidth = sizeof(SpriteVertex2) * 6;
    desc.StructureByteStride = sizeof(SpriteVertex2);

    SpriteVertex2 quad[6] =
    {
        { Vector2(-0.5f, 0.5f), Vector2(0, 0) },
        { Vector2(0.5f, 0.5f), Vector2(1, 0) },
        { Vector2(0.5f, -0.5f), Vector2(1, 1) },

        { Vector2(-0.5f, 0.5f), Vector2(0, 0) },
        { Vector2(0.5f, -0.5f), Vector2(1, 1) },
        { Vector2(-0.5f, -0.5f), Vector2(0, 1) },
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
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    CHECKHR(_device->CreateSamplerState(&sd, &_sampler));
}
