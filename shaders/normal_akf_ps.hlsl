#include "kuwahara_common.hlsl"

cbuffer LivePaintCB : register(b3)
{
    float LivePaintPaintingThickness;
    float LivePaintPadding;
    float LivePaintStrokeDensity;
    float LivePaintMinStrokeScale;
    float LivePaintMaxStrokeScale;
    float LivePaintScaleThreshold;
    float LivePaintMinStrokeOpacity;
    float LivePaintMaxStrokeOpacity;
    float LivePaintOpacityThreshold;
    float LivePaintFilterStrength;
    float LivePaintHueVariation;
    float LivePaintSaturationVariation;
    float LivePaintValueVariation;
    float LivePaintBumpStrength;
    float LivePaintBrushGrid;
    float LivePaintCanvasStrength;
    float LivePaintBrushNormalScale;
    int LivePaintPaintersColorFilter;
    int LivePaintBaking;
    int LivePaintSeed;
    int LivePaintCanvas;
    int LivePaintHoldout;
    int LivePaintTransparentBackground;
    int LivePaintStackDirection;
    int LivePaintBrokenEdges;
    int LivePaintRandomSeedPerFrame;
    int LivePaintEnabled;
    float LivePaintTime;
    float LivePaintPreviewExaggeration;
    float LivePaintFlatBrushBody;
    float LivePaintFlatOpacityBoost;
    float LivePaintFlatColorFollow;
    float LivePaintFlatLayerBlocking;
    float LivePaintFlatAccumulation;
    float LivePaintFlatStrokeLength;
    float LivePaintFlatStrokeWidth;
    float LivePaintFlatPaintOpacity;
    float AKFRadius;
    float AKFStrength;
    float AKFSharpness;
    float AKFHardness;
    float AKFEccentricity;
    float AKFAnisotropy;
    int NoiseType;
    float NoiseAmount;
    float NoiseScale;
    float NoiseSeed;
    int NoiseOctaves;
    float NoiseLacunarity;
    float NoiseGain;
    float NoiseJitter;
    float NoiseContrast;
    float NoiseAngle;
    float NoiseDirectionality;
    int SkipInShaderBlur;
    float BlurViewportOriginX;
    float BlurViewportOriginY;
    int BlurPassDirection;
    float BlurPassPad;
    float2 BlurSourceTexelSize;
};

Texture2D BlurSourceMap : register(t8);
Texture2D KuwaharaLutMap : register(t9);
SamplerState BlurClampSampler : register(s1);
SamplerState KuwaharaLutSampler : register(s2);

float4 PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float3 center = BlurSourceMap.SampleLevel(BlurClampSampler, uv, 0).rgb;
    if (AKFStrength <= 0.0)
        return float4(center, 1.0);

    const int kernelRadius = clamp((int)round(AKFRadius), 1, KU_MAX_RADIUS);
    const float stdDeviation = max(AKFHardness, 0.25);
    const float alpha = max(AKFEccentricity, 0.1);
    const float sharpness = max(AKFSharpness, 0.001);

    const float3 filtered = kuwComputeKuwaharaFilter(
        BlurSourceMap,
        KuwaharaLutMap,
        BlurClampSampler,
        KuwaharaLutSampler,
        uv,
        BlurSourceTexelSize,
        kernelRadius,
        stdDeviation,
        alpha,
        sharpness);

    return float4(lerp(center, filtered, saturate(AKFStrength)), 1.0);
}
