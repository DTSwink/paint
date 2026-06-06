#include "MeshLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

namespace sv {

bool MeshLoader::Load(const fs::path& path, LoadedMesh& out) {
    if (!fs::exists(path)) {
        std::wcerr << L"[MeshLoader] File not found: " << path.wstring() << std::endl;
        return false;
    }

    Assimp::Importer importer;
    const unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace |
                           aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality;
    const aiScene* scene = importer.ReadFile(path.string(), flags);
    if (!scene || !scene->HasMeshes()) {
        std::cerr << "[MeshLoader] Assimp failed: " << importer.GetErrorString() << std::endl;
        return false;
    }

    out.vertices.clear();
    out.indices.clear();
    out.path = path.wstring();

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(out.vertices.size());

        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v{};
            v.position[0] = mesh->mVertices[i].x;
            v.position[1] = mesh->mVertices[i].y;
            v.position[2] = mesh->mVertices[i].z;
            if (mesh->HasNormals()) {
                v.normal[0] = mesh->mNormals[i].x;
                v.normal[1] = mesh->mNormals[i].y;
                v.normal[2] = mesh->mNormals[i].z;
            }
            if (mesh->HasTangentsAndBitangents()) {
                v.tangent[0] = mesh->mTangents[i].x;
                v.tangent[1] = mesh->mTangents[i].y;
                v.tangent[2] = mesh->mTangents[i].z;
                const aiVector3D bit = mesh->mBitangents[i];
                const aiVector3D tan = mesh->mTangents[i];
                const aiVector3D n = mesh->mNormals[i];
                const aiVector3D cross = n ^ tan;
                v.tangent[3] = (cross * bit < 0.f) ? -1.f : 1.f;
            } else {
                v.tangent[0] = 1.f;
                v.tangent[1] = 0.f;
                v.tangent[2] = 0.f;
                v.tangent[3] = 1.f;
            }
            if (mesh->HasTextureCoords(0)) {
                v.uv[0] = mesh->mTextureCoords[0][i].x;
                v.uv[1] = mesh->mTextureCoords[0][i].y;
            }
            out.vertices.push_back(v);
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (unsigned k = 0; k < 3; ++k) {
                out.indices.push_back(base + face.mIndices[k]);
            }
        }
    }

    return !out.vertices.empty() && !out.indices.empty();
}

} // namespace sv
