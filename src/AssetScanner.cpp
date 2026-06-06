#include "AssetScanner.h"
#include <cstring>
#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>

namespace sv {

namespace {

bool EndsWithIgnoreCase(const std::string& haystack, const std::string& suffix) {
    if (suffix.size() > haystack.size()) return false;
    const size_t off = haystack.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(haystack[off + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
    auto lower = haystack;
    auto n = needle;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower.find(n) != std::string::npos;
}

bool IsTextureExt(const fs::path& p) {
    static const wchar_t* exts[] = {
        L".png", L".jpg", L".jpeg", L".tga", L".tif", L".tiff", L".bmp", L".dds", L".exr", L".hdr"
    };
    const auto ext = p.extension().wstring();
    for (const auto* e : exts) {
        if (_wcsicmp(ext.c_str(), e) == 0) return true;
    }
    return false;
}

bool IsMeshExt(const fs::path& p) {
    static const wchar_t* exts[] = {L".obj", L".fbx", L".gltf", L".glb"};
    const auto ext = p.extension().wstring();
    for (const auto* e : exts) {
        if (_wcsicmp(ext.c_str(), e) == 0) return true;
    }
    return false;
}

} // namespace

void AssetScanner::SetRoot(const std::wstring& root) {
    root_ = root;
}

void AssetScanner::AddScanRoot(const std::wstring& root) {
    if (root.empty()) return;
    if (std::find(extraRoots_.begin(), extraRoots_.end(), root) == extraRoots_.end()) {
        extraRoots_.push_back(root);
    }
}

bool AssetScanner::ShouldSkip(const fs::path& path) {
    static const wchar_t* skip[] = {
        L"Binaries", L"Intermediate", L"DerivedDataCache", L".git", L"Build"
    };
    const auto name = path.filename().wstring();
    for (const auto* s : skip) {
        if (_wcsicmp(name.c_str(), s) == 0) return true;
    }
    return false;
}

std::string AssetScanner::StemKey(const fs::path& path) {
    std::string stem = path.stem().string();
    static const char* suffixes[] = {
        "_BaseColor", "_Base_Color", "_Basecolour", "_Albedo", "_Diffuse", "_Diff", "_Color", "_Colour",
        "_D", "_BC", "_Base", "_Normal", "_Norm", "_Nrm", "_N", "_NM", "_TangentNormal",
        "_Roughness", "_Rough", "_R", "_Metallic", "_Metalness", "_Metal", "_M",
        "_AO", "_Occlusion", "_AmbientOcclusion", "_O", "_Occ",
        "_Emissive", "_Emission", "_Glow", "_E",
        "_Height", "_Displacement", "_Disp", "_Bump", "_H",
        "_ORM", "_RMA", "_MRA", "_Mask", "_Packed"
    };
    for (const char* s : suffixes) {
        if (EndsWithIgnoreCase(stem, s)) {
            stem.resize(stem.size() - strlen(s));
            break;
        }
    }
    return stem;
}

int AssetScanner::ClassifyTexture(const std::string& name) {
    if (ContainsIgnoreCase(name, "basecolor") || ContainsIgnoreCase(name, "base_color") ||
        ContainsIgnoreCase(name, "basecolour") || ContainsIgnoreCase(name, "albedo") ||
        ContainsIgnoreCase(name, "diffuse") || ContainsIgnoreCase(name, "colour") ||
        EndsWithIgnoreCase(name, "_d") || EndsWithIgnoreCase(name, "_bc") || EndsWithIgnoreCase(name, "_base") ||
        (ContainsIgnoreCase(name, "color") && !ContainsIgnoreCase(name, "normal"))) {
        return 0;
    }
    if (ContainsIgnoreCase(name, "normal") || ContainsIgnoreCase(name, "nrm") ||
        ContainsIgnoreCase(name, "tangentnormal") || EndsWithIgnoreCase(name, "_n") ||
        EndsWithIgnoreCase(name, "_nm")) {
        return 1;
    }
    if (ContainsIgnoreCase(name, "roughness") || ContainsIgnoreCase(name, "rough") || EndsWithIgnoreCase(name, "_r")) {
        return 2;
    }
    if (ContainsIgnoreCase(name, "metallic") || ContainsIgnoreCase(name, "metalness") ||
        ContainsIgnoreCase(name, "metal") || EndsWithIgnoreCase(name, "_m")) {
        return 3;
    }
    if (ContainsIgnoreCase(name, "ambientocclusion") || ContainsIgnoreCase(name, "occlusion") ||
        EndsWithIgnoreCase(name, "_ao") || EndsWithIgnoreCase(name, "_o") || EndsWithIgnoreCase(name, "_occ") ||
        (ContainsIgnoreCase(name, "ao") && !ContainsIgnoreCase(name, "albedo"))) {
        return 4;
    }
    if (ContainsIgnoreCase(name, "emissive") || ContainsIgnoreCase(name, "emission") ||
        ContainsIgnoreCase(name, "glow") || EndsWithIgnoreCase(name, "_e")) {
        return 5;
    }
    if (ContainsIgnoreCase(name, "height") || ContainsIgnoreCase(name, "displacement") ||
        ContainsIgnoreCase(name, "disp") || ContainsIgnoreCase(name, "bump") || EndsWithIgnoreCase(name, "_h")) {
        return 6;
    }
    if (ContainsIgnoreCase(name, "orm") || ContainsIgnoreCase(name, "rma") || ContainsIgnoreCase(name, "mra") ||
        ContainsIgnoreCase(name, "packed") || ContainsIgnoreCase(name, "mask")) {
        return 7;
    }
    return -1;
}

void AssetScanner::Rescan() {
    materialSets_.clear();
    meshFiles_.clear();
    uassetCandidates_.clear();

    std::vector<std::wstring> roots;
    if (!root_.empty()) roots.push_back(root_);
    for (const auto& extra : extraRoots_) {
        if (!extra.empty()) roots.push_back(extra);
    }
    if (roots.empty()) return;

    std::unordered_map<std::string, MaterialSet> grouped;

    for (const std::wstring& scanRoot : roots) {
        if (!fs::exists(scanRoot)) continue;

        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(scanRoot, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); ++it) {
        if (ShouldSkip(it->path())) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file()) continue;

        const fs::path p = it->path();
        if (IsMeshExt(p)) {
            meshFiles_.push_back(p);
            continue;
        }

        if (p.extension() == ".uasset") {
            const std::string stem = p.stem().string();
            if (ClassifyTexture(stem) >= 0 || ContainsIgnoreCase(stem, "texture")) {
                UAssetCandidate c;
                c.uassetPath = p;
                c.materialName = StemKey(p);
                uassetCandidates_.push_back(c);
            }
            continue;
        }

        if (!IsTextureExt(p)) continue;

        const std::string stem = p.stem().string();
        const int kind = ClassifyTexture(stem);
        const std::string key = StemKey(p);
        MaterialSet& set = grouped[key.empty() ? stem : key];
        if (set.name.empty()) set.name = key.empty() ? stem : key;

        switch (kind) {
        case 0: if (set.albedo.empty()) set.albedo = p; break;
        case 1: if (set.normal.empty()) set.normal = p; break;
        case 2: if (set.roughness.empty()) set.roughness = p; break;
        case 3: if (set.metallic.empty()) set.metallic = p; break;
        case 4: if (set.ao.empty()) set.ao = p; break;
        case 5: if (set.emissive.empty()) set.emissive = p; break;
        case 6: if (set.height.empty()) set.height = p; break;
        case 7: if (set.packedMask.empty()) set.packedMask = p; break;
        default: break;
        }
        }
    }

    materialSets_.reserve(grouped.size());
    for (auto& [_, set] : grouped) {
        if (set.albedo.empty() && set.normal.empty() && set.roughness.empty() && set.metallic.empty() &&
            set.ao.empty() && set.emissive.empty() && set.height.empty() && set.packedMask.empty()) {
            continue;
        }
        materialSets_.push_back(std::move(set));
    }

    std::sort(materialSets_.begin(), materialSets_.end(),
              [](const MaterialSet& a, const MaterialSet& b) { return a.name < b.name; });
    std::sort(meshFiles_.begin(), meshFiles_.end());
}

} // namespace sv
