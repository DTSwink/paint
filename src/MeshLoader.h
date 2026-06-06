#pragma once

#include "Types.h"
#include <vector>
#include <filesystem>

namespace sv {

struct LoadedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::wstring path;
};

class MeshLoader {
public:
    static bool Load(const std::filesystem::path& path, LoadedMesh& out);
};

} // namespace sv
