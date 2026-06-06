#include "RendererD3D11.h"
#include "ImGuiLayer.h"
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <wincodec.h>
#include <algorithm>
#include <dxgi.h>
#include <iostream>

namespace sv {

using namespace DirectX;
using Microsoft::WRL::ComPtr;

bool RendererD3D11::Initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    UINT flags = 0;
#ifdef SHADER_VIEWER_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL created{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    const HRESULT hrDevice = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 1,
        D3D11_SDK_VERSION, &device, &created, &context);
    if (FAILED(hrDevice)) {
        std::cerr << "D3D11CreateDevice failed." << std::endl;
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);
    ComPtr<IDXGIFactory> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGISwapChain> swapChain;
    if (FAILED(factory->CreateSwapChain(device.Get(), &scd, &swapChain))) {
        std::cerr << "CreateSwapChain failed." << std::endl;
        return false;
    }

    device_ = device;
    context_ = context;
    swapChain_ = swapChain;

    if (!CreateBackbufferResources(width, height)) return false;
    CreateSamplers();
    CreateConstantBuffers();

    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_BACK;
    rs.FrontCounterClockwise = FALSE;
    rs.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rs, &rasterState_);

    D3D11_RASTERIZER_DESC rsNoCull = rs;
    rsNoCull.CullMode = D3D11_CULL_NONE;
    device_->CreateRasterizerState(&rsNoCull, &rasterStateNoCull_);

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    device_->CreateDepthStencilState(&ds, &depthState_);

    D3D11_DEPTH_STENCIL_DESC dsOff = ds;
    dsOff.DepthEnable = FALSE;
    dsOff.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    device_->CreateDepthStencilState(&dsOff, &depthDisabledState_);

    // Large triangle covering the viewport (correct winding for D3D11 back-face cull).
    const std::vector<Vertex> fullscreenVertices = {
        {{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{3.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {2.f, 0.f}},
        {{-1.f, -3.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {0.f, 2.f}},
    };
    const std::vector<uint32_t> fullscreenIndices = {0, 1, 2};
    fullscreenMesh_ = UploadMesh(device_.Get(), fullscreenVertices, fullscreenIndices);

    return true;
}

bool RendererD3D11::CreateBackbufferResources(int width, int height) {
    rtv_.Reset();
    depthTex_.Reset();
    dsv_.Reset();

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_))) return false;

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = width;
    dd.Height = height;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device_->CreateTexture2D(&dd, nullptr, &depthTex_))) return false;
    if (FAILED(device_->CreateDepthStencilView(depthTex_.Get(), nullptr, &dsv_))) return false;
    return true;
}

void RendererD3D11::CreateSamplers() {
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&sd, &wrapSampler_);

    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&sd, &clampSampler_);

    D3D11_SAMPLER_DESC pointDesc = sd;
    pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    pointDesc.AddressU = pointDesc.AddressV = pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    device_->CreateSamplerState(&pointDesc, &pointSampler_);
}

void RendererD3D11::CreateConstantBuffers() {
    auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& out) {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (size + 15) & ~15u;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device_->CreateBuffer(&bd, nullptr, &out);
    };
    makeCB(sizeof(FrameCBData), frameCB_);
    makeCB(sizeof(ObjectCBData), objectCB_);
    makeCB(sizeof(MaterialCBData), materialCB_);
    makeCB(sizeof(LivePaintCBData), livePaintCB_);
}

void RendererD3D11::Resize(int width, int height) {
    if (!swapChain_ || width <= 0 || height <= 0) return;
    width_ = width;
    height_ = height;
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    rtv_.Reset();
    depthTex_.Reset();
    dsv_.Reset();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateBackbufferResources(width, height);
}

GpuMesh RendererD3D11::UploadMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    GpuMesh mesh;
    if (vertices.empty() || indices.empty()) return mesh;

    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{vertices.data()};
    device->CreateBuffer(&vbd, &vinit, &mesh.vertexBuffer);

    D3D11_BUFFER_DESC ibd{};
    ibd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit{indices.data()};
    device->CreateBuffer(&ibd, &iinit, &mesh.indexBuffer);
    mesh.indexCount = static_cast<UINT>(indices.size());
    return mesh;
}

void RendererD3D11::BeginFrame() {
    if (!rtv_) return;

    const float clear[4] = {0.08f, 0.09f, 0.11f, 1.f};
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
    context_->ClearRenderTargetView(rtv_.Get(), clear);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MaxDepth = 1.f;
    context_->RSSetViewports(1, &vp);
}

