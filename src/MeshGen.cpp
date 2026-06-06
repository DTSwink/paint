#include "MeshGen.h"
#include <DirectXMath.h>
#include <cmath>

namespace sv {

using namespace DirectX;

static Vertex MakeVertex(const XMFLOAT3& pos, const XMFLOAT3& normal, const XMFLOAT2& uv) {
    Vertex v{};
    v.position[0] = pos.x;
    v.position[1] = pos.y;
    v.position[2] = pos.z;
    v.normal[0] = normal.x;
    v.normal[1] = normal.y;
    v.normal[2] = normal.z;
    v.tangent[0] = 1.f;
    v.tangent[1] = 0.f;
    v.tangent[2] = 0.f;
    v.tangent[3] = 1.f;
    v.uv[0] = uv.x;
    v.uv[1] = uv.y;
    return v;
}

static void ComputeTangents(std::vector<Vertex>& verts, const std::vector<uint32_t>& indices) {
    std::vector<XMFLOAT3> tanAccum(verts.size(), {0, 0, 0});
    std::vector<XMFLOAT3> bitanAccum(verts.size(), {0, 0, 0});

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vertex& v0 = verts[indices[i]];
        const Vertex& v1 = verts[indices[i + 1]];
        const Vertex& v2 = verts[indices[i + 2]];

        const XMVECTOR p0 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(v0.position));
        const XMVECTOR p1 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(v1.position));
        const XMVECTOR p2 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(v2.position));

        const XMVECTOR dp1 = XMVectorSubtract(p1, p0);
        const XMVECTOR dp2 = XMVectorSubtract(p2, p0);

        const float du1 = v1.uv[0] - v0.uv[0];
        const float dv1 = v1.uv[1] - v0.uv[1];
        const float du2 = v2.uv[0] - v0.uv[0];
        const float dv2 = v2.uv[1] - v0.uv[1];

        const float r = 1.f / (du1 * dv2 - du2 * dv1 + 1e-8f);
        const XMVECTOR tangent = XMVectorScale(
            XMVectorSubtract(XMVectorScale(dp1, dv2), XMVectorScale(dp2, dv1)), r);
        const XMVECTOR bitangent = XMVectorScale(
            XMVectorSubtract(XMVectorScale(dp2, du1), XMVectorScale(dp1, du2)), r);

        for (uint32_t idx : {indices[i], indices[i + 1], indices[i + 2]}) {
            XMFLOAT3 t, b;
            XMStoreFloat3(&t, XMVectorAdd(XMLoadFloat3(&tanAccum[idx]), tangent));
            XMStoreFloat3(&b, XMVectorAdd(XMLoadFloat3(&bitanAccum[idx]), bitangent));
            tanAccum[idx] = t;
            bitanAccum[idx] = b;
        }
    }

    for (size_t i = 0; i < verts.size(); ++i) {
        const XMVECTOR n = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(verts[i].normal));
        const XMVECTOR t = XMVector3Normalize(XMVectorSubtract(
            XMLoadFloat3(&tanAccum[i]), XMVectorScale(n, XMVectorGetX(XMVector3Dot(n, XMLoadFloat3(&tanAccum[i]))))));
        const XMVECTOR b = XMVector3Cross(n, t);
        const float sign = (XMVectorGetX(XMVector3Dot(b, XMLoadFloat3(&bitanAccum[i]))) < 0.f) ? -1.f : 1.f;
        XMFLOAT3 tf;
        XMStoreFloat3(&tf, t);
        verts[i].tangent[0] = tf.x;
        verts[i].tangent[1] = tf.y;
        verts[i].tangent[2] = tf.z;
        verts[i].tangent[3] = sign;
    }
}

