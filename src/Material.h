#pragma once

#include "Types.h"
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <string>
#include <array>

namespace sv {

struct MaterialCBData {
    DirectX::XMFLOAT4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float aoFactor;
    float normalStrength;
    float emissiveStrength;
    int hasAlbedo;
    int hasNormal;
    int hasRoughness;
    int hasMetallic;
    int hasAO;
    int hasEmissive;
    int hasHeight;
    int hasPackedMask;
    int flipNormalGreen;
    int packedR;
    int packedG;
    int packedB;
    int viewMode;
    int normalViewSpace;
    int uvShowAlbedo;
    int uvShowWireframe;
    int viewFlatPreview;
    int normalModifierEnabled;
    float uvCheckerScale;
    float normalViewScale;
    float viewPad;
};

struct MaterialParams {
    DirectX::XMFLOAT4 baseColorFactor{1.f, 1.f, 1.f, 1.f};
    float metallicFactor = 0.f;
    float roughnessFactor = 0.5f;
    float aoFactor = 1.f;
    float normalStrength = 1.f;
    float emissiveStrength = 1.f;
    bool flipNormalGreen = false;
    PackedChannelMeaning packedR = PackedChannelMeaning::AO;
    PackedChannelMeaning packedG = PackedChannelMeaning::Roughness;
    PackedChannelMeaning packedB = PackedChannelMeaning::Metallic;
    ViewParams view{};
};

class Material {
public:
    void SetDevice(ID3D11Device* device);

    bool LoadFromSet(ID3D11Device* device, const MaterialSet& set);
    void ApplyToContext(ID3D11DeviceContext* context, const MaterialParams& params) const;
    MaterialCBData BuildCB(const MaterialParams& params, bool normalModifierEnabled = false) const;
    void BindDefaultTextures(ID3D11DeviceContext* context) const;

    const std::array<std::wstring, static_cast<size_t>(TextureSlot::Count)>& Paths() const { return paths_; }
    const std::array<bool, static_cast<size_t>(TextureSlot::Count)>& HasMap() const { return hasMap_; }
    std::array<bool, static_cast<size_t>(TextureSlot::Count)>& SrgbOverride() { return srgbOverride_; }
    const std::array<bool, static_cast<size_t>(TextureSlot::Count)>& SrgbOverride() const { return srgbOverride_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, static_cast<size_t>(TextureSlot::Count)> srvs_;
    std::array<std::wstring, static_cast<size_t>(TextureSlot::Count)> paths_;
    std::array<bool, static_cast<size_t>(TextureSlot::Count)> hasMap_{};
    std::array<bool, static_cast<size_t>(TextureSlot::Count)> srgbOverride_{};

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormalSRV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blackSRV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> defaultRoughSRV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> defaultMetallicSRV_;

    void CreateDefaults();
    bool LoadSlot(TextureSlot slot, const fs::path& path, bool defaultSrgb);
};

} // namespace sv
