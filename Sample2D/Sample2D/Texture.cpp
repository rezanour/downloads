#include "Precomp.h"
#include "Texture.h"

using namespace Microsoft::WRL;

Texture::Texture(ID3D11Device* device, int width, int height, const float color[4])
    : _width(width)
    , _height(height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;

    std::unique_ptr<uint32_t[]> pixels(new uint32_t[width * height]);
    for (int i = 0; i < width * height; ++i)
    {
        pixels[i] =
            (uint32_t)(color[3] * 255.0f) << 24 |
            (uint32_t)(color[2] * 255.0f) << 16 |
            (uint32_t)(color[1] * 255.0f) << 8 |
            (uint32_t)(color[0] * 255.0f);

    }
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = pixels.get();
    init.SysMemPitch = width * sizeof(uint32_t);
    init.SysMemSlicePitch = init.SysMemPitch * height;

    CHECKHR(device->CreateTexture2D(&desc, &init, &_texture));
    CHECKHR(device->CreateShaderResourceView(_texture.Get(), nullptr, &_srv));
}

Texture::Texture(ID3D11Device* device, const wchar_t* filename)
    : _width(0)
    , _height(0)
{
    ComPtr<IWICImagingFactory> wicFactory;
    CHECKHR(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&wicFactory)));

    ComPtr<IWICBitmapDecoder> decoder;
    CHECKHR(wicFactory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));

    ComPtr<IWICBitmapFrameDecode> frame;
    CHECKHR(decoder->GetFrame(0, &frame));

    ComPtr<IWICFormatConverter> converter;
    CHECKHR(wicFactory->CreateFormatConverter(&converter));
    CHECKHR(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 1.0f, WICBitmapPaletteTypeCustom));

    uint32_t width, height;
    CHECKHR(converter->GetSize(&width, &height));
    _width = (int)width;
    _height = (int)height;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;

    std::unique_ptr<uint32_t[]> pixels(new uint32_t[width * height]);
    CHECKHR(converter->CopyPixels(nullptr, width * sizeof(uint32_t), width * height * sizeof(uint32_t), (BYTE*)pixels.get()));

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = pixels.get();
    init.SysMemPitch = width * sizeof(uint32_t);
    init.SysMemSlicePitch = init.SysMemPitch * height;

    CHECKHR(device->CreateTexture2D(&desc, &init, &_texture));
    CHECKHR(device->CreateShaderResourceView(_texture.Get(), nullptr, &_srv));
}
