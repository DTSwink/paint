#include "common.hlsli"

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 Tangent : TEXCOORD2;
    float3 Bitangent : TEXCOORD3;
    float2 UV : TEXCOORD4;
};

bool UseFlatUvLayout()
{
    return ViewFlatPreview != 0 && (
        (ViewMode == 1 && NormalViewSpace != 0) ||
        ViewMode == 2);
}

float4 FlatUvToClip(float2 uv)
{
    float halfH = 0.5 / max(FlatViewZoom, 0.01);
    float aspect = FlatViewInvHeight / max(FlatViewInvWidth, 1e-6);
    float halfW = halfH * aspect;
    float2 ndc;
    ndc.x = (uv.x - FlatViewPanX) / max(halfW, 1e-6);
    ndc.y = -(uv.y - FlatViewPanY) / max(halfH, 1e-6);
    return float4(ndc, 0.0, 1.0);
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.Position, 1.0), Model);
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(mul(float4(input.Normal, 0.0), NormalMatrix).xyz);
    output.Tangent = normalize(mul(float4(input.Tangent.xyz, 0.0), NormalMatrix).xyz);
    output.Bitangent = normalize(cross(output.Normal, output.Tangent) * input.Tangent.w);
    output.UV = input.UV;

    if (UseFlatUvLayout())
        output.Position = FlatUvToClip(input.UV);
    else
        output.Position = mul(worldPos, ViewProjection);

    return output;
}
