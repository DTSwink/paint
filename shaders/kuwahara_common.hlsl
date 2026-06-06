#ifndef KUWAHARA_COMMON_HLSL
#define KUWAHARA_COMMON_HLSL

// BrushStrokeTest / Shadertoy DtKczW / testcode.txt (itworks preset)
static const float KU_EPSILON = 1e-4;
static const int KU_MAX_RADIUS = 16;
static const int KU_SECTORS = 8;

float kuwGaussian(float sigma, float pos)
{
    const float s = max(sigma, 1e-3);
    return exp(-0.5 * pos * pos / (s * s));
}

float3 kuwComputeStructureTensor(Texture2D source, SamplerState samplerState, float2 uv, float2 rcpSize)
{
    const float3 fx = (
        -1.0 * source.SampleLevel(samplerState, uv + float2(-rcpSize.x, -rcpSize.y), 0).xyz +
        -2.0 * source.SampleLevel(samplerState, uv + float2(-rcpSize.x, 0.0), 0).xyz +
        -1.0 * source.SampleLevel(samplerState, uv + float2(-rcpSize.x, rcpSize.y), 0).xyz +
        +1.0 * source.SampleLevel(samplerState, uv + float2(rcpSize.x, -rcpSize.y), 0).xyz +
        +2.0 * source.SampleLevel(samplerState, uv + float2(rcpSize.x, 0.0), 0).xyz +
        +1.0 * source.SampleLevel(samplerState, uv + float2(rcpSize.x, rcpSize.y), 0).xyz) / 4.0;

    const float3 fy = (
        -1.0 * source.SampleLevel(samplerState, uv + float2(-rcpSize.x, -rcpSize.y), 0).xyz +
        -2.0 * source.SampleLevel(samplerState, uv + float2(0.0, -rcpSize.y), 0).xyz +
        -1.0 * source.SampleLevel(samplerState, uv + float2(rcpSize.x, -rcpSize.y), 0).xyz +
        +1.0 * source.SampleLevel(samplerState, uv + float2(-rcpSize.x, rcpSize.y), 0).xyz +
        +2.0 * source.SampleLevel(samplerState, uv + float2(0.0, rcpSize.y), 0).xyz +
        +1.0 * source.SampleLevel(samplerState, uv + float2(rcpSize.x, rcpSize.y), 0).xyz) / 4.0;

    return float3(dot(fx, fx), dot(fx, fy), dot(fy, fy));
}

float3 kuwComputeGaussianStructureTensor(
    Texture2D source, SamplerState samplerState, float2 uv, float2 rcpSize, int kernelRadius, float stdDeviation)
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
            totalSample.xyz += gaussianW * kuwComputeStructureTensor(
                source, samplerState, uv + offset * rcpSize, rcpSize);
            totalSample.w += gaussianW;
        }
    }

    return totalSample.xyz / max(totalSample.w, KU_EPSILON);
}

float4 kuwComputeEigenVector(
    Texture2D source, SamplerState samplerState, float2 uv, float2 rcpSize, int kernelRadius, float stdDeviation)
{
    const float3 g = kuwComputeGaussianStructureTensor(source, samplerState, uv, rcpSize, kernelRadius, stdDeviation);

    const float lambda1 = 0.5 * (g.x + g.z + sqrt(max((g.x - g.z) * (g.x - g.z) + 4.0 * g.y * g.y, KU_EPSILON)));
    const float lambda2 = 0.5 * (g.x + g.z - sqrt(max((g.x - g.z) * (g.x - g.z) + 4.0 * g.y * g.y, KU_EPSILON)));

    float2 t = float2(lambda1 - g.x, -g.y);
    t = length(t) > 0.0 ? normalize(t) : float2(0.0, 1.0);

    const float phi = atan2(t.y, t.x);
    const float anisotropy = clamp((lambda1 - lambda2) / max(lambda1 + lambda2, KU_EPSILON), 0.0, 1.0);
    return float4(t, phi, anisotropy);
}

float2 kuwLutUv(float2 srOffset)
{
    return clamp(float2(0.5, 0.5) + srOffset, float2(0.001, 0.001), float2(0.999, 0.999));
}