MeshData MeshGen::CreateSphere(int slices, int stacks, float radius) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    for (int y = 0; y <= stacks; ++y) {
        const float v = static_cast<float>(y) / stacks;
        const float phi = v * XM_PI;
        for (int x = 0; x <= slices; ++x) {
            const float u = static_cast<float>(x) / slices;
            const float theta = u * XM_2PI;
            const float sinPhi = std::sinf(phi);
            const XMFLOAT3 pos{
                radius * sinPhi * std::cosf(theta),
                radius * std::cosf(phi),
                radius * sinPhi * std::sinf(theta)};
            const XMFLOAT3 normal{pos.x / radius, pos.y / radius, pos.z / radius};
            verts.push_back(MakeVertex(pos, normal, {u, v}));
        }
    }

    for (int y = 0; y < stacks; ++y) {
        for (int x = 0; x < slices; ++x) {
            const uint32_t i0 = y * (slices + 1) + x;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + static_cast<uint32_t>(slices + 1);
            const uint32_t i3 = i2 + 1;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    ComputeTangents(verts, indices);
    return {std::move(verts), std::move(indices)};
}

MeshData MeshGen::CreateCube(float halfExtent) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    struct Face {
        XMFLOAT3 n;
        XMFLOAT3 t;
        float sign;
        int axis;
    };
    const Face faces[] = {
        {{0, 0, 1}, {1, 0, 0}, 1.f, 2},
        {{0, 0, -1}, {-1, 0, 0}, 1.f, 2},
        {{1, 0, 0}, {0, 0, -1}, 1.f, 0},
        {{-1, 0, 0}, {0, 0, 1}, 1.f, 0},
        {{0, 1, 0}, {1, 0, 0}, 1.f, 1},
        {{0, -1, 0}, {1, 0, 0}, -1.f, 1},
    };

    for (const Face& f : faces) {
        const uint32_t base = static_cast<uint32_t>(verts.size());
        const XMFLOAT3 corners[4] = {
            {}, {}, {}, {}
        };
        for (int i = 0; i < 4; ++i) {
            float px = 0, py = 0, pz = 0;
            const float sx = (i % 2 == 0) ? -1.f : 1.f;
            const float sy = (i / 2 == 0) ? -1.f : 1.f;
            if (f.axis == 0) {
                px = f.n.x * halfExtent;
                py = sy * halfExtent;
                pz = sx * halfExtent;
            } else if (f.axis == 1) {
                px = sx * halfExtent;
                py = f.n.y * halfExtent;
                pz = sy * halfExtent;
            } else {
                px = sx * halfExtent;
                py = sy * halfExtent;
                pz = f.n.z * halfExtent;
            }
            const XMFLOAT2 uv{static_cast<float>(i % 2), static_cast<float>(i / 2)};
            verts.push_back(MakeVertex({px, py, pz}, f.n, uv));
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 1, base + 3});
        for (uint32_t k = 0; k < 4; ++k) {
            verts[base + k].tangent[0] = f.t.x;
            verts[base + k].tangent[1] = f.t.y;
            verts[base + k].tangent[2] = f.t.z;
            verts[base + k].tangent[3] = f.sign;
        }
    }

    return {std::move(verts), std::move(indices)};
}

MeshData MeshGen::CreatePlane(float halfSize, int subdivisions) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    const int n = std::max(1, subdivisions);
    for (int y = 0; y <= n; ++y) {
        for (int x = 0; x <= n; ++x) {
            const float u = static_cast<float>(x) / n;
            const float v = static_cast<float>(y) / n;
            const XMFLOAT3 pos{(u * 2.f - 1.f) * halfSize, 0.f, (v * 2.f - 1.f) * halfSize};
            const XMFLOAT3 normal{0.f, 1.f, 0.f};
            verts.push_back(MakeVertex(pos, normal, {u, v}));
        }
    }
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const uint32_t i0 = y * (n + 1) + x;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + static_cast<uint32_t>(n + 1);
            const uint32_t i3 = i2 + 1;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    ComputeTangents(verts, indices);
    return {std::move(verts), std::move(indices)};
}

} // namespace sv
