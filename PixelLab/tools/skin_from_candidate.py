"""PixelLab 후보 그림(같은 모양의 슬라임)의 '몸 질감'을 기존 슬라임의 모든 동작 프레임에 입힌다.
모양·동작은 원본 그대로, 몸 색과 질감만 후보 그림으로 바뀐다. 몸 밖으로 튀어나온 것(나뭇가지, 흘러내린 용암 등)도 붙인다.

    python -I skin_from_candidate.py <설정.json>

설정 예 (PixelLab/<이름>/_skin.json):
{
  "name": "slime_mud",
  "src": "slime_blue",                         // 동작을 가져올 원본 슬라임
  "texture": "_pixellab_src/texture.png",      // 몸 질감 후보 (PixelLab 생성본, 오른쪽을 봄). 이름 폴더 기준 경로
  "protrude": "_pixellab_src/twigs.png",       // (선택) 몸 밖 돌출물을 가져올 후보. 없으면 texture 것
  "defend_texture": "_pixellab_src/shell.png", // (선택) 방어 때 이 질감으로 바뀌었다가 돌아옴 (라바: 굳은 껍질)
  "embers": true,                              // (선택) 위로 오르는 불티와 연기 (라바)
  "matte": 0.6                                 // (선택) 0~1. 반짝임을 죽이고 명암을 눌러 흙처럼 (머드: "유리 같다")
}

- 질감은 몸 기준 좌표(u,w)로 샘플링해서 몸이 늘고 눌려도 따라감
- 원본 프레임의 밝기 변화(방어 때 진해짐, 스페셜 때 밝아짐)는 비율로 곱해서 살림
- 외곽선·눈·하얀 번쩍임·흰 이펙트는 원본 프레임 것 그대로
- 원본 몸 안의 어두운 선(공격 때 감은 눈 등)은 질감의 어두운 색으로 그림
- 후보 그림의 눈(과 둘레의 빛 테두리)은 질감에서 지우고 주변 질감으로 메움 (눈 감은 프레임에 유령 눈이 안 남게)
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/


def hsv(a):
    rgb = a[..., :3].astype(np.float32) / 255
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    return h, s, mx


def src_body(a):
    """원본(파란) 프레임의 몸 = 채도 있는 파랑 계열. 이펙트(밝은 하늘색/흰색)는 채도가 낮아 빠짐
    반투명도 몸으로 봄 (사망 끝에서 흐려지는 프레임이 원본 파랑 그대로 남지 않게)"""
    h, s, v = hsv(a)
    return (a[..., 3] > 0) & (s > 0.18) & (v > 0.15) & (h > 180) & (h < 235)


def bbox(m):
    ys, xs = np.nonzero(m)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def dome_box(cand, aspect):
    """후보 몸의 범위. 위쪽은 돌출물이 있을 수 있어서, 아래쪽 줄의 너비와 원본 몸 비율로 높이를 잡음"""
    a = np.asarray(cand)
    al = a[..., 3] > 128
    ys, xs = np.nonzero(al)
    y1 = ys.max() + 1
    # 바닥에서 20~35% 높이 줄들 중 가장 넓은 줄
    hgt = y1 - ys.min()
    best = (0, 0, 0)
    for y in range(int(y1 - hgt * 0.35), int(y1 - hgt * 0.15)):
        row = np.nonzero(al[y])[0]
        if len(row) and row.max() - row.min() > best[0]:
            best = (row.max() - row.min(), row.min(), row.max() + 1)
    _, x0, x1 = best
    y0 = int(round(y1 - (x1 - x0) * aspect))
    return x0, y0, x1, y1


def tex_ok(c, u, w, dark=0.12, eye_dark=0.3):
    """질감으로 쓸 수 있는 후보 픽셀인지 (외곽선, 눈, 하이라이트 점 제외).
    아주 어두운 질감(굳은 바위 껍질)은 dark, eye_dark 를 낮춰서 씀"""
    h, s, v = hsv(c)
    eyes = (u < 0.62) & (w > 0.22) & (w < 0.82) & ((v < eye_dark) | ((s < 0.15) & (v > 0.85)))
    return (c[..., 3] > 128) & (v > dark) & ~eyes


def enclosed(body):
    """몸 안에 외곽선으로 갇힌 작은 몸 색 영역 (눈동자). 몸 아래쪽 가운데에서 몸 색을 따라 퍼져서 못 닿는 곳"""
    ys, xs = np.nonzero(body)
    reach = np.zeros_like(body)
    y = ys.max()
    reach[y, xs[ys == y]] = True
    while True:
        g = reach.copy()
        for oy, ox in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            g |= np.roll(np.roll(reach, oy, 0), ox, 1)
        g &= body
        if (g == reach).all():
            return body & ~reach
        reach = g


def clean_eyes(tex, aspect, grow=3):
    """후보의 눈 자리를 지우고 주변 질감으로 메움. 눈 = 눈 영역의 아주 어둡거나 하얀 점 + 둘레 grow 픽셀"""
    box = dome_box(Image.fromarray(tex), aspect)
    H, W = tex.shape[:2]
    _, u, w, _ = sample(tex, box, box, H, W)
    h, s, v = hsv(tex)
    zone = (u > 0.1) & (u < 0.62) & (w > 0.22) & (w < 0.82) & (tex[..., 3] > 128)
    eye = zone & ((v < 0.3) | ((s < 0.15) & (v > 0.85)))
    for _ in range(grow):
        g = eye.copy()
        for oy, ox in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            g |= np.roll(np.roll(eye, oy, 0), ox, 1)
        eye = g & zone
    out = tex.astype(np.float32).copy()
    known = (tex[..., 3] > 128) & (v > 0.12) & ~eye
    hole = eye.copy()
    while hole.any():        # 바깥쪽부터 이웃 평균으로 한 겹씩 메움
        acc = np.zeros(out.shape[:2] + (3,), np.float32)
        cnt = np.zeros(out.shape[:2], np.float32)
        for oy, ox in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            k = np.roll(np.roll(known, oy, 0), ox, 1)
            acc += np.roll(np.roll(out[..., :3], oy, 0), ox, 1) * k[..., None]
            cnt += k
        fill = hole & (cnt > 0)
        if not fill.any():
            break
        out[..., :3] = np.where(fill[..., None], acc / np.maximum(cnt, 1)[..., None], out[..., :3])
        known |= fill
        hole &= ~fill
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


def to_rgb(h, s, v):
    c = v * s
    x = c * (1 - np.abs((h / 60) % 2 - 1))
    m = v - c
    k = (h // 60).astype(int) % 6
    z = np.zeros_like(c)
    table = [(c, x, z), (x, c, z), (z, c, x), (z, x, c), (x, z, c), (c, z, x)]
    out = np.zeros(h.shape + (3,), np.float32)
    for i, (r, g, b) in enumerate(table):
        sel = k == i
        for j, ch in enumerate((r, g, b)):
            out[..., j] = np.where(sel, ch, out[..., j])
    return (out + m[..., None]) * 255


def matte_tex(tex, k):
    """반짝이는 하이라이트를 죽이고 명암을 눌러 무광 흙처럼. 흙 알갱이(밝고 어두운 점)를 조금 뿌림"""
    h, s, v = hsv(tex)
    m = (tex[..., 3] > 128) & (v > 0.12)
    vm = np.median(v[m])
    v2 = vm + (v - vm) * (1 - 0.6 * k)
    v2 = np.minimum(v2, vm * (1 + 0.25 * (1 - k)))           # 하얀 반사광 상한
    s2 = np.where(s < 0.35, np.maximum(s, np.median(s[m]) * 0.9), s)   # 하이라이트의 빠진 채도 채움
    rng = np.random.default_rng(7)
    grain = rng.random(v.shape)
    v2 = np.where(grain < 0.07 * k, v2 * 0.78, np.where(grain > 1 - 0.04 * k, v2 * 1.1, v2))
    out = tex.copy()
    out[..., :3] = np.where(m[..., None], np.clip(to_rgb(h, np.clip(s2, 0, 1), np.clip(v2, 0, 1)) + 0.5, 0, 255), tex[..., :3])
    return out


def sample(img, box, fbox, H, W):
    """프레임 좌표 -> 후보 좌표 역샘플링"""
    fx0, fy0, fx1, fy1 = fbox
    cx0, cy0, cx1, cy1 = box
    yy, xx = np.mgrid[0:H, 0:W]
    u = (xx + 0.5 - fx0) / (fx1 - fx0)
    w = (yy + 0.5 - fy0) / (fy1 - fy0)
    sx = np.clip(np.floor(cx0 + u * (cx1 - cx0)).astype(int), 0, img.shape[1] - 1)
    sy = np.clip(np.floor(cy0 + w * (cy1 - cy0)).astype(int), 0, img.shape[0] - 1)
    inside = (u >= 0) & (u < 1) & (w >= -0.6) & (w < 1.1)
    return img[sy, sx], u, w, inside


def put(px, x, y, col, al):
    H, W = px.shape[:2]
    if 0 <= x < W and 0 <= y < H and al > 0:
        sa = al / 255
        da = px[y, x, 3] / 255
        oa = sa + da * (1 - sa)
        px[y, x, :3] = (np.array(col, np.float32) * sa + px[y, x, :3] * da * (1 - sa)) / max(oa, 1e-6)
        px[y, x, 3] = oa * 255


EMBER = [(255, 236, 140), (255, 170, 50), (230, 90, 30)]
SMOKE = [(92, 86, 84), (64, 60, 60)]

def embers(px, fbox, t, fade, scale):
    fx0, fy0, fx1, fy1 = fbox
    w = fx1 - fx0
    for k in range(5):
        rng = random.Random(k * 7 + 3)
        phase = ((t + rng.uniform(0, 8)) % 8) / 8
        x = int(fx0 + w * rng.uniform(0.25, 0.85) + math.sin(phase * 6 + k) * 2 * scale)
        y = int(fy0 - 1 - phase * 14 * scale)
        put(px, x, y, EMBER[min(2, int(phase * 3))], int(255 * (1 - phase * 0.7) * fade))
    for k in range(2):
        phase = ((t + k * 4) % 8) / 8
        x = int(fx0 + w * (0.45 + 0.2 * k) + phase * 3 * scale)
        y = int(fy0 - 2 - phase * 12 * scale)
        r = max(1, round((1 + phase * 1.5) * scale))
        for yy in range(-r, r + 1):
            for xx in range(-r, r + 1):
                if xx * xx + yy * yy <= r * r:
                    put(px, x + xx, y + yy, SMOKE[(xx + yy) % 2], int(120 * (1 - phase) * fade))


def save_anim(out_dir, name, anim, frames):
    d = os.path.join(out_dir, f"{name}_{anim}")
    os.makedirs(d, exist_ok=True)
    for old in os.listdir(d):
        os.remove(os.path.join(d, old))
    for i, f in enumerate(frames):
        f.save(os.path.join(d, f"frame_{i}.png"))
    w, h = frames[0].size
    sheet = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, (i * w, 0))
    sheet.save(os.path.join(out_dir, f"{name}_{anim}_sheet.png"))
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(os.path.join(out_dir, f"{name}_{anim}_preview.gif"), save_all=True, append_images=big[1:],
                duration=100, loop=0, disposal=2)
    print(anim, len(frames))


def main():
    cfg_path = sys.argv[1]
    cfg = json.load(open(cfg_path, encoding="utf-8"))
    name, src = cfg["name"], cfg["src"]
    out_dir = os.path.join(ROOT, name)
    src_dir = os.path.join(ROOT, src)
    load = lambda p: np.asarray(ImageOps.mirror(Image.open(os.path.join(out_dir, p)).convert("RGBA")))  # 생성본(오른쪽) -> 왼쪽

    ref = np.asarray(Image.open(os.path.join(src_dir, f"{src}_idle", "frame_0.png")).convert("RGBA"))
    rb = src_body(ref)
    rx0, ry0, rx1, ry1 = bbox(rb)
    aspect = (ry1 - ry0) / (rx1 - rx0)
    ref_v = hsv(ref)[2]

    tex = clean_eyes(load(cfg["texture"]), aspect)
    matte = float(cfg.get("matte", 0))
    if matte:
        tex = matte_tex(tex, matte)
    tbox = dome_box(Image.fromarray(tex), aspect)
    prot = load(cfg["protrude"]) if cfg.get("protrude") else tex
    pbox = dome_box(Image.fromarray(prot), aspect)
    dtex = load(cfg["defend_texture"]) if cfg.get("defend_texture") else None
    dbox = dome_box(Image.fromarray(dtex), aspect) if dtex is not None else None

    # 몸 밖 돌출물: 후보에서 '원본 몸 모양을 후보 크기로 옮긴 것' 바깥에 있는 픽셀
    H, W = prot.shape[:2]
    _, ru, rw, _ = sample(ref, (rx0, ry0, rx1, ry1), pbox, H, W)        # 후보 좌표계에서 본 원본 몸
    ref_dome = rb[np.clip(np.floor(ry0 + rw * (ry1 - ry0)).astype(int), 0, H - 1),
                  np.clip(np.floor(rx0 + ru * (rx1 - rx0)).astype(int), 0, W - 1)] & (ru >= 0) & (ru < 1) & (rw >= 0) & (rw < 1)
    grown = ref_dome.copy()
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            grown |= np.roll(np.roll(ref_dome, oy, 0), ox, 1)
    _, pu, pw, _ = sample(prot, pbox, pbox, H, W)
    eye_zone = (pu > 0.05) & (pu < 0.7) & (pw > 0.15) & (pw < 0.9)     # 후보의 눈·반사광이 돌출물로 딸려오지 않게
    protr = prot.copy()
    protr[..., 3] = np.where((prot[..., 3] > 128) & ~grown & ~eye_zone, prot[..., 3], 0)

    anims = sorted(d[len(src) + 1:] for d in os.listdir(src_dir)
                   if os.path.isdir(os.path.join(src_dir, d)) and d.startswith(src + "_"))
    scale = ref.shape[1] / 64
    # 질감의 밝은 색 (이펙트 색으로 씀): 질감 픽셀 밝기 상위 10% 평균
    tv = hsv(tex)[2]
    tm = (tex[..., 3] > 128) & (tv > 0.12)
    light = tex[tm & (tv >= np.percentile(tv[tm], 90))][:, :3].astype(np.float32).mean(0)
    line_col = tex[tm & (tv <= np.percentile(tv[tm], 8))][:, :3].astype(np.float32).mean(0) * 0.55   # 감은 눈 등 선 색
    for anim in anims:
        sd = os.path.join(src_dir, f"{src}_{anim}")
        n = len([p for p in os.listdir(sd) if p.startswith("frame_")])
        raw = [np.asarray(Image.open(os.path.join(sd, f"frame_{i}.png")).convert("RGBA")) for i in range(n)]
        base_cnt = max(1, int(src_body(raw[0]).sum()))
        base_mean_v = hsv(raw[0])[2][src_body(raw[0])].mean()
        frames = []
        for t, a in enumerate(raw):
            body = src_body(a)
            if not body.any():
                frames.append(Image.fromarray(a, "RGBA"))
                continue
            fbox = bbox(body)
            Hh, Ww = a.shape[:2]
            c, u, w, inside = sample(tex, tbox, fbox, Hh, Ww)
            ok = tex_ok(c, u, w) & inside
            v = hsv(a)[2]
            # 밝기 변화: 원본 프레임 밝기 / 같은 자리 원본 대기 밝기
            _, ru2, rw2, _ = sample(ref, (rx0, ry0, rx1, ry1), fbox, Hh, Ww)
            rvx = np.clip(np.floor(rx0 + ru2 * (rx1 - rx0)).astype(int), 0, Ww - 1)
            rvy = np.clip(np.floor(ry0 + rw2 * (ry1 - ry0)).astype(int), 0, Hh - 1)
            rv = np.where(rb[rvy, rvx], ref_v[rvy, rvx], base_mean_v)
            ratio = np.clip(v / np.maximum(rv, 0.05), 0.4, 1.7)[..., None]
            if matte:
                ratio = 1 + (ratio - 1) * (1 - 0.5 * matte)     # 원본 슬라임의 광택 띠도 약하게
            col = c[..., :3].astype(np.float32)
            # 방어: 껍질 질감으로 바뀌었다가 돌아옴 (프레임이 어두워진 정도로)
            if dtex is not None and anim == "defend":
                k = float(np.clip(((1 - v[body].mean() / base_mean_v) - 0.05) / 0.1, 0, 1))
                if k > 0:
                    dc, du, dw, dins = sample(dtex, dbox, fbox, Hh, Ww)
                    dok = tex_ok(dc, du, dw, dark=0.06, eye_dark=0.1) & dins
                    col = np.where(dok[..., None], col * (1 - k) + dc[..., :3] * k, col)
                    ratio = ratio * (1 - k) + 1.0 * k          # 껍질은 자기 명암 그대로
            new = np.clip(col * ratio, 0, 255)
            out = a.astype(np.float32).copy()
            m = body & ok
            out[..., :3] = np.where(m[..., None], new, out[..., :3])
            # 몸인데 질감 픽셀이 없는 자리(후보의 눈/외곽선 위치): 질감 평균색에 밝기만
            avg = tex[tex_ok(tex, *sample(tex, tbox, tbox, *tex.shape[:2])[1:3])][:, :3].mean(0)
            miss = body & ~ok
            out[..., :3] = np.where(miss[..., None], np.clip(avg * (v[..., None] / max(base_mean_v, 0.05)), 0, 255), out[..., :3])
            # 원본 몸 안의 어두운 선 (감은 눈, 입)
            line = body & (v < 0.55 * base_mean_v)
            out[..., :3] = np.where(line[..., None], line_col, out[..., :3])
            # 눈동자(외곽선에 갇힌 파란 부분)는 질감 대신 어두운 눈 색으로, 밝기 단계는 살림
            iris = enclosed(body)
            eye_col = line_col[None, None] * 0.5 + (v[..., None] * 110)
            out[..., :3] = np.where(iris[..., None], np.clip(eye_col, 0, 255), out[..., :3])
            # 남은 하늘색 이펙트(공격 점액 등)는 질감의 밝은 색으로
            hh, ss, vv = hsv(a)
            fx = (a[..., 3] > 0) & ~body & (hh > 175) & (hh < 240) & (ss > 0.04) & (vv > 0.55)
            out[..., :3] = np.where(fx[..., None], np.clip(light * (0.75 + 0.35 * vv[..., None]), 0, 255), out[..., :3])
            # 돌출물
            fade = 1.0
            if anim == "death":
                fade = min(1.0, body.sum() / base_cnt + 0.2) * (a[..., 3][a[..., 3] > 0].mean() / 255)
            pc, pu, pw, pins = sample(protr, pbox, fbox, Hh, Ww)
            pa = np.where(pins, pc[..., 3] / 255 * fade, 0)[..., None]
            oa = out[..., 3:4] / 255
            na = pa + oa * (1 - pa)
            out[..., :3] = (pc[..., :3] * pa + out[..., :3] * oa * (1 - pa)) / np.maximum(na, 1e-6)
            out[..., 3:4] = na * 255
            if cfg.get("embers"):
                embers(out, fbox, t, fade, scale)
            frames.append(Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8), "RGBA"))
        save_anim(out_dir, name, anim, frames)
    idle0 = Image.open(os.path.join(out_dir, f"{name}_idle", "frame_0.png"))
    for f in os.listdir(out_dir):
        if f.startswith(name + "_") and f[len(name) + 1:-4].isdigit():
            os.remove(os.path.join(out_dir, f))
    idle0.save(os.path.join(out_dir, f"{name}_{idle0.width}.png"))
    idle0.resize((512, 512), Image.NEAREST).save(os.path.join(out_dir, f"{name}_preview_512.png"))


if __name__ == "__main__":
    main()
