#include "AssetScanner.h"
#include "AppMesh.h"
#include "Camera.h"
#include "FileWatcher.h"
#include "ImGuiLayer.h"
#include "Material.h"
#include "MeshLoader.h"
#include "RendererD3D11.h"
#include "ShaderCompiler.h"
#include "Types.h"
#include "UserSettings.h"

#include "resource.h"

#include <Windows.h>

#include <backends/imgui_impl_win32.h>
#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include <DirectXMath.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <shellapi.h>

namespace {

constexpr wchar_t kWindowClass[] = L"ShaderViewerWindow";
constexpr int kDefaultWidth = 1600;
constexpr int kDefaultHeight = 900;

sv::RendererD3D11 g_renderer;
sv::ImGuiLayer g_imgui;
AppState g_app;
sv::GpuMesh g_activeMesh;
sv::FileWatcher g_shaderWatcher;
sv::FileWatcher g_shaderWatcherBuild;
std::chrono::steady_clock::time_point g_shaderReloadAfter{};
HWND g_hwnd = nullptr;
int g_width = kDefaultWidth;
int g_height = kDefaultHeight;
bool g_running = true;
bool g_rendererReady = false;
std::wstring g_proofCapturePath;
int g_proofFramesRemaining = -1;

std::wstring ParseProofCapturePath() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return {};
    std::wstring path;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg.rfind(L"--proof-capture=", 0) == 0) {
            path = arg.substr(16);
        }
    }
    LocalFree(argv);
    return path;
}

std::filesystem::path GetExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path ResolveShaderRoot() {
    const auto exeDir = GetExecutableDirectory();
    const auto nextToExe = exeDir / L"shaders";
    if (std::filesystem::exists(nextToExe / L"material_ps.hlsl")) {
        return nextToExe;
    }
    const auto projectShaders = exeDir.parent_path().parent_path().parent_path() / "shaders";
    if (std::filesystem::exists(projectShaders / L"material_ps.hlsl")) {
        return projectShaders;
    }
    if (std::filesystem::exists(nextToExe)) return nextToExe;
    return std::filesystem::current_path() / L"shaders";
}

void ReloadShaders(bool forceAll = false) {
    if (g_app.shaders.ReloadChanged(g_renderer.Device(), forceAll)) {
        const std::string& scope = g_app.shaders.LastReloadScope();
        g_app.statusMessage = L"Shader live reload OK (" +
            std::wstring(scope.begin(), scope.end()) + L").";
    } else if (g_app.shaders.HasValidShaders()) {
        g_app.statusMessage = L"Shader compile failed; keeping last valid shader.";
    } else {
        g_app.statusMessage = L"Shader compile failed.";
    }
}

void QueueShaderReload() {
    g_shaderReloadAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
}

void RescanAssets() {
    const std::wstring root(g_app.scanRoot.begin(), g_app.scanRoot.end());
    g_app.scanner.SetRoot(root);
    g_app.scanner.Rescan();
    g_app.statusMessage = L"Scan complete. Material sets: " + std::to_wstring(g_app.scanner.MaterialSets().size());
}

void SetupProofState() {
    const auto assetsRoot = sv::ResolveAssetsRoot();
    const auto testMaterialDir = assetsRoot / "test_material";
    g_app.scanRoot = testMaterialDir.string();
    std::snprintf(g_app.scanRootEdit, sizeof(g_app.scanRootEdit), "%s", g_app.scanRoot.c_str());
    g_app.scanner.SetRoot(testMaterialDir.wstring());
    g_app.scanner.AddScanRoot(assetsRoot.wstring());
    RescanAssets();

    for (int i = 0; i < static_cast<int>(g_app.scanner.MaterialSets().size()); ++i) {
        if (!g_app.scanner.MaterialSets()[static_cast<size_t>(i)].normal.empty()) {
            g_app.selectedMaterial = i;
            g_app.material.LoadFromSet(g_renderer.Device(), g_app.scanner.MaterialSets()[static_cast<size_t>(i)]);
            break;
        }
    }

    sv::LoadDefaultMesh(g_activeMesh, g_app, g_renderer.Device());

    auto& view = g_app.materialParams.view;
    view.mode = sv::ViewMode::Normal;
    view.presentation = sv::ViewPresentation::Flat;
    view.normalSpace = sv::NormalViewSpace::World;
    view.flatViewPanX = 0.5f;
    view.flatViewPanY = 0.5f;
    view.flatViewZoom = 0.66f;

    g_app.livePaint.enabled = true;
    g_app.livePaint.bumpStrength = 0.35f;
    g_app.livePaint.brushNormalScale = 1.f;
    g_app.livePaint.filterStrength = 0.95f;
    g_app.livePaint.strokeDensity = 1.75f;
    g_app.livePaint.minStrokeScale = 1.6f;
    g_app.livePaint.maxStrokeScale = 3.75f;
    g_app.livePaint.brushGrid = 8.f;
    g_app.livePaint.minStrokeOpacity = 1.f;
    g_app.livePaint.maxStrokeOpacity = 0.5f;
    g_app.livePaint.opacityThreshold = 5.f;
    g_app.livePaint.hueVariation = 0.12f;
    g_app.livePaint.saturationVariation = 0.16f;
    g_app.livePaint.valueVariation = 0.08f;
}

