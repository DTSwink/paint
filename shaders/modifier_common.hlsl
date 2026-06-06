#ifndef MODIFIER_COMMON_HLSL
#define MODIFIER_COMMON_HLSL

#include "kuwahara_common.hlsl"

Texture2D KuwaharaLutMap : register(t9);
SamplerState KuwaharaLutSampler : register(s2);

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


float luminance3(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

float2 rotate2(float2 p, float a)
{
    float s = sin(a);
    float c = cos(a);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float2 normalTexelSize()
{
    uint w = 1;
    uint h = 1;
    NormalMap.GetDimensions(w, h);
    return 1.0 / float2(max(w, 1), max(h, 1));
}

float3 sampleEncodedNormalMap(float2 uv)
{
    if (HasNormal == 0)
        return float3(0.5, 0.5, 1.0);

    float3 c = NormalMap.SampleLevel(LinearClampSampler, uv, 0).rgb;
    if (FlipNormalGreen != 0)
        c.g = 1.0 - c.g;
    float3 n = normalize(float3(c.rg * 2.0 - 1.0, c.b * 2.0 - 1.0));
    n.xy *= NormalStrength;
    return saturate(normalize(n) * 0.5 + 0.5);
}

float3 decodeTangentNormal(float3 nTex)
{
    float3 n = float3(nTex.rg * 2.0 - 1.0, nTex.b * 2.0 - 1.0);
    n.xy *= NormalStrength;
    return normalize(n);
}

float3 EncodeWorldNormalFromMap(float2 sampleUv, float3 tangent, float3 bitangent, float3 geometryNormal)
{
    float3 N = normalize(geometryNormal);
    if (HasNormal == 0)
    {
        N *= NormalViewScale;
        return saturate(N * 0.5 + 0.5);
    }

    float3 nTex = NormalMap.SampleLevel(LinearClampSampler, sampleUv, 0).rgb;
    if (FlipNormalGreen != 0)
        nTex.g = 1.0 - nTex.g;
    float3 tN = decodeTangentNormal(nTex);
    float3 T = normalize(tangent);
    float3 B = normalize(bitangent);
    float3 worldN = normalize(mul(tN, float3x3(T, B, N)));
    worldN *= NormalViewScale;
    return saturate(worldN * 0.5 + 0.5);
}

float3 rawModifierSource(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texelOffset,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    // Tangent-space flat sheet: blur in normal-map UV space.
    if (HasNormal != 0 && NormalViewSpace == 0)
        return sampleEncodedNormalMap(uv + texelOffset);

    // Mesh world/face views: screen-space blur so taps follow the surface, not UV islands.
    if (HasNormal != 0 && NormalViewSpace != 0)
    {
        float2 pixelOffset = texelOffset / max(normalTexelSize(), 1e-6);
        float2 sampleUv = uv + pixelOffset.x * ddx(uv) + pixelOffset.y * ddy(uv);
        return EncodeWorldNormalFromMap(sampleUv, tangent, bitangent, geometryNormal);
    }

    float2 pixelOffset = texelOffset / max(normalTexelSize(), 1e-6);
    return saturate(centerColor + centerDx * pixelOffset.x + centerDy * pixelOffset.y);
}

float3 blurWeightedSample(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 offset, float weight, inout float totalWeight,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    totalWeight += weight;
    return rawModifierSource(centerColor, centerDx, centerDy, uv, offset, tangent, bitangent, geometryNormal) * weight;
}

float3 edgeAwareBlurSample(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 offset, float baseWeight, inout float totalWeight,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    float3 c = rawModifierSource(centerColor, centerDx, centerDy, uv, offset, tangent, bitangent, geometryNormal);
    float diff = length(c - centerColor);
    float w = baseWeight / (1.0 + diff * max(NoiseContrast, 0.001) * 16.0);
    totalWeight += w;
    return c * w;
}

float3 blurModifierSource(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texelOffset,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    if (NoiseType == 0 || NoiseAmount <= 0.0)
        return rawModifierSource(centerColor, centerDx, centerDy, uv, texelOffset, tangent, bitangent, geometryNormal);

    float amount = saturate(NoiseAmount);
    float radius = clamp(NoiseScale, 0.0, 80.0);
    if (radius <= 0.001)
        return rawModifierSource(centerColor, centerDx, centerDy, uv, texelOffset, tangent, bitangent, geometryNormal);

    float2 texel = normalTexelSize();
    float2 base = texelOffset;
    float3 center = rawModifierSource(centerColor, centerDx, centerDy, uv, base, tangent, bitangent, geometryNormal);
    float3 sum = 0.0;
    float totalWeight = 0.0;

    if (NoiseType == 3)
    {
        float2 dir = float2(cos(NoiseAngle), sin(NoiseAngle));
        float2 stepUv = dir * texel * radius;
        float spread = max(NoiseDirectionality, 1.0);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base, 0.30, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv * 0.50, 0.22, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base - stepUv * 0.50, 0.22, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv, 0.13 / spread, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base - stepUv, 0.13 / spread, totalWeight, tangent, bitangent, geometryNormal);
    }
    else if (NoiseType == 4)
    {
        float2 stepUv = texel * radius;
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base, 0.36, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2( stepUv.x, 0.0), 0.16, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(-stepUv.x, 0.0), 0.16, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(0.0,  stepUv.y), 0.16, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(0.0, -stepUv.y), 0.16, totalWeight, tangent, bitangent, geometryNormal);
    }
    else if (NoiseType == 5)
    {
        float2 stepUv = texel * radius;
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base, 0.25, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + float2( stepUv.x, 0.0), 0.125, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + float2(-stepUv.x, 0.0), 0.125, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + float2(0.0,  stepUv.y), 0.125, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + float2(0.0, -stepUv.y), 0.125, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2( 1.0,  1.0), 0.0625, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2(-1.0,  1.0), 0.0625, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2( 1.0, -1.0), 0.0625, totalWeight, tangent, bitangent, geometryNormal);
        sum += edgeAwareBlurSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2(-1.0, -1.0), 0.0625, totalWeight, tangent, bitangent, geometryNormal);
    }
    else
    {
        float2 stepUv = texel * radius;
        float centerWeight = NoiseType == 1 ? 1.0 : 0.25;
        float cardinalWeight = NoiseType == 1 ? 1.0 : 0.125;
        float diagonalWeight = NoiseType == 1 ? 1.0 : 0.0625;
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base, centerWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2( stepUv.x, 0.0), cardinalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(-stepUv.x, 0.0), cardinalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(0.0,  stepUv.y), cardinalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + float2(0.0, -stepUv.y), cardinalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2( 1.0,  1.0), diagonalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2(-1.0,  1.0), diagonalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2( 1.0, -1.0), diagonalWeight, totalWeight, tangent, bitangent, geometryNormal);
        sum += blurWeightedSample(centerColor, centerDx, centerDy, uv, base + stepUv * float2(-1.0, -1.0), diagonalWeight, totalWeight, tangent, bitangent, geometryNormal);
    }

    float3 blurred = sum / max(totalWeight, 1e-5);
    return lerp(center, saturate(blurred), amount);
}

