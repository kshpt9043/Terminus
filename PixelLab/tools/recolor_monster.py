"""이미 만든 몬스터(프레임/시트/GIF 전부)를 색만 바꿔 다른 몬스터로 복제한다.

    python -I recolor_monster.py <원본이름> <새이름> <색상(도)> [채도배수] [밝기배수]

예) 파란 슬라임 -> 레드 슬라임
    python -I PixelLab/tools/recolor_monster.py slime_blue slime_red 0
    python -I PixelLab/tools/recolor_monster.py slime_blue slime_heal 120 0.9 1.0

- 색이 있는 픽셀만 색상(H)을 목표값으로 옮긴다. 밝기/채도 단계는 그대로, 색상 차이는 HUE_SPREAD 만큼만 남김
- 외곽선·눈동자처럼 아주 어두운 픽셀, 흰 하이라이트처럼 채도 없는 픽셀은 그대로 둔다
- 출력 구조는 PIPELINE.md 의 3단계와 같음. 원본 프레임이 왼쪽을 보면 결과도 왼쪽
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/

SAT_MIN = 0.20   # 이보다 채도가 낮으면 무채색으로 보고 안 바꿈
VAL_MIN = 0.12   # 이보다 어두우면 외곽선/눈동자로 보고 안 바꿈
HUE_SPREAD = 0.3 # 원본 색끼리의 색상 차이를 얼마나 남길지. 1 이면 그대로 (파랑->빨강에서 진한 색이 주황으로 밀려서 줄임)


def recolor(im, hue_target, sat_mul, val_mul, hue_src):
    rgba = np.asarray(im.convert("RGBA")).astype(np.float32) / 255.0
    rgb, a = rgba[..., :3], rgba[..., 3:]
    mx, mn = rgb.max(-1), rgb.min(-1)
    v = mx
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60.0

    mask = (s >= SAT_MIN) & (v >= VAL_MIN) & (a[..., 0] > 0)
    dev = (h - hue_src + 180) % 360 - 180          # 대표 색상과의 차이 (-180~180)
    h2 = np.where(mask, (hue_target + dev * HUE_SPREAD) % 360, h)
    s2 = np.where(mask, np.clip(s * sat_mul, 0, 1), s)
    v2 = np.where(mask, np.clip(v * val_mul, 0, 1), v)

    # HSV -> RGB
    c = v2 * s2
    x = c * (1 - np.abs((h2 / 60.0) % 2 - 1))
    m = v2 - c
    k = (h2 // 60).astype(int) % 6
    z = np.zeros_like(c)
    table = [(c, x, z), (x, c, z), (z, c, x), (z, x, c), (x, z, c), (c, z, x)]
    out = np.zeros_like(rgb)
    for i, (rr, gg, bb) in enumerate(table):
        sel = k == i
        out[..., 0] = np.where(sel, rr, out[..., 0])
        out[..., 1] = np.where(sel, gg, out[..., 1])
        out[..., 2] = np.where(sel, bb, out[..., 2])
    out = out + m[..., None]
    res = np.concatenate([out, a], -1)
    return Image.fromarray((res * 255 + 0.5).astype(np.uint8), "RGBA")


def main_hue(im):
    """원본 몸통의 대표 색상: 채도 있는 픽셀 중 가장 많은 색"""
    rgb = np.asarray(im.convert("RGBA"))
    px = rgb[rgb[..., 3] > 0][:, :3]
    cols, counts = np.unique(px, axis=0, return_counts=True)
    for col in cols[np.argsort(-counts)]:
        r, g, b = col / 255.0
        mx, mn = max(r, g, b), min(r, g, b)
        if mx >= VAL_MIN and (mx - mn) / mx >= SAT_MIN:
            d = mx - mn
            if mx == r: return ((g - b) / d % 6) * 60
            if mx == g: return ((b - r) / d + 2) * 60
            return ((r - g) / d + 4) * 60
    return 0.0


def save_gif(frames, path):
    w, h = frames[0].size
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(path, save_all=True, append_images=big[1:], duration=100, loop=0, disposal=2)


def main():
    src, dst, hue = sys.argv[1], sys.argv[2], float(sys.argv[3])
    sat_mul = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    val_mul = float(sys.argv[5]) if len(sys.argv) > 5 else 1.0

    src_dir, dst_dir = os.path.join(ROOT, src), os.path.join(ROOT, dst)
    # 대표 이미지: <원본>_64.png / <원본>_96.png 처럼 크기가 붙은 것
    base_name = next(f for f in os.listdir(src_dir) if f.startswith(src + "_") and f[len(src) + 1:-4].isdigit())
    base = Image.open(os.path.join(src_dir, base_name))
    hue_src = main_hue(base)
    f = lambda im: recolor(im, hue, sat_mul, val_mul, hue_src)

    os.makedirs(dst_dir, exist_ok=True)
    nb = f(base)
    nb.save(os.path.join(dst_dir, f"{dst}_{nb.width}.png"))
    nb.resize((512, 512), Image.NEAREST).save(os.path.join(dst_dir, f"{dst}_preview_512.png"))

    anims = sorted(d[len(src) + 1:] for d in os.listdir(src_dir)
                   if os.path.isdir(os.path.join(src_dir, d)) and d.startswith(src + "_"))
    for anim in anims:
        sd = os.path.join(src_dir, f"{src}_{anim}")
        n = len([p for p in os.listdir(sd) if p.startswith("frame_")])
        frames = [f(Image.open(os.path.join(sd, f"frame_{i}.png"))) for i in range(n)]
        dd = os.path.join(dst_dir, f"{dst}_{anim}")
        os.makedirs(dd, exist_ok=True)
        for old in os.listdir(dd):
            os.remove(os.path.join(dd, old))
        for i, fr in enumerate(frames):
            fr.save(os.path.join(dd, f"frame_{i}.png"))
        w, h = frames[0].size
        sheet = Image.new("RGBA", (w * n, h), (0, 0, 0, 0))
        for i, fr in enumerate(frames):
            sheet.paste(fr, (i * w, 0))
        sheet.save(os.path.join(dst_dir, f"{dst}_{anim}_sheet.png"))
        save_gif(frames, os.path.join(dst_dir, f"{dst}_{anim}_preview.gif"))
        print(anim, n)
    print(f"원본 색상 {hue_src:.0f}도 -> {hue:.0f}도")


if __name__ == "__main__":
    main()