void RendererD3D11::Render(
    const Camera& camera,
    float time,
    const XMFLOAT3& lightDir,
    float lightIntensity,
    const XMFLOAT3& lightColor,
    float exposure,
    float ambientIntensity,
    float lightWrap,
    float scatterStrength,
    const Material& material,
    const MaterialParams& materialParams,
    const LivePaintParams& livePaint,
    const LivePaintAssets& livePaintAssets,
    const GpuMesh& mesh,
    ShaderCompiler& shaders) {
    if (!shaders.HasValidShaders()) return;

    const bool flatPreview = materialParams.view.presentation == ViewPresentation::Flat &&
        (materialParams.view.mode == ViewMode::Normal || materialParams.view.mode == ViewMode::UV);
    const bool flatTangentMapSheet = flatPreview &&
        materialParams.view.mode == ViewMode::Normal &&
        materialParams.view.normalSpace == NormalViewSpace::Tangent;
    const bool flatUvLayout = flatPreview && !flatTangentMapSheet;
    const GpuMesh& drawMesh = flatTangentMapSheet ? fullscreenMesh_ : mesh;
    if (!drawMesh.vertexBuffer) return;

    const float viewportX = ImGuiLayer::kSidebarWidth;
    const float viewportW = static_cast<float>(std::max(1, width_)) - viewportX;
    const float viewportH = static_cast<float>(height_);
    const bool splitNormalModify = flatPreview && materialParams.view.mode == ViewMode::Normal;

    context_->RSSetState(flatUvLayout ? rasterStateNoCull_.Get() : rasterState_.Get());
    context_->OMSetDepthStencilState(flatPreview ? depthDisabledState_.Get() : depthState_.Get(), 0);

    const XMMATRIX view = flatPreview ? XMMatrixIdentity() : camera.ViewMatrix();
    const XMMATRIX proj = flatPreview ? XMMatrixIdentity() : camera.ProjectionMatrix();
    const XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    const XMMATRIX model = XMMatrixIdentity();
    const XMMATRIX mvp = XMMatrixMultiply(model, viewProj);
    const XMMATRIX normalM = XMMatrixTranspose(XMMatrixInverse(nullptr, model));

    FrameCBData frame{};
    XMStoreFloat4x4(&frame.view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&frame.projection, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&frame.viewProjection, XMMatrixTranspose(viewProj));
    frame.cameraPosition = camera.Position();
    frame.time = time;
    frame.lightDirection = lightDir;
    frame.lightIntensity = lightIntensity;
    frame.lightColor = lightColor;
    frame.exposure = exposure;
    frame.ambientIntensity = ambientIntensity;
    frame.lightWrap = lightWrap;
    frame.scatterStrength = scatterStrength;
    const auto& flatView = materialParams.view;
    frame.flatViewPanX = flatView.flatViewPanX;
    frame.flatViewPanY = flatView.flatViewPanY;
    frame.flatViewZoom = std::max(flatView.flatViewZoom, 0.05f);

    ObjectCBData object{};
    XMStoreFloat4x4(&object.model, XMMatrixTranspose(model));
    XMStoreFloat4x4(&object.modelViewProjection, XMMatrixTranspose(mvp));
    XMStoreFloat4x4(&object.normalMatrix, XMMatrixTranspose(normalM));

    ID3D11SamplerState* samplers[] = {wrapSampler_.Get(), clampSampler_.Get(), pointSampler_.Get()};
    context_->PSSetSamplers(0, 3, samplers);
    material.ApplyToContext(context_.Get(), materialParams);
    livePaintAssets.Bind(context_.Get());
    context_->PSSetShader(shaders.PixelShader(), nullptr, 0);
    context_->IASetInputLayout(shaders.InputLayout());
    context_->VSSetShader(shaders.VertexShader(), nullptr, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer* vb = drawMesh.vertexBuffer.Get();
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context_->IASetIndexBuffer(drawMesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    auto drawPass = [&](float vpX, float vpW, bool normalModifierEnabled, bool livePaintForPass) {
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = vpX;
        vp.TopLeftY = 0.f;
        vp.Width = vpW;
        vp.Height = viewportH;
        vp.MaxDepth = 1.f;
        context_->RSSetViewports(1, &vp);

        frame.flatViewInvWidth = 1.f / std::max(vpW, 1.f);
        frame.flatViewInvHeight = 1.f / std::max(viewportH, 1.f);
        frame.flatViewOriginX = vpX;
        frame.flatViewOriginY = 0.f;

        const MaterialCBData matCB = material.BuildCB(materialParams, normalModifierEnabled);

        LivePaintParams passParams = livePaint;
        passParams.enabled = livePaintForPass;
        passParams.previewExaggeration =
            (normalModifierEnabled && livePaintForPass) ? livePaint.previewExaggeration : 1.f;
        const LivePaintCBData livePaintCB = BuildLivePaintCB(passParams, time);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(frameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &frame, sizeof(frame));
            context_->Unmap(frameCB_.Get(), 0);
        }
        if (SUCCEEDED(context_->Map(objectCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &object, sizeof(object));
            context_->Unmap(objectCB_.Get(), 0);
        }
        if (SUCCEEDED(context_->Map(materialCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &matCB, sizeof(matCB));
            context_->Unmap(materialCB_.Get(), 0);
        }
        if (SUCCEEDED(context_->Map(livePaintCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &livePaintCB, sizeof(livePaintCB));
            context_->Unmap(livePaintCB_.Get(), 0);
        }

        ID3D11Buffer* cbs[] = {frameCB_.Get(), objectCB_.Get(), materialCB_.Get(), livePaintCB_.Get()};
        context_->VSSetConstantBuffers(0, 4, cbs);
        context_->PSSetConstantBuffers(0, 4, cbs);
        context_->DrawIndexed(drawMesh.indexCount, 0, 0);
    };

    if (splitNormalModify) {
        const float halfW = viewportW * 0.5f;
        drawPass(viewportX, halfW, false, false);
        drawPass(viewportX + halfW, halfW, true, true);
    } else {
        drawPass(viewportX, viewportW, false, livePaint.enabled);
    }
}

void RendererD3D11::Present() {
    if (swapChain_) swapChain_->Present(1, 0);
}

bool RendererD3D11::CaptureBackbuffer(const std::filesystem::path& path) const {
    if (!swapChain_ || !device_ || !context_) return false;

    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&resource)))) return false;

    DirectX::ScratchImage image;
    if (FAILED(DirectX::CaptureTexture(device_.Get(), context_.Get(), resource.Get(), image))) return false;

    const DirectX::Image* img = image.GetImage(0, 0, 0);
    if (!img) return false;

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    return SUCCEEDED(DirectX::SaveToWICFile(
        *img, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, path.wstring().c_str()));
}

} // namespace sv
