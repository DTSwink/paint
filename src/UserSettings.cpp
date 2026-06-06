#include "UserSettings.h"

#include "AppMesh.h"
#include "MeshGen.h"
#include "MeshLoader.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sv {

namespace {

std::filesystem::path GetExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

bool ParseBool(const std::string& value) {
    return value == "1" || value == "true" || value == "True";
}

int ParseInt(const std::string& value, int fallback) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

float ParseFloat(const std::string& value, float fallback) {
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

std::filesystem::path ResolveSettingsPath() {
    const auto exeDir = GetExecutableDirectory();
    const auto localSettingsDir = exeDir / "settings";
    const auto localSettings = localSettingsDir / "settings.ini";
    if (std::filesystem::exists(localSettings) || std::filesystem::exists(localSettingsDir)) {
        return localSettings;
    }
    const auto devSettingsDir = exeDir.parent_path().parent_path().parent_path() / "settings";
    if (std::filesystem::exists(devSettingsDir) || std::filesystem::exists(devSettingsDir.parent_path())) {
        return devSettingsDir / "settings.ini";
    }
    return exeDir / "settings.ini";
}

std::filesystem::path LegacyLightingPath() {
    const auto exeDir = GetExecutableDirectory();
    const auto settingsDir = exeDir.parent_path().parent_path().parent_path() / "settings";
    return settingsDir / "lighting.ini";
}

void ApplyKey(AppState& app, const std::string& key, const std::string& value) {
    auto& view = app.materialParams.view;
    auto& mat = app.materialParams;

    if (key == "lightIntensity") app.lightIntensity = ParseFloat(value, app.lightIntensity);
    else if (key == "lightColorR") app.lightColor.x = ParseFloat(value, app.lightColor.x);
    else if (key == "lightColorG") app.lightColor.y = ParseFloat(value, app.lightColor.y);
    else if (key == "lightColorB") app.lightColor.z = ParseFloat(value, app.lightColor.z);
    else if (key == "lightDirectionX") app.lightDirection.x = ParseFloat(value, app.lightDirection.x);
    else if (key == "lightDirectionY") app.lightDirection.y = ParseFloat(value, app.lightDirection.y);
    else if (key == "lightDirectionZ") app.lightDirection.z = ParseFloat(value, app.lightDirection.z);
    else if (key == "cameraRelativeLight") app.cameraRelativeLight = ParseBool(value);
    else if (key == "lightPitchOffset") app.lightPitchOffset = ParseFloat(value, app.lightPitchOffset);
    else if (key == "lightSideOffset") app.lightSideOffset = ParseFloat(value, app.lightSideOffset);
    else if (key == "ambientIntensity") app.ambientIntensity = ParseFloat(value, app.ambientIntensity);
    else if (key == "lightWrap") app.lightWrap = ParseFloat(value, app.lightWrap);
    else if (key == "scatterStrength") app.scatterStrength = ParseFloat(value, app.scatterStrength);
    else if (key == "exposure") app.exposure = ParseFloat(value, app.exposure);
    else if (key == "viewMode") view.mode = static_cast<ViewMode>(ParseInt(value, static_cast<int>(view.mode)));
    else if (key == "viewPresentation") view.presentation = static_cast<ViewPresentation>(ParseInt(value, static_cast<int>(view.presentation)));
    else if (key == "normalViewSpace") view.normalSpace = static_cast<NormalViewSpace>(ParseInt(value, static_cast<int>(view.normalSpace)));
    else if (key == "normalViewScale") view.normalViewScale = ParseFloat(value, view.normalViewScale);
    else if (key == "uvCheckerScale") view.uvCheckerScale = ParseFloat(value, view.uvCheckerScale);
    else if (key == "uvShowAlbedo") view.uvShowAlbedo = ParseBool(value);
    else if (key == "uvShowWireframe") view.uvShowWireframe = ParseBool(value);
    else if (key == "flatViewPanX") view.flatViewPanX = ParseFloat(value, view.flatViewPanX);
    else if (key == "flatViewPanY") view.flatViewPanY = ParseFloat(value, view.flatViewPanY);
    else if (key == "flatViewZoom") view.flatViewZoom = ParseFloat(value, view.flatViewZoom);
    else if (key == "applyModifierToRender") app.applyModifierToRender = ParseBool(value);
    else if (key == "activeSidebarTab") app.activeSidebarTab = ParseInt(value, app.activeSidebarTab);
    else if (key == "baseColorR") mat.baseColorFactor.x = ParseFloat(value, mat.baseColorFactor.x);
    else if (key == "baseColorG") mat.baseColorFactor.y = ParseFloat(value, mat.baseColorFactor.y);
    else if (key == "baseColorB") mat.baseColorFactor.z = ParseFloat(value, mat.baseColorFactor.z);
    else if (key == "baseColorA") mat.baseColorFactor.w = ParseFloat(value, mat.baseColorFactor.w);
    else if (key == "metallicFactor") mat.metallicFactor = ParseFloat(value, mat.metallicFactor);
    else if (key == "roughnessFactor") mat.roughnessFactor = ParseFloat(value, mat.roughnessFactor);
    else if (key == "aoFactor") mat.aoFactor = ParseFloat(value, mat.aoFactor);
    else if (key == "normalStrength") mat.normalStrength = ParseFloat(value, mat.normalStrength);
    else if (key == "emissiveStrength") mat.emissiveStrength = ParseFloat(value, mat.emissiveStrength);
    else if (key == "flipNormalGreen") mat.flipNormalGreen = ParseBool(value);
    else if (key == "packedR") mat.packedR = static_cast<PackedChannelMeaning>(ParseInt(value, static_cast<int>(mat.packedR)));
    else if (key == "packedG") mat.packedG = static_cast<PackedChannelMeaning>(ParseInt(value, static_cast<int>(mat.packedG)));
    else if (key == "packedB") mat.packedB = static_cast<PackedChannelMeaning>(ParseInt(value, static_cast<int>(mat.packedB)));
    else if (key == "scanRoot") {
        if (value.size() < sizeof(app.scanRootEdit)) {
            value.copy(app.scanRootEdit, value.size());
            app.scanRootEdit[value.size()] = '\0';
            app.scanRoot = value;
        }
    } else if (key == "savedMaterialName") app.savedMaterialName = value;
    else if (key == "savedMeshFile") app.savedMeshFile = value;
    else if (key == "savedMeshKind") app.savedMeshKind = ParseInt(value, app.savedMeshKind);
    else if (key == "savedSelectedMesh") app.savedSelectedMesh = ParseInt(value, app.savedSelectedMesh);
    else if (key == "cameraTargetX") app.savedCameraTarget.x = ParseFloat(value, app.savedCameraTarget.x);
    else if (key == "cameraTargetY") app.savedCameraTarget.y = ParseFloat(value, app.savedCameraTarget.y);
    else if (key == "cameraTargetZ") app.savedCameraTarget.z = ParseFloat(value, app.savedCameraTarget.z);
    else if (key == "cameraYaw") app.savedCameraYaw = ParseFloat(value, app.savedCameraYaw);
    else if (key == "cameraPitch") app.savedCameraPitch = ParseFloat(value, app.savedCameraPitch);
    else if (key == "cameraDistance") app.savedCameraDistance = ParseFloat(value, app.savedCameraDistance);
    else if (key == "restoreSavedCamera") app.restoreSavedCamera = ParseBool(value);
    else if (key.rfind("srgbOverride", 0) == 0) {
        const int slot = ParseInt(key.substr(12), -1);
        if (slot >= 0 && slot < static_cast<int>(TextureSlot::Count)) {
            app.material.SrgbOverride()[static_cast<size_t>(slot)] = ParseBool(value);
        }
    } else if (key == "lpEnabled") app.livePaint.enabled = ParseBool(value);
    else if (key == "lpPaintingThickness") app.livePaint.paintingThickness = ParseFloat(value, app.livePaint.paintingThickness);
    else if (key == "lpPadding") app.livePaint.padding = ParseFloat(value, app.livePaint.padding);
    else if (key == "lpCanvas") app.livePaint.canvas = ParseBool(value);
    else if (key == "lpHoldout") app.livePaint.holdout = ParseBool(value);
    else if (key == "lpTransparentBackground") app.livePaint.transparentBackground = ParseBool(value);
    else if (key == "lpStrokeDensity") app.livePaint.strokeDensity = ParseFloat(value, app.livePaint.strokeDensity);
    else if (key == "lpMinStrokeScale") app.livePaint.minStrokeScale = ParseFloat(value, app.livePaint.minStrokeScale);
    else if (key == "lpMaxStrokeScale") app.livePaint.maxStrokeScale = ParseFloat(value, app.livePaint.maxStrokeScale);
    else if (key == "lpScaleThreshold") app.livePaint.scaleThreshold = ParseFloat(value, app.livePaint.scaleThreshold);
    else if (key == "lpMinStrokeOpacity") app.livePaint.minStrokeOpacity = ParseFloat(value, app.livePaint.minStrokeOpacity);
    else if (key == "lpMaxStrokeOpacity") app.livePaint.maxStrokeOpacity = ParseFloat(value, app.livePaint.maxStrokeOpacity);
    else if (key == "lpOpacityThreshold") {
        app.livePaint.opacityThreshold = ParseFloat(value, app.livePaint.opacityThreshold);
        app.livePaint.opacityThreshold = std::clamp(app.livePaint.opacityThreshold, 1.f, 100.f);
    }
    else if (key == "lpStackDirection") app.livePaint.stackDirection = ParseBool(value);
    else if (key == "lpBrokenEdges") app.livePaint.brokenEdges = ParseBool(value);
    else if (key == "lpRandomSeedPerFrame") app.livePaint.randomSeedPerFrame = ParseBool(value);
    else if (key == "lpSeed") app.livePaint.seed = ParseInt(value, app.livePaint.seed);
    else if (key == "lpPaintersColorFilter") app.livePaint.paintersColorFilter = ParseInt(value, app.livePaint.paintersColorFilter);
    else if (key == "lpFilterStrength") app.livePaint.filterStrength = ParseFloat(value, app.livePaint.filterStrength);
    else if (key == "lpHueVariation") app.livePaint.hueVariation = ParseFloat(value, app.livePaint.hueVariation);
    else if (key == "lpSaturationVariation") app.livePaint.saturationVariation = ParseFloat(value, app.livePaint.saturationVariation);
    else if (key == "lpValueVariation") app.livePaint.valueVariation = ParseFloat(value, app.livePaint.valueVariation);
    else if (key == "lpBumpStrength") app.livePaint.bumpStrength = ParseFloat(value, app.livePaint.bumpStrength);
    else if (key == "lpBrushGrid") app.livePaint.brushGrid = ParseFloat(value, app.livePaint.brushGrid);
    else if (key == "lpCanvasStrength") app.livePaint.canvasStrength = ParseFloat(value, app.livePaint.canvasStrength);
    else if (key == "lpBrushNormalScale") app.livePaint.brushNormalScale = ParseFloat(value, app.livePaint.brushNormalScale);
    else if (key == "lpPreviewExaggeration") app.livePaint.previewExaggeration = ParseFloat(value, app.livePaint.previewExaggeration);
    else if (key == "lpFlatBrushBody") app.livePaint.flatBrushBody = ParseFloat(value, app.livePaint.flatBrushBody);
    else if (key == "lpFlatOpacityBoost") app.livePaint.flatOpacityBoost = ParseFloat(value, app.livePaint.flatOpacityBoost);
    else if (key == "lpFlatColorFollow") app.livePaint.flatColorFollow = ParseFloat(value, app.livePaint.flatColorFollow);
    else if (key == "lpFlatLayerBlocking") app.livePaint.flatLayerBlocking = ParseFloat(value, app.livePaint.flatLayerBlocking);
    else if (key == "lpFlatAccumulation") app.livePaint.flatAccumulation = ParseFloat(value, app.livePaint.flatAccumulation);
    else if (key == "lpFlatStrokeLength") app.livePaint.flatStrokeLength = ParseFloat(value, app.livePaint.flatStrokeLength);
    else if (key == "lpFlatStrokeWidth") app.livePaint.flatStrokeWidth = ParseFloat(value, app.livePaint.flatStrokeWidth);
    else if (key == "lpFlatPaintOpacity") app.livePaint.flatPaintOpacity = ParseFloat(value, app.livePaint.flatPaintOpacity);
    else if (key == "lpBaking") app.livePaint.baking = ParseInt(value, app.livePaint.baking);
    else if (key == "akfRadius") app.livePaint.kuwaharaRadius = ParseFloat(value, app.livePaint.kuwaharaRadius);
    else if (key == "akfStrength") app.livePaint.kuwaharaStrength = ParseFloat(value, app.livePaint.kuwaharaStrength);
    else if (key == "akfSharpness") app.livePaint.kuwaharaSharpness = ParseFloat(value, app.livePaint.kuwaharaSharpness);
    else if (key == "akfHardness") {
        float v = ParseFloat(value, app.livePaint.kuwaharaHardness);
        if (v > 4.f)
            v = std::max(v / 8.f, 0.25f);
        app.livePaint.kuwaharaHardness = v;
    }
    else if (key == "akfEccentricity") app.livePaint.kuwaharaEccentricity = ParseFloat(value, app.livePaint.kuwaharaEccentricity);
    else if (key == "akfAnisotropy") app.livePaint.kuwaharaAnisotropy = ParseFloat(value, app.livePaint.kuwaharaAnisotropy);
    else if (key == "noiseType") app.livePaint.noiseType = ParseInt(value, app.livePaint.noiseType);
    else if (key == "noiseAmount") {
        app.livePaint.noiseAmount = ParseFloat(value, app.livePaint.noiseAmount);
        if (app.livePaint.noiseAmount > 1.f)
            app.livePaint.noiseAmount /= 10.f;
        app.livePaint.noiseAmount = std::clamp(app.livePaint.noiseAmount, 0.f, 1.f);
    }
    else if (key == "noiseScale") app.livePaint.noiseScale = ParseFloat(value, app.livePaint.noiseScale);
    else if (key == "noiseSeed") app.livePaint.noiseSeed = ParseFloat(value, app.livePaint.noiseSeed);
    else if (key == "noiseOctaves") app.livePaint.noiseOctaves = ParseInt(value, app.livePaint.noiseOctaves);
    else if (key == "noiseLacunarity") app.livePaint.noiseLacunarity = ParseFloat(value, app.livePaint.noiseLacunarity);
    else if (key == "noiseGain") app.livePaint.noiseGain = ParseFloat(value, app.livePaint.noiseGain);
    else if (key == "noiseJitter") app.livePaint.noiseJitter = ParseFloat(value, app.livePaint.noiseJitter);
    else if (key == "noiseContrast") app.livePaint.noiseContrast = ParseFloat(value, app.livePaint.noiseContrast);
    else if (key == "noiseAngle") app.livePaint.noiseAngle = ParseFloat(value, app.livePaint.noiseAngle);
    else if (key == "noiseDirectionality") app.livePaint.noiseDirectionality = ParseFloat(value, app.livePaint.noiseDirectionality);
}

bool LoadFromPath(const std::filesystem::path& path, AppState& app) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        ApplyKey(app, line.substr(0, eq), line.substr(eq + 1));
    }
    app.selectSidebarTabOnce = app.activeSidebarTab;
    app.selectViewTabOnce = static_cast<int>(app.materialParams.view.mode);
    return true;
}

} // namespace

