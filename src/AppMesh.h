#pragma once

#include "RendererD3D11.h"
#include "Types.h"
#include <filesystem>

struct AppState;

namespace sv {

std::filesystem::path ResolveAssetsRoot();
std::filesystem::path ResolveSuzannePath();
void FocusCameraOnMesh(Camera& camera, const std::vector<Vertex>& vertices, DirectX::XMFLOAT3& outCenter,
    float& outRadius);
bool LoadMeshFromPath(const std::filesystem::path& path, PreviewMeshKind kind, GpuMesh& outMesh, AppState& app,
                      ID3D11Device* device);
bool LoadDefaultMesh(GpuMesh& outMesh, AppState& app, ID3D11Device* device);

} // namespace sv
