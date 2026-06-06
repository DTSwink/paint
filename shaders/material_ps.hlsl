#include "common.hlsli"
#include "normal_modifier.hlsl"

static const float PI = 3.14159265;

float SamplePackedChannel(float3 packed, int meaning)
{
    if (meaning == 1) return packed.r;
    if (meaning == 2) return packed.g;
    if (meaning == 3) return packed.b;
    return 0.0;
}

float3 SrgbToLinear(float3 c)
{
    return pow(abs(c), 2.2);
}

float3 LinearToSrgb(float3 c)
{
    return pow(saturate(c), 1.0 / 2.2);
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-4);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-4);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 ApplyNormalModifier(float3 normal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    if (NormalModifierEnabled == 0 || LivePaintEnabled == 0)
        return normalize(normal);
    return ModifyNormal(normal, tangent, bitangent, uv, worldPos);
}

float3 DecodePaintedNormal(float3 encodedNormal, float2 uv, float3 worldPos, float3 tangent, float3 bitangent, float3 geometryNormal)
{
    float3 painted = PaintNormalViewColor(saturate(encodedNormal), uv, worldPos, tangent, bitangent, geometryNormal);
    return normalize(painted * 2.0 - 1.0);
}

float3 ResolveShadingNormal(float3 geometryNormal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    float3 N = normalize(geometryNormal);
    float3 T = normalize(tangent);
    float3 B = normalize(bitangent);

    if (NormalModifierEnabled != 0 && LivePaintEnabled != 0)
    {
        if (NormalViewSpace == 0)
        {
            float3 tangentNormal = float3(0.0, 0.0, 1.0);
            if (HasNormal)
            {
                tangentNormal = NormalMap.Sample(LinearWrapSampler, uv).xyz * 2.0 - 1.0;
                if (FlipNormalGreen != 0)
                    tangentNormal.y = -tangentNormal.y;
                tangentNormal.xy *= NormalStrength;
                tangentNormal = normalize(tangentNormal);
            }
            tangentNormal = DecodePaintedNormal(tangentNormal * 0.5 + 0.5, uv, worldPos, T, B, N);
            float3x3 TBN = float3x3(T, B, N);
            return normalize(mul(tangentNormal, TBN));
        }

        float3 objectNormal = NormalViewSpace == 2
            ? normalize(cross(ddx(worldPos), ddy(worldPos)))
            : N;
        return DecodePaintedNormal(objectNormal * 0.5 + 0.5, uv, worldPos, T, B, objectNormal);
    }

    if (HasNormal)
    {
        float3 nTex = NormalMap.Sample(LinearWrapSampler, uv).xyz * 2.0 - 1.0;
        if (FlipNormalGreen != 0)
            nTex.y = -nTex.y;
        nTex.xy *= NormalStrength;
        nTex = normalize(nTex);
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(nTex, TBN));
    }
    return N;
}

float3 SampleAlbedo(float2 uv)
{
    float3 albedo = BaseColorFactor.rgb;
    if (HasAlbedo)
        albedo *= SrgbToLinear(AlbedoMap.Sample(LinearWrapSampler, uv).rgb);
    return albedo;
}

