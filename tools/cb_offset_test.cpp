#include <cstddef>
#include <cstdio>

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
    float kuwaharaRadius;
    float kuwaharaStrength;
    float kuwaharaSharpness;
    float kuwaharaHardness;
    float kuwaharaEccentricity;
    float kuwaharaAnisotropy;
    int noiseType;
    float noiseAmount;
    float noiseScale;
};

int main() {
    printf("time=%zu preview=%zu noiseType=%zu size=%zu\n",
        offsetof(LivePaintCBData, time),
        offsetof(LivePaintCBData, previewExaggeration),
        offsetof(LivePaintCBData, noiseType),
        sizeof(LivePaintCBData));
    return 0;
}
