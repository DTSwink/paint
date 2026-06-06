#pragma once

#include "ImGuiLayer.h"
#include "RendererD3D11.h"
#include <filesystem>

struct AppState;
namespace sv {
struct GpuMesh;
}

namespace sv {

class UserSettings {
public:
    static std::filesystem::path DefaultPath();
    static bool Load(AppState& app);
    static bool Save(const AppState& app);
    static void ApplySelection(AppState& app, GpuMesh& activeMesh, ID3D11Device* device);
};

} // namespace sv