void kuwAccumulateAtOffset(
    inout float4 moments[KU_SECTORS], inout float3 s[KU_SECTORS],
    Texture2D source, SamplerState sourceSampler, Texture2D lut, SamplerState lutSampler,
    float2 uv, float2 texelOffset, float2 srOffset)
{
    const float3 c0 = source.SampleLevel(sourceSampler, uv + texelOffset, 0).xyz;
    const float3 c1 = source.SampleLevel(sourceSampler, uv - texelOffset, 0).xyz;
    const float3 c0s = c0 * c0;
    const float3 c1s = c1 * c1;

    const float4 w0123 = lut.Sample(lutSampler, kuwLutUv(srOffset));
    const float4 w4567 = lut.Sample(lutSampler, kuwLutUv(-srOffset));

    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        moments[k] += float4(c0 * w0123[k], w0123[k]);
        s[k] += c0s * w0123[k];
        moments[k + 4] += float4(c1 * w0123[k], w0123[k]);
        s[k + 4] += c1s * w0123[k];
    }
    [unroll]
    for (int i = 4; i < 8; ++i)
    {
        moments[i] += float4(c0 * w4567[i - 4], w4567[i - 4]);
        s[i] += c0s * w4567[i - 4];
    }
    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        moments[j] += float4(c1 * w4567[j], w4567[j]);
        s[j] += c1s * w4567[j];
    }
}

float3 kuwComputeKuwaharaFilter(
    Texture2D source,
    Texture2D lut,
    SamplerState sourceSampler,
    SamplerState lutSampler,
    float2 uv,
    float2 rcpSize,
    int kernelRadius,
    float stdDeviation,
    float alpha,
    float sharpness)
{
    const float3 center = source.SampleLevel(sourceSampler, uv, 0).xyz;
    const float4 eigen = kuwComputeEigenVector(source, sourceSampler, uv, rcpSize, kernelRadius, stdDeviation);
    const float krF = max(float(kernelRadius), 1.0);

    const float2x2 S = float2x2(
        alpha / max(alpha + eigen.w, KU_EPSILON), 0.0,
        0.0, (alpha + eigen.w) / max(alpha, KU_EPSILON));
    const float cosPhi = cos(eigen.z);
    const float sinPhi = sin(eigen.z);
    const float2x2 R = float2x2(cosPhi, -sinPhi, sinPhi, cosPhi);
    const float2x2 SR = mul(S, R);

    float4 moments[KU_SECTORS];
    float3 sectorVariance[KU_SECTORS];
    [unroll]
    for (int i = 0; i < KU_SECTORS; ++i)
    {
        moments[i] = 0.0;
        sectorVariance[i] = 0.0;
    }

    {
        const float centerWeight = lut.Sample(lutSampler, float2(0.5, 0.5)).x;
        [unroll]
        for (int k = 0; k < KU_SECTORS; ++k)
        {
            moments[k] += float4(center * centerWeight, centerWeight);
            sectorVariance[k] += center * center * centerWeight;
        }
    }

    [loop]
    for (int y = -KU_MAX_RADIUS; y <= KU_MAX_RADIUS; ++y)
    {
        [loop]
        for (int x = -KU_MAX_RADIUS; x <= KU_MAX_RADIUS; ++x)
        {
            if (abs(x) > kernelRadius || abs(y) > kernelRadius)
                continue;

            const float2 offset = float2(x, y);
            if (dot(offset / krF, offset / krF) > 0.25)
                continue;

            const float2 sr = mul(SR, offset / krF);
            kuwAccumulateAtOffset(
                moments, sectorVariance, source, sourceSampler, lut, lutSampler,
                uv, offset * rcpSize, sr);
        }
    }

    float4 result = 0.0;
    const float q = max(sharpness, 0.001);
    [unroll]
    for (int k = 0; k < KU_SECTORS; ++k)
    {
        if (moments[k].w <= KU_EPSILON)
            continue;

        const float3 mean = moments[k].rgb / moments[k].w;
        const float3 variance = abs(sectorVariance[k] / moments[k].w - mean * mean);
        const float si = sqrt(dot(variance, variance));
        const float w = 1.0 / (1.0 + pow(255.0 * si, q));
        result.xyz += mean * w;
        result.w += w;
    }

    if (result.w <= KU_EPSILON)
        return center;
    return result.xyz / result.w;
}

#endif
