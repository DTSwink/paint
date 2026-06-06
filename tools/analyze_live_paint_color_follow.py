import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def rgb_to_hsv(rgb):
    r = rgb[..., 0]
    g = rgb[..., 1]
    b = rgb[..., 2]
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    d = mx - mn

    h = np.zeros_like(mx)
    nz = d > 1e-6
    rmax = nz & (mx == r)
    gmax = nz & (mx == g)
    bmax = nz & (mx == b)
    h[rmax] = ((g[rmax] - b[rmax]) / d[rmax]) % 6.0
    h[gmax] = ((b[gmax] - r[gmax]) / d[gmax]) + 2.0
    h[bmax] = ((r[bmax] - g[bmax]) / d[bmax]) + 4.0
    h = (h / 6.0) % 1.0

    s = np.zeros_like(mx)
    s[mx > 1e-6] = d[mx > 1e-6] / mx[mx > 1e-6]
    return np.stack([h, s, mx], axis=-1)


def hue_distance(a, b):
    d = np.abs(a - b)
    return np.minimum(d, 1.0 - d)


def heatmap(values, mask, color):
    v = np.clip(values, 0.0, 1.0)
    out = np.zeros((*v.shape, 3), dtype=np.float32)
    if color == "diff":
        out[..., :] = v[..., None]
    elif color == "hue":
        out[..., 0] = v
        out[..., 1] = np.clip(v * 0.55, 0.0, 1.0)
        out[..., 2] = np.clip(1.0 - v * 2.5, 0.0, 0.35)
    elif color == "sat":
        out[..., 0] = v
        out[..., 1] = np.clip(v * 0.15, 0.0, 1.0)
        out[..., 2] = np.clip(0.25 + v * 0.75, 0.0, 1.0)
    elif color == "white":
        out[..., 0] = v
        out[..., 1] = np.clip(v * 0.95, 0.0, 1.0)
        out[..., 2] = np.clip(v * 0.45, 0.0, 1.0)
    elif color == "topology":
        out[..., 0] = v
        out[..., 1] = np.clip(0.25 + v * 0.6, 0.0, 1.0)
        out[..., 2] = np.clip(1.0 - v, 0.0, 1.0)
    out[~mask] *= 0.18
    return np.uint8(np.clip(out * 255.0, 0, 255))


def find_worst_crops(score, foreground, crop_size, step, top_count):
    h, w = score.shape
    candidates = []
    for y in range(42, max(43, h - crop_size), step):
        for x in range(0, max(1, w - crop_size), step):
            fg = foreground[y : y + crop_size, x : x + crop_size]
            fg_ratio = float(fg.mean())
            if fg_ratio < 0.22:
                continue
            local = score[y : y + crop_size, x : x + crop_size]
            candidates.append((float(local[fg].mean()), fg_ratio, x, y))
    candidates.sort(reverse=True)

    selected = []
    for score_value, fg_ratio, x, y in candidates:
        keep = True
        for _, _, sx, sy in selected:
            ix0 = max(x, sx)
            iy0 = max(y, sy)
            ix1 = min(x + crop_size, sx + crop_size)
            iy1 = min(y + crop_size, sy + crop_size)
            if ix1 <= ix0 or iy1 <= iy0:
                continue
            inter = (ix1 - ix0) * (iy1 - iy0)
            union = crop_size * crop_size * 2 - inter
            if inter / max(union, 1) > 0.25:
                keep = False
                break
        if keep:
            selected.append((score_value, fg_ratio, x, y))
        if len(selected) >= top_count:
            break
    return selected


def draw_panel(draw, img, x, y, title, font, label_font, scale=1):
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.Resampling.NEAREST)
    draw.bitmap((x, y + 20), img)
    draw.text((x, y), title, fill=(245, 245, 245), font=label_font)
    return img.width, img.height + 20


def crop_array(arr, x, y, size):
    return arr[y : y + size, x : x + size]


