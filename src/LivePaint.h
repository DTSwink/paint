#pragma once

namespace sv {

struct LivePaintParams {
    bool enabled = true;
    float paintingThickness = 0.1f;
    float padding = 0.f;
    bool canvas = false;
    bool holdout = false;
    bool transparentBackground = false;
    float strokeDensity = 0.95f;
    float minStrokeScale = 2.0f;
    float maxStrokeScale = 4.7f;
    float scaleThreshold = 1.f;
    float minStrokeOpacity = 1.f;
    float maxStrokeOpacity = 0.5f;
    float opacityThreshold = 5.f;
    bool stackDirection = false;
    bool brokenEdges = true;
    bool randomSeedPerFrame = false;
    int seed = 0;
    int baking = 0;
    int paintersColorFilter = 0;
    float filterStrength = 0.95f;
    float hueVariation = 0.22f;
    float saturationVariation = 0.65f;
    float valueVariation = 1.0f;
    float bumpStrength = 0.35f;
    float brushGrid = 8.f;
    float canvasStrength = 0.33f;
    float brushNormalScale = 1.f;
    float previewExaggeration = 1.f;
    float flatBrushBody = 0.95f;
    float flatOpacityBoost = 3.35f;
    float flatColorFollow = 0.10f;
    float flatLayerBlocking = 0.86f;
    float flatAccumulation = 0.92f;
    float flatStrokeLength = 0.145f;
    float flatStrokeWidth = 0.050f;
    float flatPaintOpacity = 1.0f;
    // BrushStrokeTest / itworks preset
    float kuwaharaRadius = 5.0f;
    float kuwaharaStrength = 1.0f;
    float kuwaharaSharpness = 8.0f;
    float kuwaharaHardness = 2.0f;
    float kuwaharaEccentricity = 1.0f;
    int kuwaharaPasses = 2;
    int noiseType = 0;
    float noiseAmount = 0.f;
    float noiseScale = 20.0f;
    float noiseSeed = 0.0f;
    int noiseOctaves = 4;
    float noiseLacunarity = 2.0f;
    float noiseGain = 0.5f;
    float noiseJitter = 0.85f;
    float noiseContrast = 2.0f;
    float noiseAngle = 0.0f;
    float noiseDirectionality = 4.0f;
    int skipInShaderBlur = 0;
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
    float flatBrushBody;
    float flatOpacityBoost;
    float flatColorFollow;
    float flatLayerBlocking;
    float flatAccumulation;
    float flatStrokeLength;
    float flatStrokeWidth;
    float flatPaintOpacity;
    float kuwaharaRadius;
    float kuwaharaStrength;
    float kuwaharaSharpness;
    float kuwaharaHardness;
    float kuwaharaEccentricity;
    int kuwaharaPasses;
    int noiseType;
    float noiseAmount;
    float noiseScale;
    float noiseSeed;
    int noiseOctaves;
    float noiseLacunarity;
    float noiseGain;
    float noiseJitter;
    float noiseContrast;
    float noiseAngle;
    float noiseDirectionality;
    int skipInShaderBlur;
    float blurViewportOriginX;
    float blurViewportOriginY;
    int blurPassDirection;
    float blurPassPad;
    float blurSourceTexelSizeX;
    float blurSourceTexelSizeY;
};

LivePaintCBData BuildLivePaintCB(const LivePaintParams& params, float time);

} // namespace sv
