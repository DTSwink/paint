#include "LivePaint.h"

#include <algorithm>

namespace sv {

LivePaintCBData BuildLivePaintCB(const LivePaintParams& params, float time) {
    LivePaintCBData cb{};
    cb.paintingThickness = params.paintingThickness;
    cb.padding = params.padding;
    cb.strokeDensity = params.strokeDensity;
    cb.minStrokeScale = params.minStrokeScale;
    cb.maxStrokeScale = params.maxStrokeScale;
    cb.scaleThreshold = params.scaleThreshold;
    cb.minStrokeOpacity = params.minStrokeOpacity;
    cb.maxStrokeOpacity = params.maxStrokeOpacity;
    cb.opacityThreshold = params.opacityThreshold;
    cb.filterStrength = params.filterStrength;
    cb.hueVariation = params.hueVariation;
    cb.saturationVariation = params.saturationVariation;
    cb.valueVariation = params.valueVariation;
    cb.bumpStrength = params.bumpStrength;
    cb.brushGrid = params.brushGrid;
    cb.canvasStrength = params.canvasStrength;
    cb.brushNormalScale = params.brushNormalScale;
    cb.paintersColorFilter = params.paintersColorFilter;
    cb.baking = params.baking;
    cb.seed = params.seed;
    cb.canvas = params.canvas ? 1 : 0;
    cb.holdout = params.holdout ? 1 : 0;
    cb.transparentBackground = params.transparentBackground ? 1 : 0;
    cb.stackDirection = params.stackDirection ? 1 : 0;
    cb.brokenEdges = params.brokenEdges ? 1 : 0;
    cb.randomSeedPerFrame = params.randomSeedPerFrame ? 1 : 0;
    cb.enabled = params.enabled ? 1 : 0;
    cb.time = time;
    cb.previewExaggeration = params.previewExaggeration;
    cb.flatBrushBody = params.flatBrushBody;
    cb.flatOpacityBoost = params.flatOpacityBoost;
    cb.flatColorFollow = params.flatColorFollow;
    cb.flatLayerBlocking = params.flatLayerBlocking;
    cb.flatAccumulation = params.flatAccumulation;
    cb.flatStrokeLength = params.flatStrokeLength;
    cb.flatStrokeWidth = params.flatStrokeWidth;
    cb.flatPaintOpacity = params.flatPaintOpacity;
    cb.kuwaharaRadius = params.kuwaharaRadius;
    cb.kuwaharaStrength = params.kuwaharaStrength;
    cb.kuwaharaSharpness = params.kuwaharaSharpness;
    cb.kuwaharaHardness = params.kuwaharaHardness;
    cb.kuwaharaEccentricity = params.kuwaharaEccentricity;
    cb.kuwaharaPasses = std::clamp(params.kuwaharaPasses, 1, 4);
    cb.noiseType = params.noiseType;
    cb.noiseAmount = params.noiseAmount;
    cb.noiseScale = params.noiseScale;
    cb.noiseSeed = params.noiseSeed;
    cb.noiseOctaves = params.noiseOctaves;
    cb.noiseLacunarity = params.noiseLacunarity;
    cb.noiseGain = params.noiseGain;
    cb.noiseJitter = params.noiseJitter;
    cb.noiseContrast = params.noiseContrast;
    cb.noiseAngle = params.noiseAngle;
    cb.noiseDirectionality = params.noiseDirectionality;
    cb.skipInShaderBlur = params.skipInShaderBlur;
    cb.blurViewportOriginX = 0.f;
    cb.blurViewportOriginY = 0.f;
    cb.blurPassDirection = 0;
    cb.blurPassPad = 0.f;
    cb.blurSourceTexelSizeX = 0.f;
    cb.blurSourceTexelSizeY = 0.f;
    return cb;
}

} // namespace sv