def to_image(arr):
    return Image.fromarray(np.uint8(np.clip(arr * 255.0, 0, 255)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", required=True, help="Split proof capture with sidebar/original/modified.")
    parser.add_argument("--out", required=True, help="Output diagnostic PNG.")
    parser.add_argument("--json-out", required=True, help="Output metrics JSON.")
    parser.add_argument("--sidebar", type=int, default=380)
    parser.add_argument("--crop-size", type=int, default=160)
    parser.add_argument("--step", type=int, default=20)
    parser.add_argument("--top", type=int, default=3)
    args = parser.parse_args()

    capture_path = Path(args.capture)
    out_path = Path(args.out)
    json_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.parent.mkdir(parents=True, exist_ok=True)

    full = Image.open(capture_path).convert("RGB")
    full_np = np.asarray(full).astype(np.float32) / 255.0
    vp_w = full.width - args.sidebar
    half_w = vp_w // 2
    h = full.height
    orig = full_np[:, args.sidebar : args.sidebar + half_w, :]
    mod = full_np[:, args.sidebar + half_w : args.sidebar + 2 * half_w, :]

    orig_hsv = rgb_to_hsv(orig)
    mod_hsv = rgb_to_hsv(mod)
    orig_luma = orig @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)

    foreground = (orig_luma > 0.18) & (np.max(orig, axis=-1) > 0.35)
    foreground[:42, :] = False

    rgb_delta = np.linalg.norm(mod - orig, axis=-1) / np.sqrt(3.0)
    hue_err = hue_distance(orig_hsv[..., 0], mod_hsv[..., 0]) * np.clip(orig_hsv[..., 1] * 1.35, 0.0, 1.0)
    sat_loss = np.maximum(0.0, orig_hsv[..., 1] - mod_hsv[..., 1])
    value_gain = np.maximum(0.0, mod_hsv[..., 2] - orig_hsv[..., 2])
    value_loss = np.maximum(0.0, orig_hsv[..., 2] - mod_hsv[..., 2])
    white_paint = sat_loss * value_gain

    orig_gy, orig_gx = np.gradient(orig_luma)
    detail_gy, detail_gx = np.gradient(rgb_delta)
    orig_grad = np.sqrt(orig_gx * orig_gx + orig_gy * orig_gy)
    detail_grad = np.sqrt(detail_gx * detail_gx + detail_gy * detail_gy)
    topology_mask = foreground & (orig_grad > 0.006) & (detail_grad > 0.006)
    topology_alignment = np.zeros_like(rgb_delta)
    topology_alignment[topology_mask] = np.abs(
        orig_gx[topology_mask] * detail_gx[topology_mask]
        + orig_gy[topology_mask] * detail_gy[topology_mask]
    ) / np.maximum(orig_grad[topology_mask] * detail_grad[topology_mask], 1e-6)
    topology_mismatch = np.zeros_like(rgb_delta)
    topology_mismatch[topology_mask] = 1.0 - topology_alignment[topology_mask]

    score = (
        rgb_delta * 0.30
        + hue_err * 1.30
        + sat_loss * 0.95
        + white_paint * 2.25
        + value_loss * 0.12
        + topology_mismatch * 0.20
    )
    score[~foreground] = 0.0

    crops = find_worst_crops(score, foreground, args.crop_size, args.step, args.top)
    if not crops:
        raise SystemExit("No foreground crop candidates found.")

    crop_metrics = []
    for score_value, fg_ratio, x, y in crops:
        mask = crop_array(foreground, x, y, args.crop_size)
        topo = crop_array(topology_mask, x, y, args.crop_size)
        topology_mean = 0.0
        topology_pct = 0.0
        if topo.any():
            topology_mean = float(crop_array(topology_mismatch, x, y, args.crop_size)[topo].mean())
            topology_pct = float(topo.mean() * 100.0)
        crop_metrics.append(
            {
                "x": x,
                "y": y,
                "score": score_value,
                "foreground_pct": fg_ratio * 100.0,
                "mean_rgb_delta_0_255": float(crop_array(rgb_delta, x, y, args.crop_size)[mask].mean() * 255.0),
                "mean_hue_error_degrees": float(crop_array(hue_err, x, y, args.crop_size)[mask].mean() * 360.0),
                "mean_saturation_loss": float(crop_array(sat_loss, x, y, args.crop_size)[mask].mean()),
                "mean_value_gain": float(crop_array(value_gain, x, y, args.crop_size)[mask].mean()),
                "mean_white_paint_drift": float(crop_array(white_paint, x, y, args.crop_size)[mask].mean()),
                "mean_topology_mismatch": topology_mean,
                "topology_sample_pct": topology_pct,
            }
        )

    global_mask = foreground
    global_topology_mean = 0.0
    global_topology_pct = 0.0
    if topology_mask.any():
        global_topology_mean = float(topology_mismatch[topology_mask].mean())
        global_topology_pct = float(topology_mask.mean() * 100.0)
    worst = crop_metrics[0]
    color_fail = (
        worst["mean_saturation_loss"] > 0.09
        or worst["mean_white_paint_drift"] > 0.004
        or worst["mean_hue_error_degrees"] > 8.0
    )
    topology_fail = global_topology_pct > 1.0 and global_topology_mean > 0.58
    verdict_parts = []
    if color_fail:
        verdict_parts.append("FAIL_COLOR_FOLLOW")
    if topology_fail:
        verdict_parts.append("FAIL_TOPOLOGY_ALIGNMENT")
    if not verdict_parts:
        verdict_parts.append("PASS_DIAGNOSTIC_THRESHOLDS")
    metrics = {
        "capture": str(capture_path),
        "diagnostic": str(out_path),
        "global": {
            "foreground_pct": float(global_mask.mean() * 100.0),
            "mean_rgb_delta_0_255": float(rgb_delta[global_mask].mean() * 255.0),
            "mean_hue_error_degrees": float(hue_err[global_mask].mean() * 360.0),
            "mean_saturation_loss": float(sat_loss[global_mask].mean()),
            "mean_value_gain": float(value_gain[global_mask].mean()),
            "mean_white_paint_drift": float(white_paint[global_mask].mean()),
            "mean_topology_mismatch": global_topology_mean,
            "topology_sample_pct": global_topology_pct,
        },
        "worst_crops": crop_metrics,
        "verdict": "+".join(verdict_parts),
        "failure_definition": "High hue drift, saturation loss, bright/desaturated white-paint drift, or topology mismatch means strokes are not following the underlying color/topology.",
    }

    json_path.write_text(json.dumps(metrics, indent=2), encoding="utf-8")

    try:
        title_font = ImageFont.truetype("segoeui.ttf", 18)
        label_font = ImageFont.truetype("segoeui.ttf", 14)
        small_font = ImageFont.truetype("segoeui.ttf", 12)
    except OSError:
        title_font = label_font = small_font = ImageFont.load_default()

    overview_scale = 0.45
    overview_w = int(half_w * overview_scale)
    overview_h = int(h * overview_scale)
    row_scale = 2
    crop_panel = args.crop_size * row_scale
    columns = 7
    gap = 16
    margin = 18
    row_h = crop_panel + 88
    canvas_w = margin * 2 + columns * crop_panel + (columns - 1) * gap
    canvas_h = 120 + overview_h + len(crops) * row_h + 30
    canvas = Image.new("RGB", (canvas_w, canvas_h), (18, 20, 24))
    draw = ImageDraw.Draw(canvas)

    draw.text((margin, 14), "Live Paint Color-Follow Diagnostic", fill=(255, 255, 255), font=title_font)
    draw.text(
        (margin, 42),
        "FAIL target: strokes should repaint with local color. Heatmaps expose hue drift, saturation loss, and bright/desaturated white-paint drift.",
        fill=(210, 215, 220),
        font=label_font,
    )
    g = metrics["global"]
    draw.text(
        (margin, 68),
        f"Global foreground: RGB delta {g['mean_rgb_delta_0_255']:.1f}/255  hue error {g['mean_hue_error_degrees']:.1f} deg  sat loss {g['mean_saturation_loss']:.3f}  white drift {g['mean_white_paint_drift']:.4f}  topology mismatch {g['mean_topology_mismatch']:.3f}",
        fill=(255, 210, 150),
        font=label_font,
    )

    orig_img = to_image(orig)
    mod_img = to_image(mod)
    orig_over = orig_img.resize((overview_w, overview_h), Image.Resampling.BILINEAR)
    mod_over = mod_img.resize((overview_w, overview_h), Image.Resampling.BILINEAR)
    ox = margin
    oy = 100
    canvas.paste(orig_over, (ox, oy))
    canvas.paste(mod_over, (ox + overview_w + gap, oy))
    draw.text((ox, oy - 18), "Original overview", fill=(245, 245, 245), font=small_font)
    draw.text((ox + overview_w + gap, oy - 18), "Modified overview", fill=(245, 245, 245), font=small_font)
    for idx, (_, _, x, y) in enumerate(crops, start=1):
        color = [(255, 80, 80), (255, 190, 80), (80, 180, 255)][(idx - 1) % 3]
        rect = [
            ox + int(x * overview_scale),
            oy + int(y * overview_scale),
            ox + int((x + args.crop_size) * overview_scale),
            oy + int((y + args.crop_size) * overview_scale),
        ]
        rect_m = [
            ox + overview_w + gap + int(x * overview_scale),
            oy + int(y * overview_scale),
            ox + overview_w + gap + int((x + args.crop_size) * overview_scale),
            oy + int((y + args.crop_size) * overview_scale),
        ]
        draw.rectangle(rect, outline=color, width=3)
        draw.rectangle(rect_m, outline=color, width=3)
        draw.text((rect[0] + 3, rect[1] + 3), str(idx), fill=color, font=label_font)
        draw.text((rect_m[0] + 3, rect_m[1] + 3), str(idx), fill=color, font=label_font)

    row_y = oy + overview_h + 35
    headers = ["Original crop", "Modified crop", "RGB diff x4", "Hue error", "Saturation loss", "White-paint drift", "Topology mismatch"]
    for row_idx, (_, _, x, y) in enumerate(crops, start=1):
        mask = crop_array(foreground, x, y, args.crop_size)
        orig_crop = to_image(crop_array(orig, x, y, args.crop_size))
        mod_crop = to_image(crop_array(mod, x, y, args.crop_size))
        diff_crop = Image.fromarray(heatmap(crop_array(rgb_delta * 4.0, x, y, args.crop_size), mask, "diff"))
        hue_crop = Image.fromarray(heatmap(crop_array(hue_err * 4.0, x, y, args.crop_size), mask, "hue"))
        sat_crop = Image.fromarray(heatmap(crop_array(sat_loss * 3.0, x, y, args.crop_size), mask, "sat"))
        white_crop = Image.fromarray(heatmap(crop_array(white_paint * 18.0, x, y, args.crop_size), mask, "white"))
        topo_mask = crop_array(topology_mask, x, y, args.crop_size)
        topo_crop = Image.fromarray(heatmap(crop_array(topology_mismatch, x, y, args.crop_size), topo_mask, "topology"))
        panels = [orig_crop, mod_crop, diff_crop, hue_crop, sat_crop, white_crop, topo_crop]

        m = crop_metrics[row_idx - 1]
        draw.text(
            (margin, row_y),
            f"Worst crop {row_idx}: RGB delta {m['mean_rgb_delta_0_255']:.1f}/255, hue {m['mean_hue_error_degrees']:.1f} deg, sat loss {m['mean_saturation_loss']:.3f}, white drift {m['mean_white_paint_drift']:.4f}, topology mismatch {m['mean_topology_mismatch']:.3f}",
            fill=(255, 220, 170),
            font=label_font,
        )
        panel_y = row_y + 28
        for col, panel in enumerate(panels):
            px = margin + col * (crop_panel + gap)
            scaled = panel.resize((crop_panel, crop_panel), Image.Resampling.NEAREST)
            canvas.paste(scaled, (px, panel_y + 18))
            draw.text((px, panel_y), headers[col], fill=(240, 240, 240), font=small_font)
            draw.rectangle([px, panel_y + 18, px + crop_panel - 1, panel_y + 18 + crop_panel - 1], outline=(80, 86, 96), width=1)
        row_y += row_h

    canvas.save(out_path)

    worst = crop_metrics[0]
    print(f"Diagnostic: {out_path}")
    print(f"Metrics:    {json_path}")
    print(
        "Worst crop: "
        f"RGB delta {worst['mean_rgb_delta_0_255']:.1f}/255, "
        f"hue {worst['mean_hue_error_degrees']:.1f} deg, "
        f"sat loss {worst['mean_saturation_loss']:.3f}, "
        f"white drift {worst['mean_white_paint_drift']:.4f}"
    )
    print(f"Verdict: {metrics['verdict']}")


if __name__ == "__main__":
    main()