void ResetActiveMesh() {
    sv::LoadDefaultMesh(g_activeMesh, g_app, g_renderer.Device());
}

void NormalizeLightDirection() {
    using namespace DirectX;
    XMVECTOR v = XMLoadFloat3(&g_app.lightDirection);
    v = XMVector3Normalize(v);
    XMStoreFloat3(&g_app.lightDirection, v);
}

bool IsFlatPreviewActive() {
    const auto& view = g_app.materialParams.view;
    return view.presentation == sv::ViewPresentation::Flat &&
        (view.mode == sv::ViewMode::Normal || view.mode == sv::ViewMode::UV);
}

float RenderViewportWidth() {
    return static_cast<float>(std::max(1, g_width)) - sv::ImGuiLayer::kSidebarWidth;
}

float RenderViewportHeight() {
    return static_cast<float>(std::max(1, g_height));
}

float RenderViewportAspect() {
    return RenderViewportWidth() / RenderViewportHeight();
}

float FlatViewHalfHeight() {
    return 0.5f / std::max(g_app.materialParams.view.flatViewZoom, 0.05f);
}

float FlatViewHalfWidth() {
    return FlatViewHalfHeight() * RenderViewportAspect();
}

void PanFlatView(float dx, float dy) {
    auto& view = g_app.materialParams.view;
    const float viewportW = RenderViewportWidth();
    const float viewportH = RenderViewportHeight();
    const float halfW = FlatViewHalfWidth();
    const float halfH = FlatViewHalfHeight();
    view.flatViewPanX -= dx * (2.f * halfW / viewportW);
    view.flatViewPanY -= dy * (2.f * halfH / viewportH);
}

void ZoomFlatView(float delta) {
    auto& view = g_app.materialParams.view;
    const float factor = std::pow(1.15f, delta);
    view.flatViewZoom = std::clamp(view.flatViewZoom * factor, 0.05f, 64.f);
}