std::filesystem::path UserSettings::DefaultPath() {
    return ResolveSettingsPath();
}

bool UserSettings::Load(AppState& app) {
    if (LoadFromPath(ResolveSettingsPath(), app)) return true;
    return LoadFromPath(LegacyLightingPath(), app);
}

bool UserSettings::Save(const AppState& app) {
    const auto path = ResolveSettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;

    const auto& view = app.materialParams.view;
    const auto& mat = app.materialParams;

    out << "# ShaderViewer session settings\n";
    out << "activeSidebarTab=" << app.activeSidebarTab << '\n';

    out << "lightIntensity=" << app.lightIntensity << '\n';
    out << "lightColorR=" << app.lightColor.x << '\n';
    out << "lightColorG=" << app.lightColor.y << '\n';
    out << "lightColorB=" << app.lightColor.z << '\n';
    out << "lightDirectionX=" << app.lightDirection.x << '\n';
    out << "lightDirectionY=" << app.lightDirection.y << '\n';
    out << "lightDirectionZ=" << app.lightDirection.z << '\n';
    out << "cameraRelativeLight=" << (app.cameraRelativeLight ? 1 : 0) << '\n';
    out << "lightPitchOffset=" << app.lightPitchOffset << '\n';
    out << "lightSideOffset=" << app.lightSideOffset << '\n';
    out << "ambientIntensity=" << app.ambientIntensity << '\n';
    out << "lightWrap=" << app.lightWrap << '\n';
    out << "scatterStrength=" << app.scatterStrength << '\n';
    out << "exposure=" << app.exposure << '\n';

    out << "viewMode=" << static_cast<int>(view.mode) << '\n';
    out << "viewPresentation=" << static_cast<int>(view.presentation) << '\n';
    out << "normalViewSpace=" << static_cast<int>(view.normalSpace) << '\n';
    out << "normalViewScale=" << view.normalViewScale << '\n';
    out << "uvCheckerScale=" << view.uvCheckerScale << '\n';
    out << "uvShowAlbedo=" << (view.uvShowAlbedo ? 1 : 0) << '\n';
    out << "uvShowWireframe=" << (view.uvShowWireframe ? 1 : 0) << '\n';
    out << "flatViewPanX=" << view.flatViewPanX << '\n';
    out << "flatViewPanY=" << view.flatViewPanY << '\n';
    out << "flatViewZoom=" << view.flatViewZoom << '\n';
    out << "applyModifierToRender=" << (app.applyModifierToRender ? 1 : 0) << '\n';

    out << "baseColorR=" << mat.baseColorFactor.x << '\n';
    out << "baseColorG=" << mat.baseColorFactor.y << '\n';
    out << "baseColorB=" << mat.baseColorFactor.z << '\n';
    out << "baseColorA=" << mat.baseColorFactor.w << '\n';
    out << "metallicFactor=" << mat.metallicFactor << '\n';
    out << "roughnessFactor=" << mat.roughnessFactor << '\n';
    out << "aoFactor=" << mat.aoFactor << '\n';
    out << "normalStrength=" << mat.normalStrength << '\n';
    out << "emissiveStrength=" << mat.emissiveStrength << '\n';
    out << "flipNormalGreen=" << (mat.flipNormalGreen ? 1 : 0) << '\n';
    out << "packedR=" << static_cast<int>(mat.packedR) << '\n';
    out << "packedG=" << static_cast<int>(mat.packedG) << '\n';
    out << "packedB=" << static_cast<int>(mat.packedB) << '\n';

    for (int i = 0; i < static_cast<int>(TextureSlot::Count); ++i) {
        out << "srgbOverride" << i << '=' << (app.material.SrgbOverride()[static_cast<size_t>(i)] ? 1 : 0) << '\n';
    }

    out << "scanRoot=" << app.scanRoot << '\n';

    std::string materialName;
    if (app.selectedMaterial >= 0 && app.selectedMaterial < static_cast<int>(app.scanner.MaterialSets().size())) {
        materialName = app.scanner.MaterialSets()[static_cast<size_t>(app.selectedMaterial)].name;
    }
    out << "savedMaterialName=" << materialName << '\n';

    std::string meshFile;
    if (app.meshKind == PreviewMeshKind::Imported && app.selectedMesh >= 0 &&
        app.selectedMesh < static_cast<int>(app.scanner.MeshFiles().size())) {
        meshFile = app.scanner.MeshFiles()[static_cast<size_t>(app.selectedMesh)].filename().string();
    }
    out << "savedMeshFile=" << meshFile << '\n';
    out << "savedMeshKind=" << static_cast<int>(app.meshKind) << '\n';
    out << "savedSelectedMesh=" << app.selectedMesh << '\n';

    const auto target = app.camera.Target();
    out << "cameraTargetX=" << target.x << '\n';
    out << "cameraTargetY=" << target.y << '\n';
    out << "cameraTargetZ=" << target.z << '\n';
    out << "cameraYaw=" << app.camera.yaw() << '\n';
    out << "cameraPitch=" << app.camera.pitch() << '\n';
    out << "cameraDistance=" << app.camera.distance() << '\n';
    out << "restoreSavedCamera=1\n";

    const auto& lp = app.livePaint;
    out << "lpEnabled=" << (lp.enabled ? 1 : 0) << '\n';
    out << "lpPaintingThickness=" << lp.paintingThickness << '\n';
    out << "lpPadding=" << lp.padding << '\n';
    out << "lpCanvas=" << (lp.canvas ? 1 : 0) << '\n';
    out << "lpHoldout=" << (lp.holdout ? 1 : 0) << '\n';
    out << "lpTransparentBackground=" << (lp.transparentBackground ? 1 : 0) << '\n';
    out << "lpStrokeDensity=" << lp.strokeDensity << '\n';
    out << "lpMinStrokeScale=" << lp.minStrokeScale << '\n';
    out << "lpMaxStrokeScale=" << lp.maxStrokeScale << '\n';
    out << "lpScaleThreshold=" << lp.scaleThreshold << '\n';
    out << "lpMinStrokeOpacity=" << lp.minStrokeOpacity << '\n';
    out << "lpMaxStrokeOpacity=" << lp.maxStrokeOpacity << '\n';
    out << "lpOpacityThreshold=" << lp.opacityThreshold << '\n';
    out << "lpStackDirection=" << (lp.stackDirection ? 1 : 0) << '\n';
    out << "lpBrokenEdges=" << (lp.brokenEdges ? 1 : 0) << '\n';
    out << "lpRandomSeedPerFrame=" << (lp.randomSeedPerFrame ? 1 : 0) << '\n';
    out << "lpSeed=" << lp.seed << '\n';
    out << "lpPaintersColorFilter=" << lp.paintersColorFilter << '\n';
    out << "lpFilterStrength=" << lp.filterStrength << '\n';
    out << "lpHueVariation=" << lp.hueVariation << '\n';
    out << "lpSaturationVariation=" << lp.saturationVariation << '\n';
    out << "lpValueVariation=" << lp.valueVariation << '\n';
    out << "lpBumpStrength=" << lp.bumpStrength << '\n';
    out << "lpBrushGrid=" << lp.brushGrid << '\n';
    out << "lpCanvasStrength=" << lp.canvasStrength << '\n';
    out << "lpBrushNormalScale=" << lp.brushNormalScale << '\n';
    out << "lpPreviewExaggeration=" << lp.previewExaggeration << '\n';
    out << "lpFlatBrushBody=" << lp.flatBrushBody << '\n';
    out << "lpFlatOpacityBoost=" << lp.flatOpacityBoost << '\n';
    out << "lpFlatColorFollow=" << lp.flatColorFollow << '\n';
    out << "lpFlatLayerBlocking=" << lp.flatLayerBlocking << '\n';
    out << "lpFlatAccumulation=" << lp.flatAccumulation << '\n';
    out << "lpFlatStrokeLength=" << lp.flatStrokeLength << '\n';
    out << "lpFlatStrokeWidth=" << lp.flatStrokeWidth << '\n';
    out << "lpFlatPaintOpacity=" << lp.flatPaintOpacity << '\n';
    out << "lpBaking=" << lp.baking << '\n';
    out << "akfRadius=" << lp.kuwaharaRadius << '\n';
    out << "akfStrength=" << lp.kuwaharaStrength << '\n';
    out << "akfSharpness=" << lp.kuwaharaSharpness << '\n';
    out << "akfHardness=" << lp.kuwaharaHardness << '\n';
    out << "akfEccentricity=" << lp.kuwaharaEccentricity << '\n';
    out << "akfAnisotropy=" << lp.kuwaharaAnisotropy << '\n';
    out << "noiseType=" << lp.noiseType << '\n';
    out << "noiseAmount=" << lp.noiseAmount << '\n';
    out << "noiseScale=" << lp.noiseScale << '\n';
    out << "noiseSeed=" << lp.noiseSeed << '\n';
    out << "noiseOctaves=" << lp.noiseOctaves << '\n';
    out << "noiseLacunarity=" << lp.noiseLacunarity << '\n';
    out << "noiseGain=" << lp.noiseGain << '\n';
    out << "noiseJitter=" << lp.noiseJitter << '\n';
    out << "noiseContrast=" << lp.noiseContrast << '\n';
    out << "noiseAngle=" << lp.noiseAngle << '\n';
    out << "noiseDirectionality=" << lp.noiseDirectionality << '\n';

    return static_cast<bool>(out);
}

