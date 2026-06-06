#include "TextureLoader.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>

namespace sv {

using Microsoft::WRL::ComPtr;

static void LogTextureError(const std::filesystem::path& path, const char* msg) {
    std::wcerr << L"[TextureLoader] " << msg << L": " << path.wstring() << std::endl;
}

ComPtr<ID3D11ShaderResourceView> TextureLoader::CreateSolidColor(
    ID3D11Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool srgb) {
    const uint32_t pixel = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
                           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel;
    init.SysMemPitch = 4;

    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateTexture2D(&desc, &init, &tex))) return {};
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return {};
    return srv;
}

ComPtr<ID3D11ShaderResourceView> TextureLoader::LoadTexture(
    ID3D11Device* device, const std::filesystem::path& path, bool srgb) {
    if (!std::filesystem::exists(path)) {
        LogTextureError(path, "File not found");
        return {};
    }

    DirectX::ScratchImage image;
    DirectX::TexMetadata meta{};
    HRESULT hr = E_FAIL;
    const std::wstring ext = path.extension().wstring();
    const auto lower = [&] {
        std::wstring e = ext;
        for (wchar_t& c : e) c = static_cast<wchar_t>(towlower(c));
        return e;
    }();

    if (lower == L".dds") {
        hr = DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, &meta, image);
    } else if (lower == L".tga") {
        hr = DirectX::LoadFromTGAFile(path.c_str(), &meta, image);
    } else if (lower == L".hdr") {
        hr = DirectX::LoadFromHDRFile(path.c_str(), &meta, image);
    } else {
        const DirectX::WIC_FLAGS flags = srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_NONE;
        hr = DirectX::LoadFromWICFile(path.c_str(), flags, &meta, image);
    }

    if (FAILED(hr)) {
        LogTextureError(path, "Failed to load texture");
        return {};
    }

    DirectX::ScratchImage converted;
    const DirectX::Image* images = image.GetImages();
    size_t imageCount = image.GetImageCount();
    DirectX::TexMetadata outMeta = meta;

    if (meta.format != DXGI_FORMAT_R8G8B8A8_UNORM && meta.format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
        meta.format != DXGI_FORMAT_B8G8R8A8_UNORM && meta.format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        const DXGI_FORMAT target = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        if (FAILED(DirectX::Convert(image.GetImages(), image.GetImageCount(), meta, target,
                                    DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) {
            LogTextureError(path, "Failed to convert texture format");
            return {};
        }
        images = converted.GetImages();
        imageCount = converted.GetImageCount();
        outMeta = converted.GetMetadata();
    }

    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(CreateShaderResourceView(device, images, imageCount, outMeta, &srv))) {
        LogTextureError(path, "Failed to create SRV");
        return {};
    }
    return srv;
}

ComPtr<ID3D11ShaderResourceView> TextureLoader::CreateProceduralBrushAtlas(ID3D11Device* device, int size) {
    const int w = std::max(512, size);
    const int h = w;
    const int grid = 8;
    const int tileSize = w / grid;
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));

    auto hash = [](int x, int y, int seed) -> float {
        const float n = std::sin(static_cast<float>(x * 374761 + y * 668265 + seed * 982451)) * 43758.5453f;
        return n - std::floor(n);
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int tileX = x / tileSize;
            const int tileY = y / tileSize;
            const int localX = x - tileX * tileSize;
            const int localY = y - tileY * tileSize;
            const float fx = (static_cast<float>(localX) + 0.5f) / static_cast<float>(tileSize);
            const float fy = (static_cast<float>(localY) + 0.5f) / static_cast<float>(tileSize);
            const float seed = hash(tileX, tileY, 17) * 6.2831853f;
            const float angle = seed + hash(tileX, tileY, 31) * 3.14159f;
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);
            float rx = fx - 0.5f;
            float ry = fy - 0.5f;
            const float rotX = cosA * rx - sinA * ry;
            const float rotY = sinA * rx + cosA * ry;
            rx = rotX + 0.5f;
            ry = rotY + 0.5f;

            const float stroke = std::exp(-std::pow((rx - 0.5f) * 3.5f, 2.f) - std::pow((ry - 0.5f) * 12.f, 2.f));
            const float grain = 0.65f + 0.35f * std::sin((rx + ry) * 40.f + seed);
            const float height = std::clamp(stroke * grain, 0.f, 1.f);
            const float mask = std::clamp(std::exp(-std::pow((rx - 0.5f) * 2.5f, 2.f) - std::pow((ry - 0.5f) * 8.f, 2.f)), 0.f, 1.f);
            const float ao = 0.45f + 0.55f * height;

            const size_t i = static_cast<size_t>((y * w + x) * 4);
            pixels[i + 0] = static_cast<uint8_t>(height * 255.f);
            pixels[i + 1] = static_cast<uint8_t>(mask * 255.f);
            pixels[i + 2] = static_cast<uint8_t>(ao * 255.f);
            pixels[i + 3] = 255;
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = w * 4;

    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateTexture2D(&desc, &init, &tex))) return {};
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return {};
    return srv;
}

} // namespace sv
