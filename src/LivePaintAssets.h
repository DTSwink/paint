#pragma once

#include "Types.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>

namespace sv {

class LivePaintAssets {
public:
    void Initialize(ID3D11Device* device);
    void Bind(ID3D11DeviceContext* context) const;

    ID3D11ShaderResourceView* BrushAtlas() const { return brushAtlas_.Get(); }

private:
    std::filesystem::path ResolveLivePaintImagePath(const wchar_t* fileName) const;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brushAtlas_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintersLut_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linenCanvas_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> canvasBase_;
};

} // namespace sv
