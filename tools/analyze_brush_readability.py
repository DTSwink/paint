import argparse
import json
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont


def split_capture(path, sidebar):
    full = Image.open(path).convert("RGB")
    vp_w = full.width - sidebar
    half_w = vp_w // 2
    orig = full.crop((sidebar, 0, sidebar + half_w, full.height))
    mod = full.crop((sidebar + half_w, 0, sidebar + 2 * half_w, full.height))
    return orig, mod


def sobel_like(rgb):
    gx = np.zeros(rgb.shape[:2], dtype=np.float32)
    gy = np.zeros(rgb.shape[:2], dtype=np.float32)
    for c in range(3):
        channel = rgb[..., c]
        cy, cx = np.gradient(channel)
        gx += cx * cx
        gy += cy * cy
    gx = np.sqrt(gx / 3.0)
    gy = np.sqrt(gy / 3.0)
    return gx, gy, np.sqrt(gx * gx + gy * gy)


def connected_components(mask):
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    comps = []
    for y in range(h):
        for x in range(w):
            if not mask[y, x] or seen[y, x]:
                continue
            q = deque([(x, y)])
            seen[y, x] = True
            pts = []
            while q:
                px, py = q.popleft()
                pts.append((px, py))
                for oy in (-1, 0, 1):
                    for ox in (-1, 0, 1):
                        if ox == 0 and oy == 0:
                            continue
                        nx = px + ox
                        ny = py + oy
                        if 0 <= nx < w and 0 <= ny < h and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            q.append((nx, ny))
            comps.append(np.asarray(pts, dtype=np.float32))
    return comps


def component_stats(comps):
    stats = []
    for pts in comps:
        area = float(len(pts))
        if area < 4:
            continue
        xy = pts - pts.mean(axis=0, keepdims=True)
        cov = (xy.T @ xy) / max(area, 1.0)
        vals = np.linalg.eigvalsh(cov)
        major = float(np.sqrt(max(vals[1], 1e-6)) * 4.0)
        minor = float(np.sqrt(max(vals[0], 1e-6)) * 4.0)
        aspect = major / max(minor, 1e-3)
        stats.append({"area": area, "major": major, "minor": minor, "aspect": aspect})
    return stats


def orientation_coherence(gx, gy, edge, window):
    h, w = edge.shape
    vals = []
    heat = np.zeros((h, w), dtype=np.float32)
    for y in range(0, h, window):
        for x in range(0, w, window):
            e = edge[y : y + window, x : x + window]
            n = int(e.sum())
            if n < 12:
                continue
            theta = np.arctan2(gy[y : y + window, x : x + window][e], gx[y : y + window, x : x + window][e])
            c = float(np.cos(theta * 2.0).mean())
            s = float(np.sin(theta * 2.0).mean())
            coh = (c * c + s * s) ** 0.5
            vals.append((coh, n))
            heat[y : y + window, x : x + window] = coh
    if not vals:
        return 0.0, heat
    total = sum(n for _, n in vals)
    return sum(v * n for v, n in vals) / max(total, 1), heat


def analyze_pair(orig_img, mod_img, crop):
    x, y, w, h = crop
    orig = np.asarray(orig_img.crop((x, y, x + w, y + h)).convert("RGB")).astype(np.float32) / 255.0
    mod_pil = mod_img.crop((x, y, x + w, y + h)).convert("RGB")
    mod_smooth = mod_pil.filter(ImageFilter.GaussianBlur(radius=0.65))
    mod = np.asarray(mod_smooth).astype(np.float32) / 255.0
    mod_raw = np.asarray(mod_pil).astype(np.float32) / 255.0

    luma = orig @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    foreground = (luma > 0.13) & (np.max(orig, axis=-1) > 0.28)
    diff = np.linalg.norm(mod_raw - orig, axis=-1) / np.sqrt(3.0)
    changed = foreground & (diff > 0.045)

    gx, gy, edge_mag = sobel_like(mod)
    edge_threshold = max(float(np.percentile(edge_mag[foreground], 78.0)) if foreground.any() else 0.03, 0.025)
    edge = foreground & (edge_mag > edge_threshold)

    comps = connected_components(edge)
    stats = component_stats(comps)
    fg_area = max(float(foreground.sum()), 1.0)
    edge_area = max(float(edge.sum()), 1.0)
    small_count = sum(1 for s in stats if s["area"] < 90.0)
    long_area = sum(s["area"] for s in stats if s["aspect"] > 2.35 and s["major"] > 20.0)
    weighted_aspect = sum(s["aspect"] * s["area"] for s in stats) / max(sum(s["area"] for s in stats), 1.0)
    mean_major = sum(s["major"] * s["area"] for s in stats) / max(sum(s["area"] for s in stats), 1.0)
    coherence, coherence_heat = orientation_coherence(gx, gy, edge, 48)

    edge_density = float(edge.sum() / fg_area)
    changed_density = float(changed.sum() / fg_area)
    small_fragments_per_10k = float(small_count / fg_area * 10000.0)
    long_edge_fraction = float(long_area / edge_area)
    mosaic_index = float(
        small_fragments_per_10k * 0.55
        + edge_density * 90.0
        + max(0.0, 1.0 - long_edge_fraction) * 28.0
        + max(0.0, 1.0 - coherence) * 22.0
    )
    brush_readability = float(
        coherence * 35.0
        + long_edge_fraction * 35.0
        + min(weighted_aspect / 4.5, 1.0) * 20.0
        + min(mean_major / 38.0, 1.0) * 10.0
        - min(small_fragments_per_10k / 80.0, 1.0) * 20.0
    )

    return {
        "crop": {"x": x, "y": y, "w": w, "h": h},
        "metrics": {
            "foreground_pixels": int(foreground.sum()),
            "changed_density": changed_density,
            "edge_density": edge_density,
            "edge_components": len(stats),
            "small_fragments_per_10k_fg": small_fragments_per_10k,
            "long_elongated_edge_fraction": long_edge_fraction,
            "weighted_edge_aspect": float(weighted_aspect),
            "weighted_edge_major_px": float(mean_major),
            "orientation_coherence_48": float(coherence),
            "mosaic_index_lower_is_better": mosaic_index,
            "brush_readability_higher_is_better": brush_readability,
        },
        "images": {
            "orig": Image.fromarray(np.uint8(np.clip(orig * 255.0, 0, 255))),
            "mod": Image.fromarray(np.uint8(np.clip(mod_raw * 255.0, 0, 255))),
            "diff": Image.fromarray(np.uint8(np.clip(diff / 0.22, 0, 1) * 255.0)),
            "edge": Image.fromarray(np.uint8(edge.astype(np.float32) * 255.0)),
            "coherence": Image.fromarray(np.uint8(np.clip(coherence_heat, 0, 1) * 255.0)),
        },
    }


