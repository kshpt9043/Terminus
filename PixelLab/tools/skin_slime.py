"""기본 슬라임의 모든 동작 프레임에 다른 표면 질감을 입혀 새 슬라임을 만든다 (모양·동작은 그대로).

    python -I skin_slime.py <원본이름> <새이름> <lava|mud|metal>

예) python -I PixelLab/tools/skin_slime.py slime_blue slime_lava lava
    python -I PixelLab/tools/skin_slime.py slime_blue slime_mud mud

- 몸 색: 원본 밝기를 질감 팔레트로 옮김(그라디언트 맵). 외곽선·눈·흰 하이라이트·하얀 번쩍임은 그대로
- 얼룩/껍질: 몸 기준 좌표에 고정된 노이즈라 몸이 움직여도 같이 따라감
- 바닥에 눌러붙은 웅덩이 + 몸 옆을 타고 흘러내리는 방울 (사망에서는 몸과 함께 흐려짐)
- 출력 구조는 recolor_monster.py 와 같음
"""
import os
import random
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/
SAT_MIN, VAL_MIN = 0.20, 0.12

SKINS = {
    "metal": {
        # 강철. 모양·동작은 기본 슬라임 그대로, 질감만 금속 (2026-10-26 팀 결정: 구르는 공 대신)
        "ramp": [(38, 42, 50), (86, 92, 104), (142, 148, 160), (196, 202, 212), (242, 245, 250)],
        "patch_mode": "none",         # 무늬 없이 금속 명암만 (가로 반사 띠는 눈 옆에서 끊겨 어색해서 뺌)
        "band": (70, 76, 88),
        "band_light": (226, 230, 238),
        "pool": None,
        "drip": None,
    },
    "lava": {
        # 어두움 -> 밝음
        "ramp": [(52, 14, 8), (128, 26, 10), (196, 58, 14), (240, 116, 24), (255, 196, 72)],
        "patch": (34, 22, 20),        # 굳은 껍질
        "patch_edge": (255, 150, 40), # 껍질 가장자리 균열(빛남)
        "patch_mode": "crust",
        "pool": [(255, 140, 30), (200, 60, 14)],
        "drip": [(255, 170, 50), (220, 80, 16)],
    },
    "mud": {
        "ramp": [(38, 24, 14), (78, 52, 30), (112, 78, 46), (146, 106, 64), (184, 144, 96)],
        "patch": (62, 40, 22),        # 진한 얼룩
        "patch_edge": None,
        "patch_mode": "blotch",
        "light": (164, 124, 78),      # 밝은 얼룩
        "pool": [(96, 66, 38), (70, 46, 26)],
        "drip": [(120, 84, 50), (82, 56, 32)],
    },
}


def hsv(rgb):
    mx, mn = rgb.max(-1), rgb.min(-1)
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    return s, mx


def ramp_color(ramp, t):
    t = np.clip(t, 0, 1) * (len(ramp) - 1)
    i = np.clip(np.floor(t).astype(int), 0, len(ramp) - 2)
    f = (t - i)[..., None]
    r = np.array(ramp, np.float32)
    return r[i] * (1 - f) + r[i + 1] * f


def noise_field(seed, n=6):
    rng = np.random.default_rng(seed)
    g = rng.random((n, n))
    # 한 번 흐리게 해서 덩어리지게
    g = (g + np.roll(g, 1, 0) + np.roll(g, 1, 1) + np.roll(g, -1, 0) + np.roll(g, -1, 1)) / 5
    return (g - g.min()) / (g.max() - g.min())


def put(px, x, y, col, a):
    h, w = px.shape[:2]
    if 0 <= x < w and 0 <= y < h and a > 0:
        sa = a / 255
        da = px[y, x, 3] / 255
        oa = sa + da * (1 - sa)
        px[y, x, :3] = (np.array(col) * sa + px[y, x, :3] * da * (1 - sa)) / max(oa, 1e-6)
        px[y, x, 3] = oa * 255


