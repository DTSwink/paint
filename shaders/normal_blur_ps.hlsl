#include "common.hlsli"

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
    int AKFPasses;
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
Texture2D BlurOriginalMap : register(t9);
SamplerState BlurClampSampler : register(s1);

float GaussianWeight(float x, float sigma)
{
    return exp(-0.5 * (x * x) / (sigma * sigma));
}

float4 PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float3 center = BlurSourceMap.SampleLevel(BlurClampSampler, uv, 0).rgb;

    if (NoiseType == 0 || NoiseAmount <= 0.0)
        return float4(center, 1.0);

    const float amount = saturate(NoiseAmount);
    if (BlurPassDirection == 2)
    {
        const float3 original = BlurOriginalMap.SampleLevel(BlurClampSampler, uv, 0).rgb;
        const float3 blurred = BlurSourceMap.SampleLevel(BlurClampSampler, uv, 0).rgb;
        return float4(lerp(original, blurred, amount), 1.0);
    }

    const float radius = clamp(NoiseScale, 0.0, 80.0);
    if (radius <= 0.001)
        return float4(center, 1.0);

    const int kernelRadius = min(32, max(1, int(round(radius * 0.35))));
    const float sigma = max(radius * 0.35, 1.0);
    const float2 axis = BlurPassDirection == 0 ? float2(1.0, 0.0) : float2(0.0, 1.0);
    const float2 texel = BlurSourceTexelSize;

    float3 sum = center;
    float totalWeight = 1.0;

    [loop]
    for (int i = -kernelRadius; i <= kernelRadius; ++i)
    {
        if (i == 0)
            continue;
        const float w = GaussianWeight(float(i), sigma);
        const float2 sampleUv = uv + axis * float(i) * texel;
        sum += BlurSourceMap.SampleLevel(BlurClampSampler, sampleUv, 0).rgb * w;
        totalWeight += w;
    }

    const float3 blurred = sum / max(totalWeight, 1e-5);
    if (BlurPassDirection == 0)
        return float4(saturate(blurred), 1.0);

    const float3 original = BlurOriginalMap.SampleLevel(BlurClampSampler, uv, 0).rgb;
    return float4(lerp(original, saturate(blurred), amount), 1.0);
}