float3 RenderPBR(float3 worldPos, float3 geometryNormal, float3 tangent, float3 bitangent, float2 uv)
{
    float3 albedo = SampleAlbedo(uv);

    float roughness = RoughnessFactor;
    if (HasRoughness)
        roughness *= RoughnessMap.Sample(LinearWrapSampler, uv).r;
    roughness = clamp(roughness, 0.04, 1.0);

    float metallic = MetallicFactor;
    if (HasMetallic)
        metallic *= MetallicMap.Sample(LinearWrapSampler, uv).r;
    metallic = saturate(metallic);

    float ao = AOFactor;
    if (HasAO)
        ao *= AOMap.Sample(LinearWrapSampler, uv).r;

    if (HasPackedMask)
    {
        float3 packed = PackedMaskMap.Sample(LinearWrapSampler, uv).rgb;
        float packedAO = SamplePackedChannel(packed, PackedRChannelMeaning);
        float packedRough = SamplePackedChannel(packed, PackedGChannelMeaning);
        float packedMetal = SamplePackedChannel(packed, PackedBChannelMeaning);
        if (PackedRChannelMeaning != 0) ao *= packedAO;
        if (PackedGChannelMeaning != 0) roughness *= packedRough;
        if (PackedBChannelMeaning != 0) metallic *= packedMetal;
        roughness = clamp(roughness, 0.04, 1.0);
        metallic = saturate(metallic);
    }

    float3 emissive = 0.0;
    if (HasEmissive)
        emissive = SrgbToLinear(EmissiveMap.Sample(LinearWrapSampler, uv).rgb) * EmissiveStrength;

    float3 N = ResolveShadingNormal(geometryNormal, tangent, bitangent, uv, worldPos);
    float3 V = normalize(CameraPosition - worldPos);
    float3 L = normalize(-LightDirection);
    float3 H = normalize(V + L);

    float NdotLRaw = dot(N, L);
    float NdotL = saturate((NdotLRaw + LightWrap) / (1.0 + LightWrap));

    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = FresnelSchlick(VdotH, F0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuse = kD * albedo / PI;

    float3 radiance = LightColor * LightIntensity;
    float3 direct = (diffuse + specular) * radiance * NdotL;

    float hemi = saturate(N.y * 0.5 + 0.5);
    float3 skyFill = LightColor * float3(0.55, 0.58, 0.65);
    float3 groundFill = LightColor * float3(0.18, 0.15, 0.12);
    float3 ambient = albedo * lerp(groundFill, skyFill, hemi) * AmbientIntensity;

    float scatterTerm = saturate(1.0 - NdotLRaw);
    float3 scatter = albedo * LightColor * ScatterStrength * scatterTerm;

    float3 color = direct + ambient + scatter;
    color *= ao;
    color += emissive;

    color = ACESFilm(color * Exposure);
    return LinearToSrgb(color);
}

float Checker(float2 uv, float scale)
{
    float2 p = uv * max(scale, 1.0);
    float2 f = abs(frac(p) - 0.5);
    return step(max(f.x, f.y), 0.125);
}

float2 ComputeFlatSheetUV(float2 screenPosPx)
{
    float2 localPx = screenPosPx - float2(FlatViewOriginX, FlatViewOriginY);
    float2 screenPos = localPx * float2(FlatViewInvWidth, FlatViewInvHeight);
    float2 ndc = screenPos * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float halfH = 0.5 / max(FlatViewZoom, 0.01);
    float aspect = FlatViewInvHeight / max(FlatViewInvWidth, 1e-6);
    float halfW = halfH * aspect;
    return float2(FlatViewPanX, FlatViewPanY) + float2(ndc.x * halfW, -ndc.y * halfH);
}

float3 FlatSheetBackground(float2 uv)
{
    float3 checker = lerp(float3(0.10, 0.10, 0.12), float3(0.18, 0.18, 0.20), Checker(uv, 8.0));
    float edge = max(abs(uv.x - 0.5) - 0.5, abs(uv.y - 0.5) - 0.5);
    float border = 1.0 - step(0.004, edge);
    return lerp(checker, float3(1.0, 0.82, 0.15), border);
}

float3 EncodeNormalForView(float3 normal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    float3 N = normalize(normal);
    N = ApplyNormalModifier(N, tangent, bitangent, uv, worldPos);
    N *= NormalViewScale;
    float3 encoded = saturate(N * 0.5 + 0.5);
    if (LivePaintEnabled != 0 && NormalModifierEnabled != 0)
        encoded = PaintNormalViewColor(encoded, uv, worldPos, tangent, bitangent, normal);
    return encoded;
}

float3 SampleFlatNormalMap(float2 uv)
{
    float3 nTex = NormalMap.Sample(LinearClampSampler, uv).rgb;
    if (FlipNormalGreen != 0)
        nTex.g = 1.0 - nTex.g;
    float3 N = nTex * 2.0 - 1.0;
    N.xy *= NormalStrength;
    return normalize(N);
}

float3 EncodeTangentNormalForView(float3 tangentNormal, float2 uv, float3 worldPos)
{
    float3 T = float3(1.0, 0.0, 0.0);
    float3 B = float3(0.0, 1.0, 0.0);
    float3 N = normalize(tangentNormal);
    N = ApplyNormalModifier(N, T, B, uv, worldPos);
    N *= NormalViewScale;
    float3 encoded = saturate(N * 0.5 + 0.5);
    if (LivePaintEnabled != 0 && NormalModifierEnabled != 0)
        encoded = PaintNormalViewColor(encoded, uv, worldPos, T, B, float3(0.0, 0.0, 1.0));
    return encoded;
}

float3 SampleTangentNormalFromMap(float2 uv)
{
    return SampleFlatNormalMap(uv);
}

float3 RenderNormalFlatView(float2 uv)
{
    bool inside = (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0);
    if (!inside)
        return FlatSheetBackground(uv);

    float3 encoded = EncodeTangentNormalForView(
        SampleFlatNormalMap(uv), uv, float3(uv * 128.0, 0.0));

    float edge = max(abs(uv.x - 0.5) - 0.5, abs(uv.y - 0.5) - 0.5);
    float border = 1.0 - step(0.003, -edge);
    return lerp(encoded, float3(1.0, 0.82, 0.15), border);
}

float3 SampleTangentNormalUnmapped(float2 uv)
{
    return SampleTangentNormalFromMap(uv);
}

float3 RenderNormalView(float3 geometryNormal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    if (ViewFlatPreview != 0)
    {
        if (NormalViewSpace == 0 && HasNormal != 0)
            return EncodeTangentNormalForView(SampleFlatNormalMap(uv), uv, worldPos);
        if (NormalViewSpace == 2)
            return EncodeNormalForView(normalize(cross(ddx(worldPos), ddy(worldPos))), tangent, bitangent, uv, worldPos);
        return EncodeNormalForView(normalize(geometryNormal), tangent, bitangent, uv, worldPos);
    }

    float3 N;
    if (NormalViewSpace == 0)
    {
        float3 tangentN = SampleTangentNormalUnmapped(uv);
        tangentN = ApplyNormalModifier(tangentN, normalize(tangent), normalize(bitangent), uv, worldPos);
        float3x3 tbn = float3x3(normalize(tangent), normalize(bitangent), normalize(geometryNormal));
        N = normalize(mul(tangentN, tbn));
        N *= NormalViewScale;
        return saturate(N * 0.5 + 0.5);
    }
    else if (NormalViewSpace == 1)
    {
        N = normalize(geometryNormal);
    }
    else
    {
        N = normalize(cross(ddx(worldPos), ddy(worldPos)));
    }

    return EncodeNormalForView(N, tangent, bitangent, uv, worldPos);
}

float3 RenderUVView(float2 uv)
{
    bool inside = (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0);
    if (!inside)
        return FlatSheetBackground(uv);

    float checker = Checker(uv, UVCheckerScale);
    float3 base = lerp(float3(0.12, 0.12, 0.14), float3(0.85, 0.85, 0.88), checker);

    if (UVShowWireframe != 0)
    {
        float2 g = abs(frac(uv * max(UVCheckerScale, 1.0)) - 0.5);
        float edge = step(0.48, max(g.x, g.y));
        base = lerp(base, float3(0.05, 0.55, 0.95), edge * 0.65);
    }

    if (UVShowAlbedo != 0)
    {
        float3 albedo = SampleAlbedo(uv);
        base = lerp(base, albedo, 0.65);
    }

    float edge = max(abs(uv.x - 0.5) - 0.5, abs(uv.y - 0.5) - 0.5);
    float border = 1.0 - step(0.003, -edge);
    base = lerp(base, float3(1.0, 0.82, 0.15), border);

    return base;
}

float4 PSMain(float4 position : SV_POSITION,
              float3 worldPos : TEXCOORD0,
              float3 normal : TEXCOORD1,
              float3 tangent : TEXCOORD2,
              float3 bitangent : TEXCOORD3,
              float2 uv : TEXCOORD4) : SV_TARGET
{
    if (ViewFlatPreview != 0)
    {
        if (ViewMode == 1 && NormalViewSpace == 0)
        {
            float2 sheetUv = ComputeFlatSheetUV(position.xy);
            return float4(RenderNormalFlatView(sheetUv), 1.0);
        }
        if (ViewMode == 1)
            return float4(RenderNormalView(normal, tangent, bitangent, uv, worldPos), 1.0);
        if (ViewMode == 2)
            return float4(RenderUVView(uv), 1.0);
    }
    if (ViewMode == 1)
        return float4(RenderNormalView(normal, tangent, bitangent, uv, worldPos), 1.0);
    if (ViewMode == 2)
        return float4(RenderUVView(uv), 1.0);
    return float4(RenderPBR(worldPos, normal, tangent, bitangent, uv), 1.0);
}
