#include "LivePaint.h"

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
    return cb;
}

} // namespace sv
