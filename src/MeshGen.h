#pragma once

#include "Types.h"
#include <vector>

namespace sv {

class MeshGen {
public:
    static MeshData CreateSphere(int slices = 32, int stacks = 16, float radius = 1.f);
    static MeshData CreateCube(float halfExtent = 0.5f);
    static MeshData CreatePlane(float halfSize = 1.f, int subdivisions = 1);
};

} // namespace sv
