#ifndef LIVE_PAINT_COMMON_HLSL
#define LIVE_PAINT_COMMON_HLSL

Texture2D BrushAtlasTex : register(t8);
Texture2D PaintersLutTex : register(t9);
Texture2D LinenCanvasTex : register(t10);
Texture2D CanvasBaseTex : register(t11);
SamplerState PointRepeatSampler : register(s2);

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
};

struct BrushSample
{
    float height;
    float mask;
    float ao;
};

struct LivePaintPluginAttributes
{
    float2 uvMap;
    float random;
    float4 hsv;
    float4 paintersFilter;
    float alpha;
    float holdout;
    float baking;
    float3 normal;
};

float BlenderMapRange(float value, float fromMin, float fromMax, float toMin, float toMax, bool clampValue)
{
    float t = (value - fromMin) / max(fromMax - fromMin, 1e-6);
    if (clampValue)
        t = saturate(t);
    return lerp(toMin, toMax, t);
}

float BlenderMapRangeStepped(float value, float fromMin, float fromMax, float toMin, float toMax, float steps, bool clampValue)
{
    float t = (value - fromMin) / max(fromMax - fromMin, 1e-6);
    if (clampValue)
        t = saturate(t);
    float s = max(steps, 1.0);
    t = floor(t * s) / s;
    return lerp(toMin, toMax, t);
}

float BlenderContrastNode(float value, float contrast, bool clampValue)
{
    // Direct port of helper group ".Contrast Node".
    float contrastMinusOne = contrast - 1.0;
    float scale = BlenderMapRange(contrastMinusOne, 0.0, 1.0, 1.0, 2.0, false);
    float offset = BlenderMapRange(contrastMinusOne, 0.0, 1.0, 0.5, 1.0, false);
    float result = (value - 0.5) * scale + offset;
    return clampValue ? saturate(result) : result;
}

float hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    return frac(p * p);
}

float2 hash21(float2 p)
{
    float n = sin(dot(p, float2(127.1, 311.7))) * 43758.5453;
    return frac(float2(n, n * 1.2154));
}

float3 hash31(float2 p)
{
    float3 q = frac(float3(p.xyx) * float3(0.1031, 0.11369, 0.13787));
    q += dot(q, q.yzx + 19.19);
    return frac(float3((q.x + q.y) * q.z, (q.x + q.z) * q.y, (q.y + q.z) * q.x));
}

BrushSample SampleBrushAtlas(float2 brushUV, float brushSelect01, float brushGrid)
{
    float grid = max(brushGrid, 1.0);
    float tileCount = grid * grid;
    float tile = floor(saturate(brushSelect01) * (tileCount - 1.0));
    float2 tileXY = float2(fmod(tile, grid), floor(tile / grid));
    float2 uv = (tileXY + frac(brushUV)) / grid;

    float3 packed = BrushAtlasTex.SampleLevel(LinearWrapSampler, uv, 0).rgb;

    BrushSample s;
    // Atlas stores 0..1 height; Blender's -1..1 map is already baked into the texture.
    s.height = packed.r;
    s.mask = packed.g;
    s.ao = packed.b;
    return s;
}

BrushSample BrushAlphaNode(float2 brushVector, float xyGrid, float factor)
{
    // Direct port of Blender shader node group ".Brush Alpha".
    // Group inputs: Vector, X/Y Grid, Factor. It remaps Factor to a tile in
    // RGB_Brushstrokes_64 and samples the packed RGB channels.
    float grid = max(xyGrid, 1.0);
    float invGrid = rcp(grid);
    float total = grid * grid;
    float tile = BlenderMapRangeStepped(factor, 0.0, 1.0, 0.0, total, total, true);
    tile = clamp(floor(tile), 0.0, total - 1.0);

    float row = floor(tile / grid);
    float atlasX = tile * invGrid;
    float atlasY = (grid - 1.0 - row) * invGrid;
    float2 atlasUV = brushVector * invGrid + float2(atlasX, atlasY);
    float3 packed = BrushAtlasTex.SampleLevel(LinearWrapSampler, atlasUV, 0).rgb;

    BrushSample s;
    s.height = BlenderMapRange(packed.r, -0.99999994, 1.0, 0.0, 0.99999988, false);
    s.mask = packed.g;
    s.ao = packed.b;
    return s;
}

