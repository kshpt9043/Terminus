"""PixelLab 후보 그림에서 꾸밈(이끼·돌·풀·꽃 등 몸 색이 아닌 부분)만 오려 내서,
기존 슬라임의 모든 동작 프레임에 붙인다. 꾸밈은 프레임마다 몸 범위에 맞춰 늘고 줄며 같이 움직인다.

    python -I decor_overlay.py <후보 png(PixelLab 생성본, 오른쪽을 봄)> <원본이름> <새이름> [break] [body=색상,채도배수,밝기배수]

- break: 사망 때 꾸밈(투구 등)이 두 조각으로 갈라져 떨어지며 사라짐 (슬라임 장군: "투구가 부셔지고")
- body=...: 몸 색도 바꿈 (recolor_monster 와 같은 방식). 꾸밈과 어울리게 (에이션트: 이끼·돌에 맞춘 색)

예) python -I PixelLab/tools/decor_overlay.py ancient_c3.png slime_big_blue slime_ancient

- 후보와 원본은 같은 종류의 몸(같은 크기, 비슷한 돔 모양)이어야 자연스럽다
- 몸 색(채도 있는 파랑 계열)과 눈(앞쪽 가운데)은 꾸밈에서 뺌. 꾸밈에 붙은 어두운 외곽선은 같이 가져옴
- 사망에서는 몸이 흐려지는 만큼 같이 흐려짐
"""
import os
import sys

import numpy as np
from PIL import Image, ImageOps

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # -I 로 실행해도 같은 폴더 도구를 씀
from recolor_monster import main_hue, recolor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/


def hsv(a):
    rgb = a[..., :3].astype(np.float32) / 255
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    return h, s, mx


def body_mask(a, hue_lo=185, hue_hi=235):
    """몸 = 파랑 계열로 채도 있는 픽셀"""
    h, s, v = hsv(a)
    return (a[..., 3] > 128) & (s > 0.25) & (v > 0.2) & (h > hue_lo) & (h < hue_hi)


def bbox(mask):
    ys, xs = np.nonzero(mask)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def palette_mask(a, palette, tol=34):
    """몸 = 원본 슬라임 몸에 실제로 쓰인 색과 가까운 픽셀 (푸른 강철 투구가 몸으로 잘못 잡히지 않게)"""
    rgb = a[..., :3].astype(np.int32)
    best = np.full(rgb.shape[:2], 10 ** 9)
    for c in palette:
        d = ((rgb - np.array(c)) ** 2).sum(-1)
        best = np.minimum(best, d)
    return (a[..., 3] > 128) & (best <= tol * tol)


def extract_decor(cand, ref):
    """ref = 원본 슬라임 대기 0번 프레임. 후보는 몸 윗부분이 꾸밈에 덮여 있어서,
    몸 높이를 '원본 몸의 가로세로 비율'로 다시 잡음 (바닥과 좌우 끝은 후보 것)"""
    a = np.asarray(cand).copy()
    alpha = a[..., 3] > 128
    ra = np.asarray(ref)
    rb = body_mask(ra)
    cols, cnt = np.unique(ra[rb][:, :3], axis=0, return_counts=True)
    palette = [tuple(c) for c in cols[cnt >= 3]]
    body = palette_mask(a, palette) & body_mask(a, 150, 260)
    x0, y0, x1, y1 = bbox(body)
    rx0, ry0, rx1, ry1 = bbox(rb)
    y0 = int(round(y1 - (ry1 - ry0) * (x1 - x0) / (rx1 - rx0)))
    H, W = alpha.shape
    yy, xx = np.mgrid[0:H, 0:W]
    u = (xx - x0) / (x1 - x0)
    w = (yy - y0) / (y1 - y0)
    h, s, v = hsv(a)
    # 눈 하이라이트처럼 채도 없이 아주 밝은 점은 빼고 (돌의 밝은 회색은 남게 기준을 높게)
    shine = (s < 0.12) & (v > 0.88)
    eyes = (u < 0.62) & (w > 0.22) & (w < 0.82) & (shine | (v <= 0.12))   # 눈 자리의 눈 픽셀만
    colored = alpha & ~body & ~shine & (v > 0.12)       # 몸 색도 눈도 아닌 색 = 꾸밈
    # 꾸밈에 붙은 어두운 외곽선: 꾸밈 바로 옆의 어두운 픽셀 (몸 외곽선은 꾸밈과 안 닿으면 제외)
    dark = alpha & (v <= 0.12) & ~eyes
    near = np.zeros_like(colored)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            near |= np.roll(np.roll(colored, oy, 0), ox, 1)
    decor = colored | (dark & near)
    out = a.copy()
    out[..., 3] = np.where(decor, a[..., 3], 0)
    return out, (x0, y0, x1, y1)


