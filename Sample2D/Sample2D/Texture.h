#pragma once

class Texture
{
public:
    Texture(ID3D11Device* device, int width, int height, const float color[4]);
    Texture(ID3D11Device* device, const wchar_t* filename);

    ID3D11ShaderResourceView* GetSRV() { return _srv.Get(); }
    ID3D11ShaderResourceView** GetSRVAddress() { return _srv.GetAddressOf(); }

    int GetWidth() const { return _width; }
    int GetHeight() const { return _height; }

private:
    // Prevent copy
    Texture(const Texture&);
    Texture& operator= (const Texture&);

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _srv;
    int _width;
    int _height;
};
