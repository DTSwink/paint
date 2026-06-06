#pragma once

#include "AssetScanner.h"
#include "Camera.h"
#include "LivePaint.h"
#include "LivePaintAssets.h"
#include "Material.h"
#include "RendererD3D11.h"
#include "ShaderCompiler.h"
#include "Types.h"
#include <DirectXMath.h>
#include <string>

struct AppState {
    sv::Camera camera;
    sv::Material material;
    sv::MaterialParams materialParams;
    sv::AssetScanner scanner;
    sv::ShaderCompiler shaders;
    sv::LivePaintParams livePaint{};
    sv::LivePaintAssets livePaintAssets;
    sv::PreviewMeshKind meshKind = sv::PreviewMeshKind::Suzanne;
    int selectedMaterial = -1;
    int selectedMesh = -1;
    float lightIntensity = 2.4f;
    DirectX::XMFLOAT3 lightColor{1.f, 1.f, 1.f};
    DirectX::XMFLOAT3 lightDirection{0.35f, -0.85f, 0.38f};
    bool cameraRelativeLight = false;
    float lightPitchOffset = 0.35f;
    float lightSideOffset = 0.18f;
    float ambientIntensity = 0.11f;
    float lightWrap = 0.55f;
    float scatterStrength = 0.35f;
    float exposure = 1.f;
    float time = 0.f;
    std::string scanRoot;
    char scanRootEdit[512] = "";
    std::wstring shaderRoot;
    std::wstring statusMessage;
    bool requestRescan = false;
    bool requestApplySavedSettings = false;
    bool requestDeferredStartup = false;
    bool requestShaderReload = false;
    bool requestResetCamera = false;
    bool requestResetFlatView = false;
    bool orbitDragging = false;
    bool panDragging = false;
    DirectX::XMFLOAT2 lastMouse{0.f, 0.f};
    DirectX::XMFLOAT3 meshFocusCenter{0.f, 0.f, 0.f};
    float meshFocusRadius = 1.f;
    int activeSidebarTab = 0;
    int selectSidebarTabOnce = -1;
    int selectViewTabOnce = -1;
    std::string savedMaterialName;
    std::string savedMeshFile;
    int savedMeshKind = static_cast<int>(sv::PreviewMeshKind::Suzanne);
    int savedSelectedMesh = -1;
    bool restoreSavedCamera = false;
    DirectX::XMFLOAT3 savedCameraTarget{0.f, 0.f, 0.f};
    float savedCameraYaw = 0.f;
    float savedCameraPitch = 0.25f;
    float savedCameraDistance = 3.f;
    bool applyModifierToRender = false;
};

namespace sv {

class ImGuiLayer {
public:
    static constexpr float kSidebarWidth = 380.f;

    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void NewFrame();
    void BuildUI(AppState& app, GpuMesh& activeMesh, ID3D11Device* device);
    void Render(ID3D11DeviceContext* context);

private:
    bool BeginLeftSidebar(const char* id);
    void EndLeftSidebar();
    void DrawLightingPanel(AppState& app);
    void DrawViewPanel(AppState& app);
    void DrawAssetsPanel(AppState& app, GpuMesh& activeMesh, ID3D11Device* device);
    void DrawMaterialPanel(AppState& app, ID3D11Device* device);
    void DrawLivePaintPanel(AppState& app);
    void DrawShadersPanel(AppState& app);
    void DrawViewportOverlay(const AppState& app);
    void DrawQuickViewControls(AppState& app);
    void DrawChannelCombo(const char* label, PackedChannelMeaning& value);
    const char* SlotLabel(TextureSlot slot) const;
};

} // namespace sv