def render_board(result, out_path):
    imgs = result["images"]
    metrics = result["metrics"]
    try:
        title_font = ImageFont.truetype("segoeui.ttf", 18)
        label_font = ImageFont.truetype("segoeui.ttf", 13)
    except OSError:
        title_font = label_font = ImageFont.load_default()

    scale = 2
    panels = [
        ("Original crop", imgs["orig"]),
        ("Modified crop", imgs["mod"]),
        ("Change mask", imgs["diff"].convert("RGB")),
        ("Internal edge fragments", imgs["edge"].convert("RGB")),
        ("Orientation coherence", imgs["coherence"].convert("RGB")),
    ]
    pw = panels[0][1].width * scale
    ph = panels[0][1].height * scale
    margin = 18
    gap = 14
    header = 116
    canvas = Image.new("RGB", (margin * 2 + len(panels) * pw + (len(panels) - 1) * gap, header + ph + margin), (18, 20, 24))
    draw = ImageDraw.Draw(canvas)
    draw.text((margin, 14), "Brush Readability Diagnostic", fill=(245, 245, 245), font=title_font)
    draw.text(
        (margin, 42),
        (
            f"mosaic {metrics['mosaic_index_lower_is_better']:.1f} lower better  |  "
            f"brush {metrics['brush_readability_higher_is_better']:.1f} higher better  |  "
            f"small fragments {metrics['small_fragments_per_10k_fg']:.1f}/10k  |  "
            f"elongated edge {metrics['long_elongated_edge_fraction']:.2f}  |  "
            f"coherence {metrics['orientation_coherence_48']:.2f}"
        ),
        fill=(235, 205, 150),
        font=label_font,
    )
    draw.text(
        (margin, 68),
        "Clean brush strokes should show fewer short angular fragments, more elongated continuous edges, and coherent local orientation.",
        fill=(205, 210, 220),
        font=label_font,
    )
    x = margin
    for title, img in panels:
        draw.text((x, header - 22), title, fill=(245, 245, 245), font=label_font)
        canvas.paste(img.resize((pw, ph), Image.Resampling.NEAREST), (x, header))
        x += pw + gap
    canvas.save(out_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--sidebar", type=int, default=380)
    parser.add_argument("--crop-x", type=int, default=18)
    parser.add_argument("--crop-y", type=int, default=130)
    parser.add_argument("--crop-w", type=int, default=300)
    parser.add_argument("--crop-h", type=int, default=235)
    args = parser.parse_args()

    orig, mod = split_capture(args.capture, args.sidebar)
    result = analyze_pair(orig, mod, (args.crop_x, args.crop_y, args.crop_w, args.crop_h))
    out_path = Path(args.out)
    json_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.parent.mkdir(parents=True, exist_ok=True)

    render_board(result, out_path)
    json_path.write_text(json.dumps({"capture": args.capture, "diagnostic": str(out_path), **result["metrics"]}, indent=2), encoding="utf-8")

    m = result["metrics"]
    print(f"Diagnostic: {out_path}")
    print(f"Metrics:    {json_path}")
    print(
        "mosaic={:.2f} brush={:.2f} small_fragments={:.2f}/10k elongated={:.3f} coherence={:.3f}".format(
            m["mosaic_index_lower_is_better"],
            m["brush_readability_higher_is_better"],
            m["small_fragments_per_10k_fg"],
            m["long_elongated_edge_fraction"],
            m["orientation_coherence_48"],
        )
    )


if __name__ == "__main__":
    main()