def skin_frame(im, skin, t, fade, noise, val_lo, val_hi):
    px = np.asarray(im.convert("RGBA")).astype(np.float32)
    a = px[..., 3]
    if not (a > 0).any():
        return im
    rgb = px[..., :3] / 255
    s, v = hsv(rgb)
    body = (s >= SAT_MIN) & (v >= VAL_MIN) & (a > 0)

    # 1) 밝기 -> 질감 팔레트
    tv = (v - val_lo) / max(val_hi - val_lo, 1e-6)
    px[..., :3] = np.where(body[..., None], ramp_color(skin["ramp"], tv), px[..., :3])

    # 2) 몸 기준 좌표의 얼룩/껍질
    ys, xs = np.nonzero(a > 128) if (a > 128).any() else np.nonzero(a > 0)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    H, W = a.shape
    yy, xx = np.mgrid[0:H, 0:W]
    u = np.clip((xx - x0) / max(x1 - x0, 1), 0, 0.999)
    w_ = np.clip((yy - y0) / max(y1 - y0, 1), 0, 0.999)
    # 성긴 노이즈를 몸 크기로 부드럽게 보간 -> 둥근 덩어리
    n = noise.shape[0] - 1
    fu, fw = u * n, w_ * n
    iu, iw = np.minimum(fu.astype(int), n - 1), np.minimum(fw.astype(int), n - 1)
    du, dw = fu - iu, fw - iw
    nv = (noise[iw, iu] * (1 - du) * (1 - dw) + noise[iw, iu + 1] * du * (1 - dw)
          + noise[iw + 1, iu] * (1 - du) * dw + noise[iw + 1, iu + 1] * du * dw)
    # 눈이 있는 앞(왼쪽) 가운데는 비워 둠. 표정이 가려지지 않게
    eyes = (u < 0.5) & (w_ > 0.18) & (w_ < 0.72)
    body_p = body & ~eyes
    if skin["patch_mode"] == "chrome":
        band = body_p & (w_ > 0.56) & (w_ < 0.62)
        light = body_p & (w_ >= 0.62) & (w_ < 0.66)
        px[..., :3] = np.where(band[..., None], skin["band"], px[..., :3])
        px[..., :3] = np.where(light[..., None], skin["band_light"], px[..., :3])
    elif skin["patch_mode"] == "none":
        pass
    elif skin["patch_mode"] == "crust":
        crust = body_p & (nv > 0.70) & (w_ > 0.15)
        # 껍질 테두리(바로 옆이 껍질 아닌 몸) = 빛나는 균열
        edge = crust & ~(np.roll(crust, 1, 0) & np.roll(crust, -1, 0) & np.roll(crust, 1, 1) & np.roll(crust, -1, 1))
        px[..., :3] = np.where(crust[..., None], skin["patch"], px[..., :3])
        px[..., :3] = np.where(edge[..., None], skin["patch_edge"], px[..., :3])
    else:
        dark = body_p & (nv > 0.70)
        light = body_p & (nv < 0.18) & (w_ < 0.55)
        px[..., :3] = np.where(dark[..., None], skin["patch"], px[..., :3])
        px[..., :3] = np.where(light[..., None], skin["light"], px[..., :3])

    # 3) 바닥 웅덩이: 바닥 줄 양옆으로 눌러붙어 퍼짐
    bottom = y1 - 1
    row = np.nonzero(a[bottom] > 0)[0]
    if len(row) and fade > 0 and skin["pool"]:
        l, r = row.min(), row.max()
        al = int(255 * fade)
        for x in range(l - 3, r + 4):
            put(px, x, bottom, skin["pool"][1], al)
        for x in list(range(l - 2, l + 4)) + list(range(r - 3, r + 3)):
            put(px, x, bottom - 1, skin["pool"][0], al)
        put(px, l - 4, bottom, skin["pool"][1], al // 2)
        put(px, r + 4, bottom, skin["pool"][1], al // 2)

    # 4) 몸 옆을 타고 흘러내리는 방울 2개 (8프레임 주기)
    for k, side in enumerate((-1, 1) if skin["drip"] else ()):
        phase = ((t + k * 4) % 8) / 8
        y = int(y0 + (y1 - y0) * (0.35 + 0.6 * phase))
        if not (0 <= y < H):
            continue
        rx = np.nonzero(a[y] > 0)[0]
        if not len(rx):
            continue
        x = (rx.min() if side < 0 else rx.max()) + side
        al = int(235 * fade)
        put(px, x, y, skin["drip"][0], al)
        put(px, x, y - 1, skin["drip"][1], al)
        put(px, x - side, y - 2, skin["drip"][1], al)

    return Image.fromarray(np.clip(px + 0.5, 0, 255).astype(np.uint8), "RGBA")


def save_gif(frames, path):
    w, h = frames[0].size
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(path, save_all=True, append_images=big[1:], duration=100, loop=0, disposal=2)


def body_range(im):
    """기본 이미지 몸 색의 밝기 범위 (그라디언트 맵 기준)"""
    px = np.asarray(im.convert("RGBA")).astype(np.float32)
    s, v = hsv(px[..., :3] / 255)
    m = (s >= SAT_MIN) & (v >= VAL_MIN) & (px[..., 3] > 0)
    return float(np.percentile(v[m], 2)), float(np.percentile(v[m], 98))


def main():
    src, dst, kind = sys.argv[1], sys.argv[2], sys.argv[3]
    skin = SKINS[kind]
    src_dir, dst_dir = os.path.join(ROOT, src), os.path.join(ROOT, dst)
    base_name = next(f for f in os.listdir(src_dir) if f.startswith(src + "_") and f[len(src) + 1:-4].isdigit())
    base = Image.open(os.path.join(src_dir, base_name))
    lo, hi = body_range(base)
    noise = noise_field(seed={"lava": 11, "mud": 23}.get(kind, 5))
    os.makedirs(dst_dir, exist_ok=True)

    anims = sorted(d[len(src) + 1:] for d in os.listdir(src_dir)
                   if os.path.isdir(os.path.join(src_dir, d)) and d.startswith(src + "_"))
    for anim in anims:
        sd = os.path.join(src_dir, f"{src}_{anim}")
        n = len([p for p in os.listdir(sd) if p.startswith("frame_")])
        raw = [Image.open(os.path.join(sd, f"frame_{i}.png")).convert("RGBA") for i in range(n)]
        base_cnt = max(1, int((np.asarray(raw[0])[..., 3] > 128).sum()))
        frames = []
        for t, f in enumerate(raw):
            fade = 1.0
            if anim == "death":
                al = np.asarray(f)[..., 3]
                fade = 0.0 if not (al > 0).any() else min(1.0, (al > 128).sum() / base_cnt) * al[al > 0].mean() / 255
            frames.append(skin_frame(f, skin, t, fade, noise, lo, hi))
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
