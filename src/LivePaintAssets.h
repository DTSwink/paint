#pragma once

#include "Types.h"
#include <wrl/client.h>
#include <d3d11.h>

namespace sv {

class LivePaintAssets {
public:
    void Initialize(ID3D11Device* device);
    void Bind(ID3D11DeviceContext* context) const;
    ID3D11ShaderResourceView* KuwaharaKernelLut() const { return kuwaharaKernelLut_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brushAtlas_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> kuwaharaKernelLut_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linenCanvas_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> canvasBase_;
};

} // namespace sv