float2 BuildStrokeUV(float2 uv, float2 worldPosXZ, out float brushPick, out float scale)
{
    float seedOffset = LivePaintRandomSeedPerFrame != 0 ? LivePaintTime : 0.0;
    float density = max(LivePaintStrokeDensity, 0.01);
    float2 strokeUV = (uv + worldPosXZ * 0.0025) * density * max(LivePaintScaleThreshold, 0.01);
    float2 cell = floor(strokeUV);
    if (LivePaintBrokenEdges != 0)
    {
        float broken = step(0.35, hash11(dot(cell, float2(12.9898, 78.233)) + LivePaintSeed + seedOffset));
        strokeUV += hash21(cell + LivePaintSeed) * 0.25 * broken;
    }

    brushPick = hash11(dot(cell, float2(0.371, 0.913)) + LivePaintSeed + seedOffset);
    scale = lerp(LivePaintMinStrokeScale, LivePaintMaxStrokeScale, brushPick);
    return strokeUV / max(scale, 0.01);
}

float SampleBrushHeightAt(float2 strokeUV, float brushPick)
{
    BrushSample brush = BrushAlphaNode(strokeUV, LivePaintBrushGrid, brushPick);
    float opacity = lerp(LivePaintMinStrokeOpacity, LivePaintMaxStrokeOpacity, hash11(brushPick + 0.17));
    float threshold = max(LivePaintOpacityThreshold, 0.01);
    float strokeWeight = saturate((opacity * brush.mask) / threshold);
    return lerp(0.5, brush.height, strokeWeight);
}