float3 sampleModifierSource(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texelOffset,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    return rawModifierSource(centerColor, centerDx, centerDy, uv, texelOffset, tangent, bitangent, geometryNormal);
}

float3 modifierStructureTensor(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texel,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    const float3 fx = (
        -1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(-1.0, -1.0), tangent, bitangent, geometryNormal) +
        -2.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(-1.0, 0.0), tangent, bitangent, geometryNormal) +
        -1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(-1.0, 1.0), tangent, bitangent, geometryNormal) +
        +1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(1.0, -1.0), tangent, bitangent, geometryNormal) +
        +2.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(1.0, 0.0), tangent, bitangent, geometryNormal) +
        +1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(1.0, 1.0), tangent, bitangent, geometryNormal)) / 4.0;

    const float3 fy = (
        -1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(-1.0, -1.0), tangent, bitangent, geometryNormal) +
        -2.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(0.0, -1.0), tangent, bitangent, geometryNormal) +
        -1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(1.0, -1.0), tangent, bitangent, geometryNormal) +
        +1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(-1.0, 1.0), tangent, bitangent, geometryNormal) +
        +2.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(0.0, 1.0), tangent, bitangent, geometryNormal) +
        +1.0 * sampleModifierSource(centerColor, centerDx, centerDy, uv, texel * float2(1.0, 1.0), tangent, bitangent, geometryNormal)) / 4.0;

    return float3(dot(fx, fx), dot(fx, fy), dot(fy, fy));
}

