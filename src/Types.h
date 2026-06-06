#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace sv {

namespace fs = std::filesystem;

inline constexpr wchar_t kDefaultScanRoot[] = L"C:\\Users\\singerie\\Documents\\Cursor\\Berserk";
inline constexpr wchar_t kExportFolder[] = L"C:\\Users\\singerie\\Documents\\Cursor\\Berserk\\Saved\\ShaderViewerExports";

enum class TextureSlot : int {
    Albedo = 0,
    Normal = 1,
    Roughness = 2,
    Metallic = 3,
    AO = 4,
    Emissive = 5,
    Height = 6,
    PackedMask = 7,
    Count = 8
};

enum class PackedChannelMeaning : int {
    Unused = 0,
    AO = 1,
    Roughness = 2,
    Metallic = 3
};

enum class PreviewMeshKind {
    Suzanne,
    Sphere,
    Cube,
    Plane,
    Imported
};

enum class ViewMode {
    Render = 0,
    Normal = 1,
    UV = 2
};

enum class ViewPresentation {
    Mesh = 0,
    Flat = 1
};

enum class NormalViewSpace {
    Tangent = 0,
    World = 1,
    Geometry = 2
};

struct ViewParams {
    ViewMode mode = ViewMode::Render;
    ViewPresentation presentation = ViewPresentation::Mesh;
    NormalViewSpace normalSpace = NormalViewSpace::Tangent;
    float normalViewScale = 1.f;
    float uvCheckerScale = 8.f;
    bool uvShowAlbedo = true;
    bool uvShowWireframe = true;
    float flatViewPanX = 0.5f;
    float flatViewPanY = 0.5f;
    float flatViewZoom = 1.f;
};

struct Vertex {
    float position[3];
    float normal[3];
    float tangent[4]; // xyz = tangent, w = bitangent sign
    float uv[2];
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct MaterialSet {
    std::string name;
    fs::path albedo;
    fs::path normal;
    fs::path roughness;
    fs::path metallic;
    fs::path ao;
    fs::path emissive;
    fs::path height;
    fs::path packedMask;
    fs::path sourceMesh; // optional associated mesh
};

struct UAssetCandidate {
    std::string materialName;
    fs::path uassetPath;
};

} // namespace sv