float2 Rotate2(float2 p, float a)
{
    float s = sin(a);
    float c = cos(a);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float3 RgbToHsl(float3 c)
{
    float maxc = max(c.r, max(c.g, c.b));
    float minc = min(c.r, min(c.g, c.b));
    float l = (maxc + minc) * 0.5;
    float d = maxc - minc;
    float h = 0.0;
    float s = 0.0;
    if (d > 1e-5)
    {
        s = d / max(1.0 - abs(2.0 * l - 1.0), 1e-5);
        if (maxc == c.r)
            h = frac((c.g - c.b) / d / 6.0);
        else if (maxc == c.g)
            h = ((c.b - c.r) / d + 2.0) / 6.0;
        else
            h = ((c.r - c.g) / d + 4.0) / 6.0;
    }
    return float3(h, saturate(s), saturate(l));
}

float HueToRgb(float p, float q, float t)
{
    t = frac(t);
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

float3 HslToRgb(float3 hsl)
{
    float h = frac(hsl.x);
    float s = saturate(hsl.y);
    float l = saturate(hsl.z);
    if (s <= 1e-5)
        return l.xxx;
    float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    float p = 2.0 * l - q;
    return float3(HueToRgb(p, q, h + 1.0 / 3.0), HueToRgb(p, q, h), HueToRgb(p, q, h - 1.0 / 3.0));
}

float3 PainterColorFilterNode(float3 color, float colorPalette)
{
    // Direct material-side port of ".Painter's Color Filter": HSL-derived
    // nearest LUT samples, mixed by source saturation, bypassed when palette is 0.
    float paletteSteps = round(colorPalette * 10.0);
    if (paletteSteps <= 0.0)
        return color;

    float3 hsl = RgbToHsl(color);
    float row = BlenderMapRangeStepped(paletteSteps, 0.0, 9.0, 0.0, 2.0 / 3.0, 2.0, false);
    float col = BlenderMapRange(paletteSteps, 0.0, 1.0, 0.0, 1.0 / 3.0, false);
    col = frac(col);

    float2 lutScale = 1.0 / 1000.0;
    float hueU = BlenderMapRange(hsl.x, 0.0, 1.0, 0.0, 120.0, false);
    float lumV = BlenderMapRange(hsl.z, 0.0, 1.0, 0.0, 1000.0, false);
    float2 uvA = frac(float2(hueU, lumV) * lutScale + float2(col, row));

    float hueU2 = BlenderMapRange(hsl.x, 0.0, 1.0, col, col + 120.0, false);
    float2 uvB = frac(float2(hueU2, lumV) * lutScale + float2(1.0 / 3.0, row));

    float3 lutA = PaintersLutTex.SampleLevel(PointRepeatSampler, uvA, 0).rgb;
    float3 lutB = PaintersLutTex.SampleLevel(PointRepeatSampler, uvB, 0).rgb;
    float satFactor = BlenderMapRange(hsl.y, 0.0, 1.0, 0.5, 1.0, false);
    float3 lutMix = lerp(lutA, lutB, saturate(satFactor));

    float3 lutHsl = RgbToHsl(lutMix);
    return HslToRgb(float3(lutHsl.x, lutHsl.y, hsl.z));
}

float3 ApplyHsvVariationNode(float3 color, float3 randomVector, float4 hsvControl)
{
    float hue = BlenderMapRange(randomVector.x, 0.0, 1.0, 0.4, 0.6, true);
    float saturation = BlenderMapRange(randomVector.y, 0.0, 1.0, 0.75, 1.25, true);
    float brightness = BlenderMapRange(randomVector.z, 0.0, 1.0, -0.05, 0.05, false);

    float3 hsl = RgbToHsl(color);
    hsl.x = frac(hsl.x + (hue - 0.5) * hsvControl.r);
    hsl.y = saturate(hsl.y * lerp(1.0, saturation, hsvControl.g));
    hsl.z = saturate(hsl.z + brightness * hsvControl.b);
    return HslToRgb(hsl);
}

float3 PreservePaintChroma(float3 paintColor, float3 shadedColor, float maxValueShift)
{
    float3 paintHsl = RgbToHsl(paintColor);
    float3 shadedHsl = RgbToHsl(shadedColor);
    float value = clamp(shadedHsl.z, paintHsl.z - maxValueShift, paintHsl.z + maxValueShift);
    float saturation = max(shadedHsl.y, paintHsl.y);
    return HslToRgb(float3(paintHsl.x, saturate(saturation), saturate(value)));
}

float3 OverlayBlend(float3 a, float3 b)
{
    return lerp(2.0 * a * b, 1.0 - 2.0 * (1.0 - a) * (1.0 - b), step(0.5, a));
}

float3 ScreenBlend(float3 a, float3 b)
{
    return 1.0 - (1.0 - a) * (1.0 - b);
}

float3 MixMultiply(float3 a, float3 b, float factor)
{
    return lerp(a, a * b, saturate(factor));
}

float3 MixOverlay(float3 a, float3 b, float factor)
{
    return lerp(a, OverlayBlend(a, b), saturate(factor));
}

float3 MixScreen(float3 a, float3 b, float factor)
{
    return lerp(a, ScreenBlend(a, b), saturate(factor));
}

float3 FakeLightingNode(float3 color, float3 normal, float ao, float factor, float windowY)
{
    // Direct material-side port of ".Fake Lighting".
    float3 rotated = normalize(normal);
    float lightA = saturate(dot(rotated, float3(0.0, 0.49999997, 0.99999988)));
    float lightB = saturate(dot(rotated, float3(0.0, 0.09999993, 0.19999969)));
    float screenRamp = saturate(windowY);
    float rimRamp = saturate((lightB - 0.21818182) / max(0.23181915 - 0.21818182, 1e-6));

    float3 c = MixMultiply(color, lightA.xxx, 0.75);
    c = MixOverlay(c, lightA.xxx, 0.75);
    c = lerp(c, float3(0.00698322, 0.00935069, 0.00761206), 0.30000001);
    c = MixOverlay(c, screenRamp.xxx, 0.49999997);
    c = MixScreen(c, float3(0.18730813, 0.18730813, 0.18730813), 0.05000000);
    c = MixMultiply(c, ao.xxx, 0.5);
    c = lerp(color, c, saturate(factor));
    c = MixScreen(c, rimRamp.xxx, 0.5);
    return c;
}

LivePaintPluginAttributes MakePreviewPluginAttributes(float2 uv, float3 worldPos)
{
    // Fallback only for this viewer. Real engine integration should feed the
    // named attributes written by Blender's Paint Filter Geometry Nodes group.
    float seedOffset = LivePaintRandomSeedPerFrame != 0 ? LivePaintTime : 0.0;
    float density = max(LivePaintStrokeDensity, 0.01);
    float2 cell = floor((uv + worldPos.xy * LivePaintPaintingThickness * 0.02) * density * 8.0);
    float r = hash11(dot(cell, float2(12.9898, 78.233)) + LivePaintSeed + seedOffset);
    float scale = lerp(LivePaintMinStrokeScale, LivePaintMaxStrokeScale, r);
    float scaleGate = smoothstep(0.0, max(LivePaintScaleThreshold, 0.001), scale);
    float2 jitter = LivePaintBrokenEdges != 0 ? (hash21(cell + LivePaintSeed) - 0.5) * 0.35 : 0.0;
    float angle = LivePaintStackDirection != 0 ? 0.78539816 : (hash11(r + LivePaintSeed) - 0.5) * 6.2831853;
    float2 centered = uv - 0.5 + jitter * LivePaintPadding;
    float2 rotatedUv = Rotate2(centered, angle) + 0.5;

    LivePaintPluginAttributes a;
    a.uvMap = rotatedUv * density / max(scale, 0.04);
    a.random = r;
    a.hsv = float4(LivePaintHueVariation, LivePaintSaturationVariation, LivePaintValueVariation, LivePaintBumpStrength);
    a.paintersFilter = float4(LivePaintFilterStrength, LivePaintPaintersColorFilter, 0.0, 1.0);
    a.alpha = lerp(LivePaintMinStrokeOpacity, LivePaintMaxStrokeOpacity, r) * scaleGate;
    a.holdout = LivePaintHoldout != 0 ? 1.0 : 0.0;
    a.baking = LivePaintBaking;
    a.normal = normalize(float3(worldPos.xy * LivePaintBrushNormalScale, 1.0));
    return a;
}

float3 RefractivePlaneMaterialNode(float3 color, LivePaintPluginAttributes a, float screenY, float unlitNormalOutput)
{
    BrushSample brush = BrushAlphaNode(a.uvMap, LivePaintBrushGrid, a.random);
    float palette = BlenderMapRange(a.paintersFilter.g, 0.0, 9.0, 0.0, 1.0, false);
    float filterStrength = saturate(a.paintersFilter.r);

    float3 filtered = PainterColorFilterNode(color, palette);
    color = lerp(color, filtered, filterStrength);
    color = ApplyHsvVariationNode(color, hash31(float2(a.random, a.random + 0.37)), a.hsv);

    if (unlitNormalOutput < 0.5)
    {
        float2 brushDelta = 1.0 / max(float2(LivePaintBrushGrid * 64.0, LivePaintBrushGrid * 64.0), 1.0);
        float hx = BrushAlphaNode(a.uvMap + float2(brushDelta.x, 0.0), LivePaintBrushGrid, a.random).height;
        float hy = BrushAlphaNode(a.uvMap + float2(0.0, brushDelta.y), LivePaintBrushGrid, a.random).height;
        float bumpDistance = BlenderMapRange(saturate(a.hsv.a), 0.0, 1.0, 0.0, 0.005, false) *
            max(abs(a.alpha), 0.001) * max(LivePaintBrushNormalScale, 0.001) * 260.0;
        float3 brushNormal = normalize(float3((brush.height - hx) * bumpDistance, (brush.height - hy) * bumpDistance, 1.0));
        brushNormal = normalize(lerp(normalize(a.normal), brushNormal, saturate(a.hsv.a)));

        float3 paintColor = color;
        float3 litColor = FakeLightingNode(color, brushNormal, brush.ao, a.hsv.a, screenY);
        color = PreservePaintChroma(paintColor, litColor, 0.08 + 0.05 * saturate(a.hsv.a));
    }

    float3 linen = LinenCanvasTex.SampleLevel(LinearWrapSampler, a.uvMap * 0.15, 0).rgb;
    float3 canvas = CanvasBaseTex.SampleLevel(LinearWrapSampler, a.uvMap, 0).rgb;
    color = lerp(color, color * lerp(canvas, canvas * linen, 0.333), saturate(LivePaintCanvasStrength) * (LivePaintCanvas != 0));
    color = lerp(color, color * 0.35, saturate(a.holdout));
    if (unlitNormalOutput < 0.5)
        color = lerp(color, color * lerp(0.85, 1.15, brush.mask), saturate(LivePaintBumpStrength) * 0.25);
    if (LivePaintTransparentBackground != 0)
        color = saturate(color * 1.05);

    if (a.baking == 1.0)
        color = brush.height.xxx;
    else if (a.baking == 2.0)
        color = brush.mask.xxx;
    else if (a.baking == 3.0)
        color = brush.ao.xxx;

    return color;
}

float3 EstimateFlatMapColorAtDomain(float3 baseColor, float2 domain, float2 targetDomain)
{
    float2 dxDomain = ddx(domain);
    float2 dyDomain = ddy(domain);
    float det = dxDomain.x * dyDomain.y - dxDomain.y * dyDomain.x;
    float2 delta = targetDomain - domain;
    float2 pixelDelta = 0.0;

    if (abs(det) > 1e-6)
    {
        pixelDelta.x = (delta.x * dyDomain.y - delta.y * dyDomain.x) / det;
        pixelDelta.y = (dxDomain.x * delta.y - dxDomain.y * delta.x) / det;
    }

    pixelDelta = clamp(pixelDelta, -48.0, 48.0);
    return saturate(baseColor + ddx(baseColor) * pixelDelta.x + ddy(baseColor) * pixelDelta.y);
}

float3 PaintNormalViewColor(float3 baseColor, float2 uv, float3 worldPos)
{
    float seedOffset = LivePaintRandomSeedPerFrame != 0 ? floor(LivePaintTime * 60.0) : 0.0;
    float density = max(LivePaintStrokeDensity, 0.01) * 10.0;
    // In flat preview this is a 2D paint pass over the unfolded normal map.
    // Do not feed mesh/world position into the stroke domain: UV seams and
    // triangle boundaries would become visible paint discontinuities.
    float2 domain = uv * density;
    float2 baseCell = floor(domain);

    float3 outColor = baseColor;
    float accumAlpha = 0.0;
    float minOpacity = BlenderMapRange(LivePaintMinStrokeOpacity, 0.0, 1.0, 0.0, 2.0, true);
    float maxOpacity = BlenderMapRange(LivePaintMaxStrokeOpacity, 0.0, 1.0, 0.0, 2.0, true);

    [loop]
    for (int oy = -4; oy <= 4; ++oy)
    {
        [loop]
        for (int ox = -4; ox <= 4; ++ox)
        {
            float2 cell = baseCell + float2(ox, oy);
            float cellSeed = LivePaintSeed + seedOffset + dot(cell, float2(17.17, 59.31));
            float3 rnd = hash31(cell + cellSeed);
            float randomFactor = rnd.x;

            float2 center = cell + rnd.xy;
            float2 local = domain - center;
            float scale = max(lerp(LivePaintMinStrokeScale, LivePaintMaxStrokeScale, rnd.z), 0.001);
            float scaleGate = smoothstep(0.0, max(LivePaintScaleThreshold, 0.001), scale);

            float randomAngle = (hash11(cellSeed + 2.0) - 0.5) * 6.2831853;
            float angle = LivePaintStackDirection != 0
                ? atan2(baseColor.g - 0.5, baseColor.r - 0.5)
                : randomAngle;
            local = Rotate2(local, angle);

            float2 strokeSize = float2(0.065, 0.025) * density * scale * (1.0 + LivePaintPadding);
            strokeSize *= lerp(0.85, 1.25, hash11(cellSeed + 3.0)).xx;
            float2 brushUv = local / max(strokeSize, 0.001) + 0.5;
            float inside = step(0.0, brushUv.x) * step(brushUv.x, 1.0) * step(0.0, brushUv.y) * step(brushUv.y, 1.0);

            BrushSample brush = BrushAlphaNode(brushUv, LivePaintBrushGrid, randomFactor);
            float brushField = LivePaintBrokenEdges != 0 ? brush.mask : max(brush.mask, 0.65);
            float opacityFactor = BlenderContrastNode(brushField, LivePaintOpacityThreshold, true);
            float bodyCoverage = smoothstep(0.10, 0.55, brush.mask);
            opacityFactor = max(opacityFactor, bodyCoverage * 0.72);
            float pluginAlpha = lerp(minOpacity, maxOpacity, opacityFactor);
            float alpha = saturate(opacityFactor * pluginAlpha * scaleGate * inside * LivePaintFilterStrength);
            alpha = saturate(alpha * 2.35);
            float3 centerColor = EstimateFlatMapColorAtDomain(baseColor, domain, center);
            float3 strokeBaseColor = lerp(baseColor, centerColor, 0.72);

            LivePaintPluginAttributes attrs;
            attrs.uvMap = brushUv;
            attrs.random = randomFactor;
            attrs.hsv = float4(LivePaintHueVariation, LivePaintSaturationVariation, LivePaintValueVariation, LivePaintBumpStrength);
            attrs.paintersFilter = float4(LivePaintFilterStrength, LivePaintPaintersColorFilter, 0.0, 1.0);
            attrs.alpha = pluginAlpha;
            attrs.holdout = LivePaintHoldout != 0 ? 1.0 : 0.0;
            attrs.baking = LivePaintBaking;
            attrs.normal = normalize(float3(Rotate2(float2(0.0, 1.0), angle) * LivePaintBrushNormalScale, 1.0));

            float3 strokeColor = RefractivePlaneMaterialNode(strokeBaseColor, attrs, uv.y, 1.0);
            float layer = alpha * saturate(1.0 - accumAlpha * 0.55);
            outColor = lerp(outColor, strokeColor, layer);
            accumAlpha = saturate(accumAlpha + layer * 0.65);
        }
    }

    if (LivePaintCanvas != 0)
    {
        float3 linen = LinenCanvasTex.SampleLevel(LinearWrapSampler, uv * 3.0, 0).rgb;
        outColor = lerp(outColor, outColor * linen, saturate(LivePaintCanvasStrength) * 0.35);
    }
    if (LivePaintTransparentBackground != 0)
        outColor = saturate(outColor * 1.05);
    return outColor;
}

float3 PerturbNormalWithBrush(float3 worldNormal, float3 tangent, float3 bitangent, float2 uv, float3 worldPos)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(tangent);
    float3 B = normalize(bitangent);

    float2 worldXZ = (worldPos.xz + worldPos.xy * 0.125) * max(LivePaintPaintingThickness, 0.001) +
        float2(LivePaintSeed * 0.01, 0.0);
    float brushPick;
    float scale;
    float2 strokeUV = BuildStrokeUV(uv, worldXZ, brushPick, scale);

    float2 dStroke = LivePaintPreviewExaggeration / max(LivePaintBrushGrid * 12.0, 1.0);
    float h = SampleBrushHeightAt(strokeUV, brushPick);
    float hx = SampleBrushHeightAt(strokeUV + float2(dStroke.x, 0.0), brushPick);
    float hy = SampleBrushHeightAt(strokeUV + float2(0.0, dStroke.y), brushPick);

    float strength = LivePaintBumpStrength * LivePaintBrushNormalScale * LivePaintPreviewExaggeration * 0.25;
    float3 perturbed = normalize(N + T * (h - hx) * strength + B * (h - hy) * strength);
    return normalize(lerp(N, perturbed, saturate(LivePaintFilterStrength)));
}

#endif
