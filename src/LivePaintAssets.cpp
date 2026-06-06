#include "LivePaintAssets.h"
#include "TextureLoader.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>

namespace sv {

static std::filesystem::path ResolveAssetsRoot() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const auto exeDir = std::filesystem::path(path).parent_path();
    const auto nextToExe = exeDir / "assets";
    if (std::filesystem::exists(nextToExe)) return nextToExe;
    const auto projectAssets = exeDir.parent_path().parent_path().parent_path() / "assets";
    if (std::filesystem::exists(projectAssets)) return projectAssets;
    return nextToExe;
}

void LivePaintAssets::Initialize(ID3D11Device* device) {
    device_ = device;
    brushAtlas_ = TextureLoader::CreateSolidColor(device, 255, 255, 255, 255, false);
    linenCanvas_ = TextureLoader::CreateSolidColor(device, 255, 255, 255, 255, false);
    canvasBase_ = TextureLoader::CreateSolidColor(device, 255, 255, 255, 255, false);

    const auto lutPath = ResolveAssetsRoot() / "kuwahara_lut_32n8.tga";
    kuwaharaKernelLut_ = TextureLoader::LoadTexture(device, lutPath, false);
    if (!kuwaharaKernelLut_) {
        std::wcerr << L"[LivePaintAssets] Failed to load Kuwahara LUT: " << lutPath.wstring()
                   << L" — using fallback (Kuwahara will be a no-op)." << std::endl;
        kuwaharaKernelLut_ = TextureLoader::CreateSolidColor(device, 255, 255, 255, 255, false);
    } else {
        std::wcout << L"[LivePaintAssets] Loaded Kuwahara LUT: " << lutPath.filename().wstring() << std::endl;
    }
}

void LivePaintAssets::Bind(ID3D11DeviceContext* context) const {
    ID3D11ShaderResourceView* srvs[] = {
        brushAtlas_.Get(),
        kuwaharaKernelLut_.Get(),
        linenCanvas_.Get(),
        canvasBase_.Get(),
    };
    context->PSSetShaderResources(8, 4, srvs);
}

} // namespace sv
