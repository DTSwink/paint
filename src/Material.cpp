#include "Material.h"
#include "TextureLoader.h"

namespace sv {

void Material::SetDevice(ID3D11Device* device) {
    device_ = device;
    CreateDefaults();
}

void Material::CreateDefaults() {
    whiteSRV_ = TextureLoader::CreateSolidColor(device_.Get(), 255, 255, 255, 255, true);
    flatNormalSRV_ = TextureLoader::CreateSolidColor(device_.Get(), 128, 128, 255, 255, false);
    blackSRV_ = TextureLoader::CreateSolidColor(device_.Get(), 0, 0, 0, 255, false);
    defaultRoughSRV_ = TextureLoader::CreateSolidColor(device_.Get(), 128, 128, 128, 255, false);
    defaultMetallicSRV_ = TextureLoader::CreateSolidColor(device_.Get(), 0, 0, 0, 255, false);
}

bool Material::LoadSlot(TextureSlot slot, const fs::path& path, bool defaultSrgb) {
    if (path.empty()) {
        hasMap_[static_cast<size_t>(slot)] = false;
        paths_[static_cast<size_t>(slot)].clear();
        return true;
    }
    const bool useSrgb = srgbOverride_[static_cast<size_t>(slot)] ? !defaultSrgb : defaultSrgb;
    auto srv = TextureLoader::LoadTexture(device_.Get(), path, useSrgb);
    if (!srv) {
        hasMap_[static_cast<size_t>(slot)] = false;
        return false;
    }
    srvs_[static_cast<size_t>(slot)] = srv;
    hasMap_[static_cast<size_t>(slot)] = true;
    paths_[static_cast<size_t>(slot)] = path.wstring();
    return true;
}

bool Material::LoadFromSet(ID3D11Device* device, const MaterialSet& set) {
    device_ = device;
    if (!whiteSRV_) CreateDefaults();

    srgbOverride_.fill(false);
    bool ok = true;
    ok &= LoadSlot(TextureSlot::Albedo, set.albedo, true);
    ok &= LoadSlot(TextureSlot::Normal, set.normal, false);
    ok &= LoadSlot(TextureSlot::Roughness, set.roughness, false);
    ok &= LoadSlot(TextureSlot::Metallic, set.metallic, false);
    ok &= LoadSlot(TextureSlot::AO, set.ao, false);
    ok &= LoadSlot(TextureSlot::Emissive, set.emissive, true);
    ok &= LoadSlot(TextureSlot::Height, set.height, false);
    ok &= LoadSlot(TextureSlot::PackedMask, set.packedMask, false);
    return ok;
}

void Material::BindDefaultTextures(ID3D11DeviceContext* context) const {
    ID3D11ShaderResourceView* srvs[8] = {
        whiteSRV_.Get(), flatNormalSRV_.Get(), defaultRoughSRV_.Get(), defaultMetallicSRV_.Get(),
        whiteSRV_.Get(), blackSRV_.Get(), blackSRV_.Get(), blackSRV_.Get()
    };
    context->PSSetShaderResources(0, 8, srvs);
}

MaterialCBData Material::BuildCB(const MaterialParams& params, bool normalModifierEnabled) const {
    MaterialCBData cb{};
    cb.baseColorFactor = params.baseColorFactor;
    cb.metallicFactor = params.metallicFactor;
    cb.roughnessFactor = params.roughnessFactor;
    cb.aoFactor = params.aoFactor;
    cb.normalStrength = params.normalStrength;
    cb.emissiveStrength = params.emissiveStrength;
    cb.hasAlbedo = hasMap_[static_cast<size_t>(TextureSlot::Albedo)] ? 1 : 0;
    cb.hasNormal = hasMap_[static_cast<size_t>(TextureSlot::Normal)] ? 1 : 0;
    cb.hasRoughness = hasMap_[static_cast<size_t>(TextureSlot::Roughness)] ? 1 : 0;
    cb.hasMetallic = hasMap_[static_cast<size_t>(TextureSlot::Metallic)] ? 1 : 0;
    cb.hasAO = hasMap_[static_cast<size_t>(TextureSlot::AO)] ? 1 : 0;
    cb.hasEmissive = hasMap_[static_cast<size_t>(TextureSlot::Emissive)] ? 1 : 0;
    cb.hasHeight = hasMap_[static_cast<size_t>(TextureSlot::Height)] ? 1 : 0;
    cb.hasPackedMask = hasMap_[static_cast<size_t>(TextureSlot::PackedMask)] ? 1 : 0;
    cb.flipNormalGreen = params.flipNormalGreen ? 1 : 0;
    cb.packedR = static_cast<int>(params.packedR);
    cb.packedG = static_cast<int>(params.packedG);
    cb.packedB = static_cast<int>(params.packedB);
    cb.viewMode = static_cast<int>(params.view.mode);
    cb.normalViewSpace = static_cast<int>(params.view.normalSpace);
    cb.uvShowAlbedo = params.view.uvShowAlbedo ? 1 : 0;
    cb.uvShowWireframe = params.view.uvShowWireframe ? 1 : 0;
    cb.viewFlatPreview = params.view.presentation == ViewPresentation::Flat ? 1 : 0;
    cb.normalModifierEnabled = normalModifierEnabled ? 1 : 0;
    cb.uvCheckerScale = params.view.uvCheckerScale;
    cb.normalViewScale = params.view.normalViewScale;
    cb.viewPad = 0.f;
    return cb;
}

void Material::ApplyToContext(ID3D11DeviceContext* context, const MaterialParams& params) const {
    auto pick = [&](TextureSlot slot, ID3D11ShaderResourceView* fallback) -> ID3D11ShaderResourceView* {
        const size_t i = static_cast<size_t>(slot);
        return hasMap_[i] && srvs_[i] ? srvs_[i].Get() : fallback;
    };

    ID3D11ShaderResourceView* srvs[8] = {
        pick(TextureSlot::Albedo, whiteSRV_.Get()),
        pick(TextureSlot::Normal, flatNormalSRV_.Get()),
        pick(TextureSlot::Roughness, defaultRoughSRV_.Get()),
        pick(TextureSlot::Metallic, defaultMetallicSRV_.Get()),
        pick(TextureSlot::AO, whiteSRV_.Get()),
        pick(TextureSlot::Emissive, blackSRV_.Get()),
        pick(TextureSlot::Height, blackSRV_.Get()),
        pick(TextureSlot::PackedMask, blackSRV_.Get())
    };
    context->PSSetShaderResources(0, 8, srvs);
    (void)params;
}

} // namespace sv