void ResetFlatView() {
    auto& view = g_app.materialParams.view;
    view.flatViewPanX = 0.5f;
    view.flatViewPanY = 0.5f;
    view.flatViewZoom = 1.f;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_rendererReady && wParam != SIZE_MINIMIZED) {
            g_width = LOWORD(lParam);
            g_height = HIWORD(lParam);
            g_renderer.Resize(g_width, g_height);
            g_app.camera.SetAspect(RenderViewportAspect());
        }
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_LBUTTONDOWN:
        g_app.orbitDragging = true;
        g_app.lastMouse = {static_cast<float>(LOWORD(lParam)), static_cast<float>(HIWORD(lParam))};
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        g_app.orbitDragging = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        g_app.panDragging = true;
        g_app.lastMouse = {static_cast<float>(LOWORD(lParam)), static_cast<float>(HIWORD(lParam))};
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONUP:
        g_app.panDragging = false;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE: {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return 0;
        if (LOWORD(lParam) < static_cast<WORD>(sv::ImGuiLayer::kSidebarWidth)) return 0;
        const float x = static_cast<float>(LOWORD(lParam));
        const float y = static_cast<float>(HIWORD(lParam));
        const float dx = x - g_app.lastMouse.x;
        const float dy = y - g_app.lastMouse.y;
        if (IsFlatPreviewActive()) {
            if (g_app.orbitDragging || g_app.panDragging) {
                PanFlatView(dx, dy);
            }
        } else if (g_app.orbitDragging) {
            g_app.camera.Orbit(dx * 0.01f, -dy * 0.01f);
        } else if (g_app.panDragging) {
            g_app.camera.Pan(-dx * 0.005f, dy * 0.005f);
        }
        g_app.lastMouse = {x, y};
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureMouse) {
            const float delta = GET_WHEEL_DELTA_WPARAM(wParam) / 120.f;
            if (IsFlatPreviewActive()) {
                ZoomFlatView(delta);
            } else {
                g_app.camera.Zoom(-delta * 0.25f);
            }
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool CreateAppWindow(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    RECT rect{0, 0, g_width, g_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExW(
        0, kWindowClass, L"Shader Viewer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, nullptr);
    return g_hwnd != nullptr;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int showCommand) {
    g_proofCapturePath = ParseProofCapturePath();
    const bool proofMode = !g_proofCapturePath.empty();

    if (!CreateAppWindow(hInstance)) return 1;

    if (!g_renderer.Initialize(g_hwnd, g_width, g_height)) return 1;
    if (!g_imgui.Initialize(g_hwnd, g_renderer.Device(), g_renderer.Context())) return 1;
    g_rendererReady = true;

    ShowWindow(g_hwnd, showCommand);
    UpdateWindow(g_hwnd);

    g_app.camera.SetAspect(RenderViewportAspect());
    g_app.material.SetDevice(g_renderer.Device());
    g_app.livePaintAssets.Initialize(g_renderer.Device());
    sv::LoadDefaultMesh(g_activeMesh, g_app, g_renderer.Device());
    NormalizeLightDirection();

    const auto shaderRoot = ResolveShaderRoot();
    g_app.shaderRoot = shaderRoot.wstring();
    g_app.shaders.SetShaderRoot(shaderRoot);
    ReloadShaders(true);

    if (proofMode) {
        SetupProofState();
        g_proofFramesRemaining = 20;
    } else {
        g_shaderWatcher.Start(shaderRoot, [] {});
        const auto buildShaders = GetExecutableDirectory() / L"shaders";
        if (buildShaders != shaderRoot && std::filesystem::exists(buildShaders)) {
            g_shaderWatcherBuild.Start(buildShaders, [] {});
        }

        g_app.scanner.AddScanRoot(sv::ResolveAssetsRoot().wstring());

        if (sv::UserSettings::Load(g_app)) {
            g_app.requestApplySavedSettings = true;
        }
    }

    auto lastTime = std::chrono::steady_clock::now();
    MSG msg{};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_app.requestRescan) {
            g_app.requestRescan = false;
            RescanAssets();
            if (g_app.requestApplySavedSettings) {
                g_app.requestApplySavedSettings = false;
                sv::UserSettings::ApplySelection(g_app, g_activeMesh, g_renderer.Device());
                g_app.statusMessage = L"Loaded settings from " + sv::UserSettings::DefaultPath().wstring();
            }
        }
        if (g_app.requestShaderReload) {
            g_app.requestShaderReload = false;
            ReloadShaders(true);
        } else if (g_shaderWatcher.ConsumePending() || g_shaderWatcherBuild.ConsumePending()) {
            QueueShaderReload();
        }
        if (g_shaderReloadAfter != std::chrono::steady_clock::time_point{} &&
            std::chrono::steady_clock::now() >= g_shaderReloadAfter) {
            g_shaderReloadAfter = {};
            ReloadShaders(false);
        }
        if (g_app.requestResetCamera) {
            g_app.requestResetCamera = false;
            g_app.camera.FocusOn(g_app.meshFocusCenter, g_app.meshFocusRadius);
        }
        if (g_app.requestResetFlatView) {
            g_app.requestResetFlatView = false;
            ResetFlatView();
        }

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        g_app.time += dt;

        g_renderer.BeginFrame();

        const DirectX::XMFLOAT3 lightDir = g_app.cameraRelativeLight
            ? g_app.camera.ComputeKeyLightDirection(g_app.lightPitchOffset, g_app.lightSideOffset)
            : g_app.lightDirection;

        g_renderer.Render(
            g_app.camera, g_app.time, lightDir, g_app.lightIntensity,
            g_app.lightColor, g_app.exposure, g_app.ambientIntensity, g_app.lightWrap,
            g_app.scatterStrength, g_app.material, g_app.materialParams,
            g_app.livePaint, g_app.livePaintAssets,
            g_activeMesh, g_app.shaders);

        g_imgui.NewFrame();
        g_imgui.BuildUI(g_app, g_activeMesh, g_renderer.Device());
        g_imgui.Render(g_renderer.Context());
        g_renderer.Present();

        if (g_proofFramesRemaining >= 0) {
            if (--g_proofFramesRemaining == 0) {
                const auto capturePath = std::filesystem::path(g_proofCapturePath);
                if (g_renderer.CaptureBackbuffer(capturePath)) {
                    std::wcout << L"Proof capture saved: " << capturePath.wstring() << std::endl;
                } else {
                    std::wcerr << L"Proof capture failed." << std::endl;
                    return 2;
                }
                g_running = false;
            }
        }
    }

    if (!proofMode) {
        g_shaderWatcher.Stop();
        g_shaderWatcherBuild.Stop();
    }
    g_imgui.Shutdown();
    return 0;
}