def place_decor(frame, decor, cbox, fade, ox=0, oy=0, paint=None):
    """프레임의 몸 범위에 맞춰 꾸밈을 늘리고 줄여 붙임 (역방향 샘플링). ox, oy = 추가로 옮길 만큼
    paint = 꾸밈을 붙일 바탕 (몸 색을 바꾼 프레임). 몸 범위는 원래 프레임으로 잡음"""
    a = np.asarray(frame).copy()
    body = body_mask(a, 0, 360) if not body_mask(a).any() else body_mask(a)
    if not body.any():
        body = a[..., 3] > 40
        if not body.any():
            return frame
    fx0, fy0, fx1, fy1 = bbox(body)
    fx0, fx1, fy0, fy1 = fx0 + ox, fx1 + ox, fy0 + oy, fy1 + oy
    cx0, cy0, cx1, cy1 = cbox
    sx = (fx1 - fx0) / (cx1 - cx0)
    sy = (fy1 - fy0) / (cy1 - cy0)
    H, W = a.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W]
    srcx = np.floor(cx0 + (xx + 0.5 - fx0) / sx).astype(int)
    srcy = np.floor(cy0 + (yy + 0.5 - fy0) / sy).astype(int)
    ok = (srcx >= 0) & (srcx < W) & (srcy >= 0) & (srcy < H)
    srcx, srcy = np.clip(srcx, 0, W - 1), np.clip(srcy, 0, H - 1)
    d = decor[srcy, srcx].astype(np.float32)
    da = np.where(ok, d[..., 3] / 255 * fade, 0)[..., None]
    base = (np.asarray(paint) if paint is not None else a).astype(np.float32)
    ba = base[..., 3:4] / 255
    oa = da + ba * (1 - da)
    rgb = (d[..., :3] * da + base[..., :3] * ba * (1 - da)) / np.maximum(oa, 1e-6)
    out = np.concatenate([rgb, oa * 255], -1)
    return Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8), "RGBA")


def save_gif(frames, path):
    w, h = frames[0].size
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(path, save_all=True, append_images=big[1:], duration=100, loop=0, disposal=2)


def main():
    cand_path, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    opts = sys.argv[4:]
    break_on_death = "break" in opts
    body = next((o[5:] for o in opts if o.startswith("body=")), None)
    cand = ImageOps.mirror(Image.open(cand_path).convert("RGBA"))      # 생성본(오른쪽) -> 왼쪽
    src_dir, dst_dir = os.path.join(ROOT, src), os.path.join(ROOT, dst)
    ref = Image.open(os.path.join(src_dir, f"{src}_idle", "frame_0.png")).convert("RGBA")
    decor, cbox = extract_decor(cand, ref)
    if body:
        hue, sat_mul, val_mul = (list(map(float, body.split(","))) + [1.0, 1.0])[:3]
        hue_src = main_hue(ref)
        tint = lambda im: recolor(im, hue, sat_mul, val_mul, hue_src)
    else:
        tint = lambda im: None
    os.makedirs(dst_dir, exist_ok=True)
    os.makedirs(os.path.join(dst_dir, "_pixellab_src"), exist_ok=True)
    cand.save(os.path.join(dst_dir, "_pixellab_src", "decor_candidate.png"))
    Image.fromarray(decor, "RGBA").save(os.path.join(dst_dir, "_pixellab_src", "decor_only.png"))

    # 갈라질 때 쓸 왼쪽/오른쪽 조각 (꾸밈 가로 가운데 기준)
    xs = np.nonzero(decor[..., 3].any(0))[0]
    mid = (xs.min() + xs.max()) // 2 if len(xs) else decor.shape[1] // 2
    left_half, right_half = decor.copy(), decor.copy()
    left_half[:, mid:, 3] = 0
    right_half[:, :mid, 3] = 0

    anims = sorted(d[len(src) + 1:] for d in os.listdir(src_dir)
                   if os.path.isdir(os.path.join(src_dir, d)) and d.startswith(src + "_"))
    for anim in anims:
        sd = os.path.join(src_dir, f"{src}_{anim}")
        n = len([p for p in os.listdir(sd) if p.startswith("frame_")])
        raw = [Image.open(os.path.join(sd, f"frame_{i}.png")).convert("RGBA") for i in range(n)]
        base_cnt = max(1, int(body_mask(np.asarray(raw[0])).sum()))
        frames = []
        for i, f in enumerate(raw):
            fade = 1.0
            if anim == "death" and break_on_death:
                # 두 조각으로 갈라져 양옆으로 떨어지며 사라짐 (앞 2프레임은 금이 간 채 그대로)
                k = max(0.0, (i - 1) / max(1, n - 4))
                if k <= 0:
                    frames.append(place_decor(f, decor, cbox, 1.0, paint=tint(f)))
                    continue
                s_ = f.width / 64
                fade = max(0.0, 1 - k)
                fr = place_decor(f, left_half, cbox, fade, round(-5 * s_ * k), round(16 * s_ * k * k), paint=tint(f))
                frames.append(place_decor(fr, right_half, cbox, fade, round(5 * s_ * k), round(14 * s_ * k * k)))
                continue
            if anim == "death":
                al = np.asarray(f)[..., 3]
                fade = 0.0 if not (al > 0).any() else min(1.0, body_mask(np.asarray(f)).sum() / base_cnt + 0.2) * al[al > 0].mean() / 255
            frames.append(place_decor(f, decor, cbox, fade, paint=tint(f)))
        dd = os.path.join(dst_dir, f"{dst}_{anim}")
        os.makedirs(dd, exist_ok=True)
        for old in os.listdir(dd):
            os.remove(os.path.join(dd, old))
        for i, fr in enumerate(frames):
            fr.save(os.path.join(dd, f"frame_{i}.png"))
        fw, fh = frames[0].size
        sheet = Image.new("RGBA", (fw * n, fh), (0, 0, 0, 0))
        for i, fr in enumerate(frames):
            sheet.paste(fr, (i * fw, 0))
        sheet.save(os.path.join(dst_dir, f"{dst}_{anim}_sheet.png"))
        save_gif(frames, os.path.join(dst_dir, f"{dst}_{anim}_preview.gif"))
        print(anim, n)
    idle0 = Image.open(os.path.join(dst_dir, f"{dst}_idle", "frame_0.png"))
    idle0.save(os.path.join(dst_dir, f"{dst}_{idle0.width}.png"))
    idle0.resize((512, 512), Image.NEAREST).save(os.path.join(dst_dir, f"{dst}_preview_512.png"))


if __name__ == "__main__":
    main()
