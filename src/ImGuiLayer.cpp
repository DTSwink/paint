#include "ImGuiLayer.h"
#include "AppMesh.h"
#include "MeshGen.h"
#include "MeshLoader.h"
#include "RendererD3D11.h"
#include "UserSettings.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <Shellapi.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace sv {

bool ImGuiLayer::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(hwnd)) return false;
    if (!ImGui_ImplDX11_Init(device, context)) return false;
    return true;
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::NewFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Render(ID3D11DeviceContext* context) {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    (void)context;
}

bool ImGuiLayer::BeginLeftSidebar(const char* id) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSidebarWidth, vp->WorkSize.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
    return ImGui::Begin(id, nullptr, flags);
}

void ImGuiLayer::EndLeftSidebar() {
    ImGui::End();
    ImGui::PopStyleVar(2);
}

const char* ImGuiLayer::SlotLabel(TextureSlot slot) const {
    switch (slot) {
    case TextureSlot::Albedo: return "Albedo";
    case TextureSlot::Normal: return "Normal";
    case TextureSlot::Roughness: return "Roughness";
    case TextureSlot::Metallic: return "Metallic";
    case TextureSlot::AO: return "AO";
    case TextureSlot::Emissive: return "Emissive";
    case TextureSlot::Height: return "Height";
    case TextureSlot::PackedMask: return "Packed";
    default: return "Unknown";
    }
}

void ImGuiLayer::DrawChannelCombo(const char* label, PackedChannelMeaning& value) {
    const char* items[] = {"Unused", "AO", "Roughness", "Metallic"};
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, IM_ARRAYSIZE(items))) {
        value = static_cast<PackedChannelMeaning>(current);
    }
}

void ImGuiLayer::DrawLightingPanel(AppState& app) {
    ImGui::Checkbox("Camera-relative light", &app.cameraRelativeLight);
    if (app.cameraRelativeLight) {
        ImGui::TextWrapped("Light follows the camera (shadows stay fixed on screen).");
        ImGui::SliderFloat("Light pitch (behind you)", &app.lightPitchOffset, -0.5f, 1.f);
        ImGui::SliderFloat("Light side angle", &app.lightSideOffset, -1.f, 1.f);
    } else {
        ImGui::TextWrapped("World light: shadows stay on the mesh as you orbit.");
        ImGui::SliderFloat3("Light Direction", &app.lightDirection.x, -1.f, 1.f);
    }
    ImGui::SliderFloat("Light Intensity", &app.lightIntensity, 0.f, 20.f);
    ImGui::ColorEdit3("Light Color", &app.lightColor.x);
    ImGui::SliderFloat("Ambient fill", &app.ambientIntensity, 0.f, 1.f);
    ImGui::SliderFloat("Shadow wrap", &app.lightWrap, 0.f, 1.f);
    ImGui::SliderFloat("Scatter fill", &app.scatterStrength, 0.f, 1.f);
    ImGui::SliderFloat("Exposure", &app.exposure, 0.1f, 5.f);
}

namespace {

ImGuiTabItemFlags TabSelectOnce(int tabIndex, int selectOnce) {
    return selectOnce == tabIndex ? ImGuiTabItemFlags_SetSelected : 0;
}

} // namespace

