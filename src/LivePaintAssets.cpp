#include "LivePaintAssets.h"
#include "TextureLoader.h"

#include <Windows.h>

#include <array>

namespace sv {

namespace {

std::filesystem::path GetExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

} // namespace

std::filesystem::path LivePaintAssets::ResolveLivePaintImagePath(const wchar_t* fileName) const {
    const auto exeDir = GetExecutableDirectory();
    const std::array<std::filesystem::path, 4> candidates = {
        exeDir / "assets" / "live_paint" / fileName,
        exeDir.parent_path().parent_path().parent_path() / "assets" / "live_paint" / fileName,
        std::filesystem::path(L"C:\\Users\\singerie\\Documents\\Cursor\\temp\\ShaderViewer")
            / "assets" / "live_paint" / fileName,
        std::filesystem::path(L"C:\\Users\\singerie\\Documents\\Cursor\\temp\\BlenderLivePaintFilterAnalysis")
            / "blend_live_paint_analysis" / "images" / fileName,
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    return {};
}

void LivePaintAssets::Initialize(ID3D11Device* device) {
    device_ = device;
    const auto brushPath = ResolveLivePaintImagePath(L"RGB_Brushstrokes_64.png");
    if (!brushPath.empty()) {
        brushAtlas_ = TextureLoader::LoadTexture(device, brushPath, false);
    }
    if (!brushAtlas_) {
        brushAtlas_ = TextureLoader::CreateProceduralBrushAtlas(device);
    }

    const auto lutPath = ResolveLivePaintImagePath(L"Painters_LUTs.jpg");
    const auto linenPath = ResolveLivePaintImagePath(L"Linen_Canvas_Texture.jpg");
    const auto canvasPath = ResolveLivePaintImagePath(L"Canvas_Base_Color.png");
    if (!lutPath.empty()) paintersLut_ = TextureLoader::LoadTexture(device, lutPath, false);
    if (!linenPath.empty()) linenCanvas_ = TextureLoader::LoadTexture(device, linenPath, true);
    if (!canvasPath.empty()) canvasBase_ = TextureLoader::LoadTexture(device, canvasPath, true);
}

void LivePaintAssets::Bind(ID3D11DeviceContext* context) const {
    ID3D11ShaderResourceView* srvs[] = {
        brushAtlas_.Get(),
        paintersLut_.Get(),
        linenCanvas_.Get(),
        canvasBase_.Get(),
    };
    context->PSSetShaderResources(8, 4, srvs);
}

} // namespace sv
