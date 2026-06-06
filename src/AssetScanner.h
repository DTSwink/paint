#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace sv {

class AssetScanner {
public:
    void SetRoot(const std::wstring& root);
    void AddScanRoot(const std::wstring& root);
    void Rescan();

    const std::vector<MaterialSet>& MaterialSets() const { return materialSets_; }
    const std::vector<fs::path>& MeshFiles() const { return meshFiles_; }
    const std::vector<UAssetCandidate>& UAssetCandidates() const { return uassetCandidates_; }
    const std::wstring& Root() const { return root_; }

private:
    std::wstring root_;
    std::vector<std::wstring> extraRoots_;
    std::vector<MaterialSet> materialSets_;
    std::vector<fs::path> meshFiles_;
    std::vector<UAssetCandidate> uassetCandidates_;

    static bool ShouldSkip(const fs::path& path);
    static std::string StemKey(const fs::path& path);
    static int ClassifyTexture(const std::string& name);
};

} // namespace sv
