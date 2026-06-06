#pragma once

#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>

namespace sv {

class TextureLoader {
public:
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTexture(
        ID3D11Device* device, const std::filesystem::path& path, bool srgb);
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColor(
        ID3D11Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool srgb);
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateProceduralBrushAtlas(ID3D11Device* device, int size = 512);
};

} // namespace sv