void ImGuiLayer::DrawViewPanel(AppState& app) {
    const bool flatActive = app.materialParams.view.presentation == sv::ViewPresentation::Flat &&
        (app.materialParams.view.mode == sv::ViewMode::Normal ||
         app.materialParams.view.mode == sv::ViewMode::UV);
    if (flatActive) {
        ImGui::Text("Left/right drag: pan sheet | Wheel: zoom");
        if (ImGui::Button("Reset flat view")) app.requestResetFlatView = true;
    } else {
        ImGui::Text("Left drag: orbit | Right drag: pan | Wheel: zoom");
        if (ImGui::Button("Reset Camera")) app.requestResetCamera = true;
    }

    if (ImGui::BeginTabBar("ViewModeTabs")) {
        if (ImGui::BeginTabItem("Render", nullptr, TabSelectOnce(0, app.selectViewTabOnce))) {
            app.materialParams.view.mode = sv::ViewMode::Render;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Normal", nullptr, TabSelectOnce(1, app.selectViewTabOnce))) {
            app.materialParams.view.mode = sv::ViewMode::Normal;
            int space = static_cast<int>(app.materialParams.view.normalSpace);
            const char* spaces[] = {"Tangent space (map RGB)", "World (smooth)", "Face (faceted)"};
            if (ImGui::Combo("Normal space", &space, spaces, IM_ARRAYSIZE(spaces))) {
                app.materialParams.view.normalSpace = static_cast<sv::NormalViewSpace>(space);
            }
            ImGui::TextWrapped("Tangent: map RGB. World: smooth vertex normals. Face: hard triangle facets.");
            ImGui::TextWrapped("Tangent flat: normal map texture. World/Face flat: mesh UV layout bake.");
            ImGui::TextWrapped("Flat preview: pan/zoom. Split view: original (left) vs modifier (right).");
            ImGui::SliderFloat("Normal view scale", &app.materialParams.view.normalViewScale, 0.1f, 4.f);
            ImGui::SliderFloat("Normal map strength", &app.materialParams.normalStrength, 0.f, 4.f);
            ImGui::Checkbox("Flip normal green", &app.materialParams.flipNormalGreen);
            bool flatPreview = app.materialParams.view.presentation == sv::ViewPresentation::Flat;
            if (ImGui::Checkbox("Flat preview (2D texture)", &flatPreview)) {
                app.materialParams.view.presentation =
                    flatPreview ? sv::ViewPresentation::Flat : sv::ViewPresentation::Mesh;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("UV", nullptr, TabSelectOnce(2, app.selectViewTabOnce))) {
            app.materialParams.view.mode = sv::ViewMode::UV;
            ImGui::SliderFloat("Checker scale", &app.materialParams.view.uvCheckerScale, 1.f, 64.f);
            ImGui::Checkbox("Show albedo overlay", &app.materialParams.view.uvShowAlbedo);
            ImGui::Checkbox("Show UV grid", &app.materialParams.view.uvShowWireframe);
            ImGui::TextWrapped("Flat preview: mesh UV islands with checker/albedo overlay.");
            bool flatPreview = app.materialParams.view.presentation == sv::ViewPresentation::Flat;
            if (ImGui::Checkbox("Flat preview (2D texture)", &flatPreview)) {
                app.materialParams.view.presentation =
                    flatPreview ? sv::ViewPresentation::Flat : sv::ViewPresentation::Mesh;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        app.selectViewTabOnce = -1;
    }
}

void ImGuiLayer::DrawAssetsPanel(AppState& app, GpuMesh& activeMesh, ID3D11Device* device) {
    ImGui::InputText("Scan Root", app.scanRootEdit, IM_ARRAYSIZE(app.scanRootEdit));
    app.scanRoot = app.scanRootEdit;
    if (ImGui::Button("Rescan Berserk Assets")) app.requestRescan = true;

    ImGui::Separator();
    ImGui::Text("Material Sets (%d)", static_cast<int>(app.scanner.MaterialSets().size()));
    ImGui::BeginChild("MaterialSets", ImVec2(0, 120), true);
    for (int i = 0; i < static_cast<int>(app.scanner.MaterialSets().size()); ++i) {
        const auto& set = app.scanner.MaterialSets()[static_cast<size_t>(i)];
        if (ImGui::Selectable(set.name.c_str(), app.selectedMaterial == i)) {
            app.selectedMaterial = i;
            if (app.material.LoadFromSet(device, set)) {
                app.statusMessage = L"Loaded material: " + std::wstring(set.name.begin(), set.name.end());
            } else {
                app.statusMessage = L"Failed to load one or more textures.";
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Text("Meshes (%d)", static_cast<int>(app.scanner.MeshFiles().size()));
    ImGui::BeginChild("Meshes", ImVec2(0, 160), true);
    if (ImGui::Selectable("Suzanne (Blender monkey)", app.meshKind == sv::PreviewMeshKind::Suzanne)) {
        sv::LoadMeshFromPath(sv::ResolveSuzannePath(), sv::PreviewMeshKind::Suzanne, activeMesh, app, device);
    }
    const char* previewNames[] = {"Sphere", "Cube", "Plane"};
    const sv::PreviewMeshKind previewKinds[] = {
        sv::PreviewMeshKind::Sphere, sv::PreviewMeshKind::Cube, sv::PreviewMeshKind::Plane};
    for (int i = 0; i < 3; ++i) {
        if (ImGui::Selectable(previewNames[i], app.meshKind == previewKinds[i] && app.selectedMesh < 0)) {
            app.meshKind = previewKinds[i];
            app.selectedMesh = -1;
            sv::MeshData data;
            if (i == 0) data = sv::MeshGen::CreateSphere();
            else if (i == 1) data = sv::MeshGen::CreateCube();
            else data = sv::MeshGen::CreatePlane();
            activeMesh = sv::RendererD3D11::UploadMesh(device, data.vertices, data.indices);
            sv::FocusCameraOnMesh(app.camera, data.vertices, app.meshFocusCenter, app.meshFocusRadius);
        }
    }
    for (int i = 0; i < static_cast<int>(app.scanner.MeshFiles().size()); ++i) {
        const auto& path = app.scanner.MeshFiles()[static_cast<size_t>(i)];
        const std::string label = path.filename().string();
        if (ImGui::Selectable(label.c_str(), app.selectedMesh == i)) {
            app.selectedMesh = i;
            app.meshKind = PreviewMeshKind::Imported;
            LoadedMesh loaded;
            if (MeshLoader::Load(path, loaded)) {
                activeMesh = RendererD3D11::UploadMesh(device, loaded.vertices, loaded.indices);
                sv::FocusCameraOnMesh(app.camera, loaded.vertices, app.meshFocusCenter, app.meshFocusRadius);
                app.statusMessage = L"Loaded mesh: " + path.filename().wstring();
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Text("UAsset Texture Candidates (%d)", static_cast<int>(app.scanner.UAssetCandidates().size()));
    if (!app.scanner.UAssetCandidates().empty()) {
        ImGui::TextWrapped("Use tools/ExportUnrealTextures.py to export Texture2D assets to Saved/ShaderViewerExports.");
    }
}

void ImGuiLayer::DrawMaterialPanel(AppState& app, ID3D11Device* device) {
    (void)device;
    ImGui::ColorEdit4("Base Color", &app.materialParams.baseColorFactor.x);
    ImGui::SliderFloat("Metallic", &app.materialParams.metallicFactor, 0.f, 1.f);
    ImGui::SliderFloat("Roughness", &app.materialParams.roughnessFactor, 0.f, 1.f);
    ImGui::SliderFloat("AO", &app.materialParams.aoFactor, 0.f, 1.f);
    ImGui::SliderFloat("Normal Strength", &app.materialParams.normalStrength, 0.f, 4.f);
    ImGui::SliderFloat("Emissive Strength", &app.materialParams.emissiveStrength, 0.f, 10.f);
    ImGui::Checkbox("Flip Normal Green", &app.materialParams.flipNormalGreen);

    ImGui::Separator();
    DrawChannelCombo("Packed R", app.materialParams.packedR);
    DrawChannelCombo("Packed G", app.materialParams.packedG);
    DrawChannelCombo("Packed B", app.materialParams.packedB);

    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(TextureSlot::Count); ++i) {
        const auto slot = static_cast<TextureSlot>(i);
        const auto& paths = app.material.Paths();
        const auto& has = app.material.HasMap();
        auto& srgb = app.material.SrgbOverride()[static_cast<size_t>(i)];
        ImGui::Text("%s: %s", SlotLabel(slot), has[static_cast<size_t>(i)] ? "bound" : "default");
        if (!paths[static_cast<size_t>(i)].empty()) {
            ImGui::TextWrapped("%ls", paths[static_cast<size_t>(i)].c_str());
        }
        ImGui::Checkbox((std::string("Force sRGB override##") + SlotLabel(slot)).c_str(), &srgb);
    }
}

void ImGuiLayer::DrawLivePaintPanel(AppState& app) {
    auto& lp = app.livePaint;
    ImGui::Checkbox("Apply to mesh shading", &lp.enabled);
    ImGui::TextWrapped("Translates Blender Live Paint Filter node inputs into HLSL. Flat preview split compares original vs modified.");

    if (ImGui::CollapsingHeader("Painting Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Painting Thickness", &lp.paintingThickness, 0.01f, 1.f);
        ImGui::SliderFloat("Padding", &lp.padding, 0.f, 1.f);
        ImGui::Checkbox("Canvas", &lp.canvas);
        ImGui::Checkbox("Holdout", &lp.holdout);
        ImGui::Checkbox("Transparent Background", &lp.transparentBackground);
    }

    if (ImGui::CollapsingHeader("Stroke Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Stroke Density", &lp.strokeDensity, 0.05f, 0.f, 10000.f);
        ImGui::DragFloat("Min Stroke Scale", &lp.minStrokeScale, 0.01f, 0.f, 100.f);
        ImGui::DragFloat("Max Stroke Scale", &lp.maxStrokeScale, 0.01f, 0.f, 100.f);
        ImGui::DragFloat("Scale Threshold", &lp.scaleThreshold, 0.05f, 0.f, 100.f);
        ImGui::SliderFloat("Min Stroke Opacity", &lp.minStrokeOpacity, 0.f, 1.f);
        ImGui::SliderFloat("Max Stroke Opacity", &lp.maxStrokeOpacity, 0.f, 1.f);
        ImGui::DragFloat("Opacity Threshold", &lp.opacityThreshold, 0.1f, 1.f, 100.f);
        ImGui::Checkbox("Stack Direction", &lp.stackDirection);
        ImGui::Checkbox("Broken Edges", &lp.brokenEdges);
        ImGui::Checkbox("Random Seed Per Frame", &lp.randomSeedPerFrame);
        ImGui::InputInt("Seed", &lp.seed);
    }

    if (ImGui::CollapsingHeader("Shader Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Painter's Color Filter", &lp.paintersColorFilter);
        ImGui::SliderFloat("Filter Strength", &lp.filterStrength, 0.f, 1.f);
        ImGui::SliderFloat("Hue Variation", &lp.hueVariation, 0.f, 1.f);
        ImGui::SliderFloat("Saturation Variation", &lp.saturationVariation, 0.f, 1.f);
        ImGui::SliderFloat("Value Variation", &lp.valueVariation, 0.f, 1.f);
        ImGui::SliderFloat("Bump Strength", &lp.bumpStrength, 0.f, 1.f);
        ImGui::SliderFloat("Brush Grid", &lp.brushGrid, 1.f, 16.f);
        ImGui::SliderFloat("Canvas Strength", &lp.canvasStrength, 0.f, 1.f);
        ImGui::SliderFloat("Normal Scale", &lp.brushNormalScale, 0.f, 8.f);
        ImGui::InputInt("Baking", &lp.baking);
        lp.paintersColorFilter = std::clamp(lp.paintersColorFilter, 0, 9);
        lp.baking = std::clamp(lp.baking, 0, 2);
    }
}

void ImGuiLayer::DrawShadersPanel(AppState& app) {
    ImGui::TextWrapped("Save HLSL files to live-apply shaders.");
    ImGui::TextWrapped("Watching: %ls", app.shaderRoot.c_str());
    ImGui::TextWrapped("Normal modifier: shaders/normal_modifier.hlsl");
    ImGui::TextWrapped("Live paint core: shaders/live_paint_common.hlsl");
    if (ImGui::Button("Reload Shaders Now")) app.requestShaderReload = true;
    if (ImGui::Button("Open Shader Folder")) {
        ShellExecuteW(nullptr, L"explore", app.shaderRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (ImGui::Button("Open Export/Cache Folder")) {
        ShellExecuteW(nullptr, L"explore", kExportFolder, nullptr, nullptr, SW_SHOWNORMAL);
    }

    ImGui::Separator();
    if (app.shaders.HasValidShaders()) {
        ImGui::TextColored({0.3f, 1.f, 0.3f, 1.f}, "Shader status: OK");
        const auto tp = app.shaders.LastSuccessTime();
        const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm localTm{};
        localtime_s(&localTm, &tt);
        std::ostringstream oss;
        oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
        ImGui::Text("Last compile: %s", oss.str().c_str());
        if (!app.shaders.LastReloadScope().empty()) {
            ImGui::Text("Scope: %s", app.shaders.LastReloadScope().c_str());
        }
    } else {
        ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "Shader status: invalid / not compiled");
    }

    if (!app.shaders.LastError().empty()) {
        ImGui::Separator();
        ImGui::Text("Compile errors:");
        ImGui::BeginChild("Errors", ImVec2(0, 120), true);
        ImGui::TextWrapped("%s", app.shaders.LastError().c_str());
        ImGui::EndChild();
    }
}

void ImGuiLayer::DrawViewportOverlay(const AppState& app) {
    const bool splitNormalFlat = app.materialParams.view.presentation == ViewPresentation::Flat &&
        app.materialParams.view.mode == ViewMode::Normal;
    if (!splitNormalFlat) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float vpX = kSidebarWidth;
    const float vpW = vp->Size.x - vpX;
    const float halfW = vpW * 0.5f;
    const float midX = vpX + halfW;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine(
        ImVec2(midX, vp->WorkPos.y),
        ImVec2(midX, vp->WorkPos.y + vp->WorkSize.y),
        IM_COL32(255, 255, 255, 140), 2.f);
    dl->AddText(ImVec2(vpX + 10.f, vp->WorkPos.y + 10.f), IM_COL32(255, 255, 255, 230), "Original");
    dl->AddText(ImVec2(vpX + halfW + 10.f, vp->WorkPos.y + 10.f), IM_COL32(255, 255, 255, 230), "Modified");
}

void ImGuiLayer::BuildUI(AppState& app, GpuMesh& activeMesh, ID3D11Device* device) {
    if (!BeginLeftSidebar("##Sidebar")) {
        return;
    }

    ImGui::TextUnformatted("Shader Viewer");
    ImGui::Separator();

    if (ImGui::BeginTabBar("SidebarTabs")) {
        if (ImGui::BeginTabItem("Lighting", nullptr, TabSelectOnce(0, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 0;
            DrawLightingPanel(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("View", nullptr, TabSelectOnce(1, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 1;
            DrawViewPanel(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Assets", nullptr, TabSelectOnce(2, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 2;
            DrawAssetsPanel(app, activeMesh, device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Material", nullptr, TabSelectOnce(3, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 3;
            DrawMaterialPanel(app, device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Live Paint", nullptr, TabSelectOnce(4, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 4;
            DrawLivePaintPanel(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Shaders", nullptr, TabSelectOnce(5, app.selectSidebarTabOnce))) {
            app.activeSidebarTab = 5;
            DrawShadersPanel(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        app.selectSidebarTabOnce = -1;
    }

    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(-1.f, 0.f))) {
        if (UserSettings::Save(app)) {
            app.statusMessage = L"Saved settings to " + UserSettings::DefaultPath().wstring();
        } else {
            app.statusMessage = L"Failed to save settings.";
        }
    }
    if (!app.statusMessage.empty()) {
        ImGui::TextWrapped("%ls", app.statusMessage.c_str());
    }

    EndLeftSidebar();
    DrawViewportOverlay(app);
}

} // namespace sv