float3 modifierGaussianStructureTensor(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texel,
    float3 tangent, float3 bitangent, float3 geometryNormal, int kernelRadius, float stdDeviation)
{
    float4 totalSample = 0.0;
    [loop]
    for (int x = -KU_MAX_RADIUS; x <= KU_MAX_RADIUS; ++x)
    {
        [loop]
        for (int y = -KU_MAX_RADIUS; y <= KU_MAX_RADIUS; ++y)
        {
            if (abs(x) > kernelRadius || abs(y) > kernelRadius)
                continue;

            const float2 offset = 0.5 * float2(x, y);
            const float gaussianW = kuwGaussian(stdDeviation, offset.x) * kuwGaussian(stdDeviation, offset.y);
            totalSample.xyz += gaussianW * modifierStructureTensor(
                centerColor, centerDx, centerDy, uv + offset * texel, texel, tangent, bitangent, geometryNormal);
            totalSample.w += gaussianW;
        }
    }

    return totalSample.xyz / max(totalSample.w, KU_EPSILON);
}

float4 modifierEigenVector(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 texel,
    float3 tangent, float3 bitangent, float3 geometryNormal, int kernelRadius, float stdDeviation)
{
    const float3 g = modifierGaussianStructureTensor(
        centerColor, centerDx, centerDy, uv, texel, tangent, bitangent, geometryNormal, kernelRadius, stdDeviation);

    const float lambda1 = 0.5 * (g.x + g.z + sqrt(max((g.x - g.z) * (g.x - g.z) + 4.0 * g.y * g.y, KU_EPSILON)));
    const float lambda2 = 0.5 * (g.x + g.z - sqrt(max((g.x - g.z) * (g.x - g.z) + 4.0 * g.y * g.y, KU_EPSILON)));

    float2 t = float2(lambda1 - g.x, -g.y);
    t = length(t) > 0.0 ? normalize(t) : float2(0.0, 1.0);

    const float phi = atan2(t.y, t.x);
    const float anisotropy = clamp((lambda1 - lambda2) / max(lambda1 + lambda2, KU_EPSILON), 0.0, 1.0);
    return float4(t, phi, anisotropy);
}

float3 modifierKuwaharaFilter(float3 centerColor, float2 uv, float3 tangent, float3 bitangent, float3 geometryNormal)
{
    const float3 center = centerColor;
    const float3 centerDx = ddx(centerColor);
    const float3 centerDy = ddy(centerColor);
    const float2 texel = normalTexelSize();
    const int kernelRadius = clamp((int)round(AKFRadius), 1, KU_MAX_RADIUS);
    const float stdDeviation = max(AKFHardness, 0.25);
    const float alpha = max(AKFEccentricity, 0.1);
    const float sharpness = max(AKFSharpness, 0.001);

    const float4 eigen = modifierEigenVector(
        centerColor, centerDx, centerDy, uv, texel, tangent, bitangent, geometryNormal, kernelRadius, stdDeviation);

    const float radius = max(float(kernelRadius), 1.0);
    const float a = radius * clamp((alpha + eigen.w) / max(alpha, KU_EPSILON), 0.1, 2.0);
    const float b = radius * clamp(alpha / max(alpha + eigen.w, KU_EPSILON), 0.1, 2.0);

    const float cosPhi = cos(eigen.z);
    const float sinPhi = sin(eigen.z);
    const float2x2 R = float2x2(cosPhi, -sinPhi, sinPhi, cosPhi);
    const float2x2 S = float2x2(0.5 / max(a, KU_EPSILON), 0.0, 0.0, 0.5 / max(b, KU_EPSILON));
    const float2x2 SR = mul(S, R);

    const float cosPhiAbs = abs(cosPhi);
    const float sinPhiAbs = abs(sinPhi);
    const int maxX = min(KU_MAX_RADIUS, max(1, int(ceil(sqrt(a * a * cosPhiAbs * cosPhiAbs + b * b * sinPhiAbs * sinPhiAbs)))));
    const int maxY = min(KU_MAX_RADIUS, max(1, int(ceil(sqrt(a * a * sinPhiAbs * sinPhiAbs + b * b * cosPhiAbs * cosPhiAbs)))));

    float4 moments[KU_SECTORS];
    float3 sectorVariance[KU_SECTORS];
    [unroll]
    for (int i = 0; i < KU_SECTORS; ++i)
    {
        moments[i] = 0.0;
        sectorVariance[i] = 0.0;
    }

    {
        const float centerWeight = KuwaharaLutMap.Sample(KuwaharaLutSampler, float2(0.5, 0.5)).x;
        [unroll]
        for (int k = 0; k < KU_SECTORS; ++k)
        {
            moments[k] += float4(center * centerWeight, centerWeight);
            sectorVariance[k] += center * center * centerWeight;
        }
    }

    [loop]
    for (int y = 0; y <= maxY; ++y)
    {
        [loop]
        for (int x = -maxX; x <= maxX; ++x)
        {
            if (y != 0 || x > 0)
            {
                const float2 v = mul(SR, float2(x, y));
                if (dot(v, v) <= 0.25)
                {
                    const float3 colour0 = sampleModifierSource(
                        centerColor, centerDx, centerDy, uv, float2(x, y) * texel, tangent, bitangent, geometryNormal);
                    const float3 colour1 = sampleModifierSource(
                        centerColor, centerDx, centerDy, uv, float2(-x, -y) * texel, tangent, bitangent, geometryNormal);
                    const float3 colour0Sqr = colour0 * colour0;
                    const float3 colour1Sqr = colour1 * colour1;

                    const float4 w0123 = KuwaharaLutMap.Sample(KuwaharaLutSampler, float2(0.5, 0.5) + v);
                    [unroll]
                    for (int k = 0; k < 4; ++k)
                    {
                        moments[k] += float4(colour0 * w0123[k], w0123[k]);
                        sectorVariance[k] += colour0Sqr * w0123[k];
                        moments[k + 4] += float4(colour1 * w0123[k], w0123[k]);
                        sectorVariance[k + 4] += colour1Sqr * w0123[k];
                    }

                    const float4 w4567 = KuwaharaLutMap.Sample(KuwaharaLutSampler, float2(0.5, 0.5) - v);
                    [unroll]
                    for (int k = 4; k < KU_SECTORS; ++k)
                    {
                        moments[k] += float4(colour0 * w4567[k - 4], w4567[k - 4]);
                        sectorVariance[k] += colour0Sqr * w4567[k - 4];
                    }
                    [unroll]
                    for (int j = 0; j < 4; ++j)
                    {
                        moments[j] += float4(colour1 * w4567[j], w4567[j]);
                        sectorVariance[j] += colour1Sqr * w4567[j];
                    }
                }
            }
        }
    }

    return kuwCombineSectors(moments, sectorVariance, sharpness, center);
}

