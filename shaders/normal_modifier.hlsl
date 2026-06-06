#ifndef NORMAL_MODIFIER_HLSLI
#define NORMAL_MODIFIER_HLSLI

#include "modifier_common.hlsl"

float3 ModifyNormal(float3 normal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    if (LivePaintEnabled == 0)
        return normalize(normal);

    return PerturbNormalWithBrush(normal, tangent, bitangent, uv, worldPos);
}

#endif