void UserSettings::ApplySelection(AppState& app, GpuMesh& activeMesh, ID3D11Device* device) {
    if (!app.savedMaterialName.empty()) {
        for (int i = 0; i < static_cast<int>(app.scanner.MaterialSets().size()); ++i) {
            const auto& set = app.scanner.MaterialSets()[static_cast<size_t>(i)];
            if (set.name == app.savedMaterialName) {
                app.selectedMaterial = i;
                app.material.LoadFromSet(device, set);
                break;
            }
        }
    }

    const auto meshKind = static_cast<PreviewMeshKind>(app.savedMeshKind);
    bool meshLoaded = false;
    if (!app.savedMeshFile.empty()) {
        for (int i = 0; i < static_cast<int>(app.scanner.MeshFiles().size()); ++i) {
            const auto& path = app.scanner.MeshFiles()[static_cast<size_t>(i)];
            if (path.filename().string() == app.savedMeshFile) {
                app.selectedMesh = i;
                app.meshKind = PreviewMeshKind::Imported;
                LoadedMesh loaded;
                if (MeshLoader::Load(path, loaded)) {
                    activeMesh = RendererD3D11::UploadMesh(device, loaded.vertices, loaded.indices);
                    FocusCameraOnMesh(app.camera, loaded.vertices, app.meshFocusCenter, app.meshFocusRadius);
                    meshLoaded = true;
                }
                break;
            }
        }
    }

    if (!meshLoaded) {
        if (meshKind == PreviewMeshKind::Suzanne) {
            meshLoaded = LoadDefaultMesh(activeMesh, app, device);
        } else if (meshKind == PreviewMeshKind::Sphere || meshKind == PreviewMeshKind::Cube ||
            meshKind == PreviewMeshKind::Plane) {
            MeshData data;
            if (meshKind == PreviewMeshKind::Sphere) data = MeshGen::CreateSphere();
            else if (meshKind == PreviewMeshKind::Cube) data = MeshGen::CreateCube();
            else data = MeshGen::CreatePlane();
            activeMesh = RendererD3D11::UploadMesh(device, data.vertices, data.indices);
            app.meshKind = meshKind;
            app.selectedMesh = -1;
            FocusCameraOnMesh(app.camera, data.vertices, app.meshFocusCenter, app.meshFocusRadius);
            meshLoaded = true;
        } else {
            app.meshKind = meshKind;
            app.selectedMesh = app.savedSelectedMesh;
        }
    }

    if (app.restoreSavedCamera) {
        app.camera.SetOrbitState(
            app.savedCameraTarget, app.savedCameraYaw, app.savedCameraPitch, app.savedCameraDistance);
    }
}

} // namespace sv
