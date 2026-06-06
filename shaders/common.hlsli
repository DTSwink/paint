#ifndef FRAME_CB
#define FRAME_CB

cbuffer FrameCB : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 CameraPosition;
    float Time;
    float3 LightDirection;
    float LightIntensity;
    float3 LightColor;
    float Exposure;
    float AmbientIntensity;
    float LightWrap;
    float ScatterStrength;
    float FlatViewPanX;
    float FlatViewPanY;
    float FlatViewZoom;
    float FlatViewInvWidth;
    float FlatViewInvHeight;
    float FlatViewOriginX;
    float FlatViewOriginY;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 Model;
    float4x4 ModelViewProjection;
    float4x4 NormalMatrix;
};

cbuffer MaterialCB : register(b2)
{
    float4 BaseColorFactor;
    float MetallicFactor;
    float RoughnessFactor;
    float AOFactor;
    float NormalStrength;
    float EmissiveStrength;
    int HasAlbedo;
    int HasNormal;
    int HasRoughness;
    int HasMetallic;
    int HasAO;
    int HasEmissive;
    int HasHeight;
    int HasPackedMask;
    int FlipNormalGreen;
    int PackedRChannelMeaning;
    int PackedGChannelMeaning;
    int PackedBChannelMeaning;
    int ViewMode;
    int NormalViewSpace;
    int UVShowAlbedo;
    int UVShowWireframe;
    int ViewFlatPreview;
    int NormalModifierEnabled;
    float UVCheckerScale;
    float NormalViewScale;
    float ViewPad;
};

Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D MetallicMap : register(t3);
Texture2D AOMap : register(t4);
Texture2D EmissiveMap : register(t5);
Texture2D HeightMap : register(t6);
Texture2D PackedMaskMap : register(t7);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearClampSampler : register(s1);

#endif
