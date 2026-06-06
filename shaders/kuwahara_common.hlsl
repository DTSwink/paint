#ifndef KUWAHARA_COMMON_HLSL
#define KUWAHARA_COMMON_HLSL

static const float KU_EPSILON = 1e-4;
static const int KU_MAX_RADIUS = 8;
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

float3 kuwCombineSectors(float4 moments[KU_SECTORS], float3 s[KU_SECTORS], float sharpness, float3 fallback)
{
    float4 output = 0.0;
    [unroll]
    for (int k = 0; k < KU_SECTORS; ++k)
    {
        if (moments[k].w <= KU_EPSILON)
            continue;

        const float3 mean = moments[k].xyz / moments[k].w;
        const float3 variance = abs(s[k] / moments[k].w - mean * mean);
        const float sigma2 = sqrt(variance.r) + sqrt(variance.g) + sqrt(variance.b);
        const float w = 1.0 / (1.0 + pow(255.0 * sigma2, max(sharpness, 1e-3)));

        output.xyz += mean * w;
        output.w += w;
    }

    if (output.w <= KU_EPSILON)
        return fallback;
    return output.xyz / output.w;
}

void kuwAccumulateAtOffset(
    inout float4 moments[KU_SECTORS], inout float3 s[KU_SECTORS],
    Texture2D source, SamplerState sourceSampler, Texture2D lut, SamplerState lutSampler,
    float2 uv, float2 texelOffset, float2 v)
{
    const float3 colour0 = source.SampleLevel(sourceSampler, uv + texelOffset, 0).xyz;
    const float3 colour1 = source.SampleLevel(sourceSampler, uv - texelOffset, 0).xyz;
    const float3 colour0Sqr = colour0 * colour0;
    const float3 colour1Sqr = colour1 * colour1;

    const float4 w0123 = lut.Sample(lutSampler, float2(0.5, 0.5) + v);
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        moments[k] += float4(colour0 * w0123[k], w0123[k]);
        s[k] += colour0Sqr * w0123[k];
        moments[k + 4] += float4(colour1 * w0123[k], w0123[k]);
        s[k + 4] += colour1Sqr * w0123[k];
    }

    const float4 w4567 = lut.Sample(lutSampler, float2(0.5, 0.5) - v);
    [unroll]
    for (int k = 4; k < KU_SECTORS; ++k)
    {
        moments[k] += float4(colour0 * w4567[k - 4], w4567[k - 4]);
        s[k] += colour0Sqr * w4567[k - 4];
    }
    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        moments[j] += float4(colour1 * w4567[j], w4567[j]);
        s[j] += colour1Sqr * w4567[j];
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
        const float centerWeight = lut.Sample(lutSampler, float2(0.5, 0.5)).x;
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
                    kuwAccumulateAtOffset(
                        moments, sectorVariance, source, sourceSampler, lut, lutSampler,
                        uv, float2(x, y) * rcpSize, v);
                }
            }
        }
    }

    return kuwCombineSectors(moments, sectorVariance, sharpness, center);
}

#endif
