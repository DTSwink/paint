#include "AppMesh.h"
#include "ImGuiLayer.h"
#include "MeshGen.h"
#include "MeshLoader.h"
#include "RendererD3D11.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <algorithm>
#include <cfloat>

namespace sv {

std::filesystem::path ResolveAssetsRoot() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const auto exeDir = std::filesystem::path(path).parent_path();
    const auto nextToExe = exeDir / "assets";
    if (std::filesystem::exists(nextToExe)) return nextToExe;
    const auto projectAssets = exeDir.parent_path().parent_path().parent_path() / "assets";
    if (std::filesystem::exists(projectAssets)) return projectAssets;
    return nextToExe;
}

std::filesystem::path ResolveSuzannePath() {
    return ResolveAssetsRoot() / "test_mesh" / "suzanne.obj";
}

void FocusCameraOnMesh(sv::Camera& camera, const std::vector<sv::Vertex>& vertices, DirectX::XMFLOAT3& outCenter,
    float& outRadius) {
    if (vertices.empty()) {
        outCenter = {0.f, 0.f, 0.f};
        outRadius = 1.f;
        camera.Reset();
        return;
    }

    DirectX::XMFLOAT3 minP{FLT_MAX, FLT_MAX, FLT_MAX};
    DirectX::XMFLOAT3 maxP{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const auto& v : vertices) {
        minP.x = std::min(minP.x, v.position[0]);
        minP.y = std::min(minP.y, v.position[1]);
        minP.z = std::min(minP.z, v.position[2]);
        maxP.x = std::max(maxP.x, v.position[0]);
        maxP.y = std::max(maxP.y, v.position[1]);
        maxP.z = std::max(maxP.z, v.position[2]);
    }

    outCenter = {
        (minP.x + maxP.x) * 0.5f,
        (minP.y + maxP.y) * 0.5f,
        (minP.z + maxP.z) * 0.5f};
    outRadius = 0.f;
    for (const auto& v : vertices) {
        const float dx = v.position[0] - outCenter.x;
        const float dy = v.position[1] - outCenter.y;
        const float dz = v.position[2] - outCenter.z;
        outRadius = std::max(outRadius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    camera.FocusOn(outCenter, outRadius);
}

bool LoadMeshFromPath(
    const std::filesystem::path& path, PreviewMeshKind kind, GpuMesh& outMesh, AppState& app, ID3D11Device* device) {
    LoadedMesh loaded;
    if (!MeshLoader::Load(path, loaded)) {
        return false;
    }
    outMesh = RendererD3D11::UploadMesh(device, loaded.vertices, loaded.indices);
    app.meshKind = kind;
    app.selectedMesh = -1;
    FocusCameraOnMesh(app.camera, loaded.vertices, app.meshFocusCenter, app.meshFocusRadius);
    app.statusMessage = L"Loaded mesh: " + path.filename().wstring();
    return true;
}

bool LoadStartupMesh(GpuMesh& outMesh, AppState& app, ID3D11Device* device) {
    const MeshData sphere = MeshGen::CreateSphere();
    outMesh = RendererD3D11::UploadMesh(device, sphere.vertices, sphere.indices);
    app.meshKind = PreviewMeshKind::Sphere;
    app.selectedMesh = -1;
    FocusCameraOnMesh(app.camera, sphere.vertices, app.meshFocusCenter, app.meshFocusRadius);
    return true;
}

bool LoadDefaultMesh(GpuMesh& outMesh, AppState& app, ID3D11Device* device) {
    if (LoadMeshFromPath(ResolveSuzannePath(), PreviewMeshKind::Suzanne, outMesh, app, device)) {
        return true;
    }
    const MeshData sphere = MeshGen::CreateSphere();
    outMesh = RendererD3D11::UploadMesh(device, sphere.vertices, sphere.indices);
    app.meshKind = PreviewMeshKind::Sphere;
    app.selectedMesh = -1;
    FocusCameraOnMesh(app.camera, sphere.vertices, app.meshFocusCenter, app.meshFocusRadius);
    app.statusMessage = L"Suzanne not found; using default sphere.";
    return false;
}

} // namespace sv
