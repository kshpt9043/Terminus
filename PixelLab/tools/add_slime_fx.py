"""색을 바꾼 슬라임에 외형 이펙트를 덧그린다. recolor_monster.py 다음에 돌린다 (결과 폴더를 그 자리에서 고침).

    python -I add_slime_fx.py <이름> poison     바닥 양 끝에서 피어오르는 연기
    python -I add_slime_fx.py <이름> ice        반투명 몸 + 몸 주변 냉기 + 공격 때 얼음 조각

- 프레임은 왼쪽을 보는 그림 기준(PixelLab/<이름>/ 의 프레임과 같음). 앞 = 왼쪽
- 이펙트는 모든 동작에 들어가고, 사망에서는 몸이 흐려지는 만큼 같이 흐려짐
- 같은 입력이면 항상 같은 결과 (난수 시드 고정). 다시 돌리면 이펙트가 겹치므로 recolor 부터 다시 할 것
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/

OUTLINE_VAL = 0.12   # recolor_monster 와 같은 기준: 이보다 어두우면 외곽선/눈동자
COLOR_SAT = 0.20


def body_info(im):
    """몸 마스크, 바닥 줄 y, 바닥 줄의 왼/오른 끝 x, 몸 픽셀 수"""
    a = np.asarray(im)[..., 3]
    ys, xs = np.nonzero(a > 0)
    if len(ys) == 0:
        return None
    bottom = ys.max()
    row = xs[ys == bottom]
    return {"bottom": int(bottom), "left": int(row.min()), "right": int(row.max()),
            "top": int(ys.min()), "minx": int(xs.min()), "maxx": int(xs.max()), "count": int((a > 128).sum())}


def put(px, x, y, color, alpha):
    """한 픽셀 덮어 그리기 (기존 픽셀 위에 알파 합성)"""
    h, w = px.shape[:2]
    if not (0 <= x < w and 0 <= y < h) or alpha <= 0:
        return
    sa = alpha / 255.0
    dst = px[y, x].astype(np.float32)
    da = dst[3] / 255.0
    oa = sa + da * (1 - sa)
    if oa <= 0:
        return
    rgb = (np.array(color, np.float32) * sa + dst[:3] * da * (1 - sa)) / oa
    px[y, x] = [*rgb, oa * 255]


# ---------------------------------------------------------------- 포이즌: 바닥 연기

SMOKE_CORE = (195, 165, 215)
SMOKE_RIM = (125, 92, 150)

def edge_x(px, y, side):
    """그 줄에서 몸의 바깥 끝 x (없으면 None)"""
    if not (0 <= y < px.shape[0]):
        return None
    xs = np.nonzero(px[y, :, 3] > 0)[0]
    if len(xs) == 0:
        return None
    return int(xs.min()) if side < 0 else int(xs.max())


def fx_poison(px, info, t, fade):
    # 바닥 양 끝 바깥에서 퍼프 3개씩. 8프레임에 걸쳐 몸 옆을 타고 바깥 위로 올라가며 작아지고 옅어짐
    for dx in (-1, 1):
        for k in range(3):
            phase = (t + k * 2.7 + (0 if dx < 0 else 1.3)) % 8 / 8.0   # 0..1
            y = info["bottom"] - 1 - int(phase * 13)
            ex = edge_x(px, y, dx)
            if ex is None:
                ex = info["left"] if dx < 0 else info["right"]
            x = ex + dx * (2 + int(phase * 3))
            r = 2 if phase < 0.35 else (1 if phase < 0.75 else 0)
            alpha = int(235 * (1 - phase * 0.7) * fade)
            for yy in range(-r, r + 1):
                for xx in range(-r, r + 1):
                    if r and abs(xx) == r and abs(yy) == r:
                        continue                                  # 모서리 깎아서 둥글게
                    rim = r and (abs(xx) == r or abs(yy) == r)
                    put(px, x + xx, y + yy, SMOKE_RIM if rim else SMOKE_CORE, alpha)


# ---------------------------------------------------------------- 아이스: 반투명 + 냉기 + 얼음 조각

FROST = (225, 248, 255)
SHARD_IN = (235, 252, 255)
SHARD_OUT = (35, 90, 150)

def make_translucent(px, amount=0.82):
    rgb = px[..., :3].astype(np.float32) / 255.0
    mx, mn = rgb.max(-1), rgb.min(-1)
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    body = (mx >= OUTLINE_VAL) & (s >= COLOR_SAT) & (px[..., 3] > 0)   # 외곽선, 눈, 흰 하이라이트는 그대로
    px[..., 3] = np.where(body, px[..., 3] * amount, px[..., 3])


def fx_frost(px, info, t, fade, rng_seed):
    # 몸 둘레에서 위로 흩어지는 냉기 점 8개. 각자 6프레임 주기
    rng = random.Random(rng_seed)
    cx = (info["minx"] + info["maxx"]) / 2
    rx = (info["maxx"] - info["minx"]) / 2 + 2
    for k in range(8):
        ang = rng.uniform(0.05, 0.95) * np.pi          # 위쪽 반원 (왼쪽~오른쪽)
        start = rng.uniform(0, 6)
        phase = ((t + start) % 6) / 6.0
        r = rx + phase * 4
        hgt = info["bottom"] - info["top"]
        x = int(round(cx + np.cos(ang) * r))
        y = int(round(info["bottom"] - hgt * 0.4 - np.sin(ang) * hgt * 0.7 - phase * 5))
        alpha = int(230 * (1 - phase) * fade)
        put(px, x, y, FROST, alpha)
        if phase < 0.35:   # 갓 나온 건 십자 반짝
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                put(px, x + dx, y + dy, FROST, alpha // 2)


SHARD = ["..o..",
         ".oIo.",
         "oIIIo",
         ".oIo.",
         "..o.."]

def draw_shard(px, x, y, alpha, small=False):
    pat = ["o", "I"] if small else None
    rows = [".o.", "oIo", ".o."] if small else SHARD
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "o":
                put(px, x + i - len(row) // 2, y + j - len(rows) // 2, SHARD_OUT, alpha)
            elif ch == "I":
                put(px, x + i - len(row) // 2, y + j - len(rows) // 2, SHARD_IN, alpha)


def fx_ice_shards(px, info, t, impact_frames):
    # 몸이 앞으로 가장 늘어난 프레임들에서 앞(왼쪽) 윗부분에 얼음 조각이 튀어 위로 흩어짐.
    # 늘어난 몸이 캔버스 왼쪽 끝에 붙어서, 몸 바깥이 아니라 앞쪽 가장자리 위에 겹쳐 그림
    if t not in impact_frames:
        return
    k = impact_frames.index(t)                       # 0,1,2...
    hgt = info["bottom"] - info["top"]
    hy = info["top"] + hgt // 3
    hx = max(3, (edge_x(px, hy, -1) or info["minx"]) + 2)
    alpha = int(255 * (1 - k / (len(impact_frames) + 1)))
    draw_shard(px, hx + 1, hy - 3 - k * 3, alpha)
    draw_shard(px, hx + 6 + k * 2, hy - 6 - k * 3, alpha, small=True)
    draw_shard(px, max(1, hx - 1), hy + 4 + k, alpha, small=True)


# ---------------------------------------------------------------- 애씨드: 반투명 + 산성 연기 + 흐르는 산성액 + 방어 때 연두 빛

ACID_SMOKE = [(196, 232, 150), (120, 168, 84)]
ACID_DRIP = [(206, 255, 110), (118, 196, 50)]
ACID_GLOW = (190, 255, 90)


def fx_acid(px, info, t, fade, scale):
    w = info["maxx"] - info["minx"]
    hgt = info["bottom"] - info["top"]
    # 몸 윗면 곳곳에서 피어오르는 산성 연기 3줄기 (8프레임 주기)
    for k, rx in enumerate((0.3, 0.55, 0.78)):
        phase = ((t + k * 2.6) % 8) / 8
        x = int(info["minx"] + w * rx + (1 if k % 2 else -1) * phase * 2 * scale)
        col_top = np.nonzero(px[:, min(max(x, 0), px.shape[1] - 1), 3] > 0)[0]
        top = int(col_top.min()) if len(col_top) else info["top"]
        y = int(top - 1 - phase * 10 * scale)
        r = max(0, round((1.6 - phase * 1.6) * scale))
        al = int(210 * (1 - phase * 0.8) * fade)
        for yy in range(-r, r + 1):
            for xx in range(-r, r + 1):
                if r and abs(xx) == r and abs(yy) == r:
                    continue
                rim = r and (abs(xx) == r or abs(yy) == r)
                put(px, x + xx, y + yy, ACID_SMOKE[1] if rim else ACID_SMOKE[0], al)
    # 몸 옆을 타고 흐르는 산성액 2줄기
    for k, side in enumerate((-1, 1)):
        phase = ((t + k * 4) % 8) / 8
        y = int(info["top"] + hgt * (0.35 + 0.6 * phase))
        ex = edge_x(px, y, side)
        if ex is None:
            continue
        al = int(235 * fade)
        for j in range(max(1, round(scale))):
            put(px, ex + side, y - j, ACID_DRIP[0], al)
        put(px, ex, y - round(2 * scale), ACID_DRIP[1], al)


def fx_glow(px, strength, color):
    """몸 실루엣 바깥에 빛 테두리 (방어 때 빛남)"""
    from PIL import ImageFilter
    a = Image.fromarray(px[..., 3].astype(np.uint8))
    r1 = np.asarray(a.filter(ImageFilter.MaxFilter(3))) > 128
    r2 = np.asarray(a.filter(ImageFilter.MaxFilter(7))) > 128
    body = px[..., 3] > 128
    for mask, al in ((r2 & ~r1, 150), (r1 & ~body, 240)):
        ys, xs = np.nonzero(mask)
        for y, x in zip(ys, xs):
            put(px, x, y, color, int(al * strength))
    # 몸도 그 색 쪽으로 조금
    m = body & (px[..., :3].max(-1) > 40)
    px[..., :3] = np.where(m[..., None], px[..., :3] * (1 - 0.3 * strength) + np.array(color) * 0.3 * strength, px[..., :3])


# ---------------------------------------------------------------- 공통

def impact_frames_of(frames):
    """공격: 몸이 가장 왼쪽까지 나간 프레임과 그 다음 둘"""
    lefts = [body_info(f)["minx"] for f in frames]
    i = int(np.argmin(lefts))
    return [i, i + 1, i + 2]


def save_gif(frames, path):
    w, h = frames[0].size
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(path, save_all=True, append_images=big[1:], duration=100, loop=0, disposal=2)


def process(im, fx, t, fade, anim, impact):
    info = body_info(im)
    if info is None:
        return im
    px = np.asarray(im.convert("RGBA")).astype(np.float32).copy()
    if fx == "poison":
        fx_poison(px, info, t, fade)
    elif fx == "ice":
        make_translucent(px)
        fx_frost(px, info, t, fade, rng_seed=7)
        if anim == "attack":
            fx_ice_shards(px, info, t, impact)
    elif fx == "acid":
        scale = im.width / 64
        make_translucent(px, 0.85)
        if anim == "defend":
            # 기획: 작아지며 진해진 뒤 연두색으로 빛남 (가장 작은 구간 4~7 프레임)
            strength = {4: 0.5, 5: 1.0, 6: 1.0, 7: 0.5}.get(t, 0)
            if strength:
                fx_glow(px, strength, ACID_GLOW)
        fx_acid(px, info, t, fade, scale)
    return Image.fromarray(np.clip(px + 0.5, 0, 255).astype(np.uint8), "RGBA")


def main():
    name, fx = sys.argv[1], sys.argv[2]
    d = os.path.join(ROOT, name)
    anims = sorted(x[len(name) + 1:] for x in os.listdir(d) if os.path.isdir(os.path.join(d, x)) and x.startswith(name + "_"))
    for anim in anims:
        ad = os.path.join(d, f"{name}_{anim}")
        n = len([p for p in os.listdir(ad) if p.startswith("frame_")])
        frames = [Image.open(os.path.join(ad, f"frame_{i}.png")).convert("RGBA") for i in range(n)]
        base_count = max(1, body_info(frames[0])["count"])
        impact = impact_frames_of(frames) if anim == "attack" else []
        out = []
        for t, f in enumerate(frames):
            info = body_info(f)
            fade = 1.0 if anim != "death" or info is None else min(1.0, info["count"] / base_count) * \
                (np.asarray(f)[..., 3][np.asarray(f)[..., 3] > 0].mean() / 255.0)
            out.append(process(f, fx, t, fade if info else 0, anim, impact))
        for t, f in enumerate(out):
            f.save(os.path.join(ad, f"frame_{t}.png"))
        w, h = out[0].size
        sheet = Image.new("RGBA", (w * n, h), (0, 0, 0, 0))
        for t, f in enumerate(out):
            sheet.paste(f, (t * w, 0))
        sheet.save(os.path.join(d, f"{name}_{anim}_sheet.png"))
        save_gif(out, os.path.join(d, f"{name}_{anim}_preview.gif"))
        print(anim, n, "impact" if impact else "", impact)

    # 대표 이미지는 대기 0번 프레임 (이펙트 포함)
    idle0 = Image.open(os.path.join(d, f"{name}_idle", "frame_0.png"))
    idle0.save(os.path.join(d, f"{name}_{idle0.width}.png"))
    idle0.resize((512, 512), Image.NEAREST).save(os.path.join(d, f"{name}_preview_512.png"))


if __name__ == "__main__":
    main()