float2 uvDeltaToPixelOffset(float2 uv, float2 uvDelta)
{
    float2 ux = ddx(uv);
    float2 uy = ddy(uv);
    float det = ux.x * uy.y - ux.y * uy.x;
    if (abs(det) < 1e-6)
        return 0.0;

    return float2(
        (uvDelta.x * uy.y - uvDelta.y * uy.x) / det,
        (ux.x * uvDelta.y - ux.y * uvDelta.x) / det);
}

float3 sampleModifierAtUv(float3 centerColor, float3 centerDx, float3 centerDy, float2 uv, float2 targetUv,
    float3 tangent, float3 bitangent, float3 geometryNormal)
{
    float2 uvDelta = targetUv - uv;
    if (HasNormal != 0 && NormalViewSpace == 0)
        return sampleEncodedNormalMap(targetUv);

    float2 pixelOffset = clamp(uvDeltaToPixelOffset(uv, uvDelta), -32.0, 32.0);
    return rawModifierSource(centerColor, centerDx, centerDy, uv, pixelOffset * normalTexelSize(), tangent, bitangent, geometryNormal);
}

float3 PaintNormalViewColor(float3 baseColor, float2 uv, float3 worldPos, float3 tangent, float3 bitangent, float3 geometryNormal)
{
    if (LivePaintEnabled == 0)
        return baseColor;

    float3 sourceColor = baseColor;
    if (HasNormal != 0 && NormalViewSpace == 0)
        sourceColor = sampleEncodedNormalMap(uv);
    else if (HasNormal != 0 && NormalViewSpace != 0)
        sourceColor = EncodeWorldNormalFromMap(uv, tangent, bitangent, geometryNormal);

    float3 centerDx = ddx(sourceColor);
    float3 centerDy = ddy(sourceColor);

    if (SkipInShaderBlur != 0)
        return sourceColor;

    float3 blurred = blurModifierSource(sourceColor, centerDx, centerDy, uv, 0.0, tangent, bitangent, geometryNormal);

    if (AKFStrength <= 0.0)
        return blurred;

    const float3 filtered = modifierKuwaharaFilter(blurred, uv, tangent, bitangent, geometryNormal);
    return lerp(blurred, saturate(filtered), saturate(AKFStrength));
}

float3 PerturbNormalWithBrush(float3 normal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    return normalize(normal);
}

#endif
