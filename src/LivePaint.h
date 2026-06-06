#pragma once

namespace sv {

struct LivePaintParams {
    bool enabled = true;
    float paintingThickness = 0.1f;
    float padding = 0.f;
    bool canvas = false;
    bool holdout = false;
    bool transparentBackground = false;
    float strokeDensity = 1.f;
    float minStrokeScale = 0.25f;
    float maxStrokeScale = 1.f;
    float scaleThreshold = 1.f;
    float minStrokeOpacity = 1.f;
    float maxStrokeOpacity = 0.25f;
    float opacityThreshold = 5.f;
    bool stackDirection = false;
    bool brokenEdges = true;
    bool randomSeedPerFrame = false;
    int seed = 0;
    int baking = 0;
    int paintersColorFilter = 0;
    float filterStrength = 0.5f;
    float hueVariation = 0.25f;
    float saturationVariation = 0.25f;
    float valueVariation = 0.25f;
    float bumpStrength = 0.5f;
    float brushGrid = 8.f;
    float canvasStrength = 0.33f;
    float brushNormalScale = 1.f;
    float previewExaggeration = 1.f;
};

struct LivePaintCBData {
    float paintingThickness;
    float padding;
    float strokeDensity;
    float minStrokeScale;
    float maxStrokeScale;
    float scaleThreshold;
    float minStrokeOpacity;
    float maxStrokeOpacity;
    float opacityThreshold;
    float filterStrength;
    float hueVariation;
    float saturationVariation;
    float valueVariation;
    float bumpStrength;
    float brushGrid;
    float canvasStrength;
    float brushNormalScale;
    int paintersColorFilter;
    int baking;
    int seed;
    int canvas;
    int holdout;
    int transparentBackground;
    int stackDirection;
    int brokenEdges;
    int randomSeedPerFrame;
    int enabled;
    float time;
    float previewExaggeration;
};

LivePaintCBData BuildLivePaintCB(const LivePaintParams& params, float time);

} // namespace sv
