#pragma once

#include "Camera.h"
#include "LivePaint.h"
#include "LivePaintAssets.h"
#include "Material.h"
#include "ShaderCompiler.h"
#include "Types.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>
#include <vector>

namespace sv {

struct FrameCBData {
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 cameraPosition;
    float time;
    DirectX::XMFLOAT3 lightDirection;
    float lightIntensity;
    DirectX::XMFLOAT3 lightColor;
    float exposure;
    float ambientIntensity;
    float lightWrap;
    float scatterStrength;
    float flatViewPanX;
    float flatViewPanY;
    float flatViewZoom;
    float flatViewInvWidth;
    float flatViewInvHeight;
    float flatViewOriginX;
    float flatViewOriginY;
};

struct ObjectCBData {
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 modelViewProjection;
    DirectX::XMFLOAT4X4 normalMatrix;
};

struct GpuMesh {
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount = 0;
};

class RendererD3D11 {
public:
    bool Initialize(HWND hwnd, int width, int height);
    void Resize(int width, int height);
    void BeginFrame();
    void Render(
        const Camera& camera,
        float time,
        const DirectX::XMFLOAT3& lightDir,
        float lightIntensity,
        const DirectX::XMFLOAT3& lightColor,
        float exposure,
        float ambientIntensity,
        float lightWrap,
        float scatterStrength,
        const Material& material,
        const MaterialParams& materialParams,
        const LivePaintParams& livePaint,
        const LivePaintAssets& livePaintAssets,
        const GpuMesh& mesh,
        ShaderCompiler& shaders);

    void Present();

    bool CaptureBackbuffer(const std::filesystem::path& path) const;

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }
    int Width() const { return width_; }
    int Height() const { return height_; }

    static GpuMesh UploadMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

private:
    bool CreateBackbufferResources(int width, int height);
    void CreateSamplers();
    void CreateConstantBuffers();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterStateNoCull_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabledState_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> wrapSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> frameCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> objectCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> materialCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> livePaintCB_;
    GpuMesh fullscreenMesh_;

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace sv
