"""PixelLab 오브젝트 하나(슬라임)에서 6개 동작을 조립해 PixelLab/<이름>/ 으로 내보낸다. 크기 무관(64, 96 ...).

    python -I build_slime.py <설정.json>

설정 예 (PixelLab/<이름>/_build.json 으로 저장해 둠):
{
  "name": "slime_big_blue",
  "object": "<PixelLab 오브젝트 파일 URL 앞부분 .../objects/<계정>/<오브젝트id>>",
  "idle":   "<애니메이션 uuid>",
  "attack": "<끝 프레임 지정으로 만든 몸통박치기 uuid>",
  "hit":    "<피격 uuid (9장)>",
  "death":  "<맞는 장면 없이 쓰러지는 사망 uuid>"
}

선택 키
  "defend", "special": 생성본 uuid 를 주면 직접 만든 방어/점프 대신 생성본을 씀 (예: 장군의 검 동작)
  "defend_mode": "raise" 이면 방어 생성본을 '들어올리기'만으로 보고, 올리기 -> 마지막 자세 유지 -> 거꾸로 내리기로 조립
    (장군: 기본 자세 + 방패 든 자세를 end_frame 으로 줘서 사이만 생성. 칼 팔/몸이 같이 움직이는 걸 막음)
  "idle_eyes": [x0, y0, x1, y1] (왼쪽을 보는 기준) 대기 생성본이 눈을 자꾸 감을 때, 기본 이미지의 뜬 눈을 이 칸째로
    프레임마다 몸 움직임에 맞춰 옮겨 붙임 (장군: "눈 너무 깜빡거림")
  "defend_keep": [x0, y0, x1, y1] (왼쪽을 보는 기준) 이 칸 안의 기본 이미지 장식(갑옷띠 등)을 방어 프레임에 다시 그림.
    방패 든 자세에서 갑옷이 사라지는 걸 막음. 지금 프레임이 몸 색인 곳에만 그려서 방패는 앞에 남음
  "attack_mode": "raw" 이면 공격 생성본을 그대로 씀 (몸통박치기가 아닌 검 휘두르기 등)
  "base_image": 기본 그림을 오브젝트 대신 이 파일로 (이름 폴더 기준, 생성본 방향 그대로). 손본 그림을
                custom_start_frame 으로 애니메이션했을 때 (예: 장군에 방패를 그려 넣은 그림)

동작별 조립 (PIPELINE.md 의 시행착오를 반영)
- idle   : 생성본 그대로
- attack : 앞으로 가장 많이 나간 프레임까지만 쓰고, 이펙트 없는 복귀 3장 + 기본 -> 한 번만 박음
- hit    : make_slime_hit.build_hit (번쩍, 젖힘, 젤리 가시, 반동)
- defend : 기본 이미지로 직접 (작아지며 진해짐 -> 유지 -> 원래대로, 빛 없음)
- special: 기본 이미지로 직접 (찌그러짐 -> 점프, 최고점에서 몸 색으로 빛남 -> 착지)
- death  : 생성본 + 바닥으로 스며들며 사라지는 4장
그림은 PixelLab 생성본(오른쪽)을 뒤집어 왼쪽을 보게 저장한다 (기존 slime_* 와 같음).
"""
import io
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image, ImageChops, ImageOps

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_slime_hit import build_hit  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "curl/8.0"})
    with urllib.request.urlopen(req) as r:
        return Image.open(io.BytesIO(r.read())).convert("RGBA")


def fetch_anim(obj, uid):
    frames, i = [], 0
    while True:
        try:
            frames.append(fetch(f"{obj}/animations/{uid}/unknown/{i}.png"))
        except Exception:
            break
        i += 1
    return frames


L = ImageOps.mirror   # 생성본(오른쪽) -> 왼쪽


def body_front(im):
    """몸통(밝은 이펙트 제외)의 앞(왼쪽) 끝 x. 이펙트는 밝고 채도 낮은 픽셀로 봄"""
    a = np.asarray(im).astype(int)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    body = (al > 200) & ((mx - mn) > 40) & (mx < 250)
    xs = np.nonzero(body.any(0))[0]
    return int(xs.min()) if len(xs) else im.width


def unlean(im, k, lean, stretch=0.12):
    """왼쪽으로 늘어난 자세를 k(1=그대로, 0=거의 원래)만큼만 남기고 되돌림. 뒤(오른쪽) 끝 기준"""
    x0, y0, x1, y1 = im.getbbox()
    h = y1 - y0
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    for y in range(y0, y1):
        row = im.crop((0, y, im.width, y + 1))
        bb = row.getbbox()
        if not bb:
            continue
        t = 1 - (y - y0) / h
        w = bb[2] - bb[0]
        nw = max(1, round(w * (1 + stretch * t * k) / (1 + stretch * t)))
        seg = row.crop((bb[0], 0, bb[2], 1)).resize((nw, 1), Image.NEAREST)
        nx1 = min(im.width, bb[2] + round(lean * t * t * (1 - k)))
        out.alpha_composite(seg, (nx1 - nw, y))
    return out


def lean_forward(im, k, lean, stretch=0.12):
    """unlean 의 반대: 윗부분일수록 앞(왼쪽)으로 기울이고 늘림. 뒤(오른쪽) 끝 기준"""
    if k <= 0:
        return im
    x0, y0, x1, y1 = im.getbbox()
    h = y1 - y0
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    for y in range(y0, y1):
        row = im.crop((0, y, im.width, y + 1))
        bb = row.getbbox()
        if not bb:
            continue
        t = 1 - (y - y0) / h
        w = bb[2] - bb[0]
        nw = round(w * (1 + stretch * t * k))
        seg = row.crop((bb[0], 0, bb[2], 1)).resize((nw, 1), Image.NEAREST)
        nx1 = bb[2] - round(lean * t * t * k)
        out.alpha_composite(seg, (max(0, nx1 - nw), y))
    return out


def build_attack(raw, base, scale, extra_lean=False):
    lunge = [L(f) for f in raw]
    if extra_lean:
        # 생성본이 앞으로 거의 안 나갈 때: 몸을 직접 기울여 박는 자세를 만듦 (이펙트는 생성본 것)
        n = len(lunge)
        ramp = [min(1.0, i / max(1, n // 2)) for i in range(n)]
        lunge = [lean_forward(f, k, lean=8 * scale) for f, k in zip(lunge, ramp)]
    fronts = [body_front(f) for f in lunge]
    peak = int(np.argmin(fronts))
    # 가장 앞으로 나간 프레임이 여러 장이면 마지막 것까지 (이펙트가 다 터진 뒤)
    while peak + 1 < len(lunge) and fronts[peak + 1] <= fronts[peak]:
        peak += 1
    src = lunge[peak]
    if extra_lean:
        # 박은 프레임에는 이펙트가 섞여 있어서, 복귀는 이펙트 없는 기본 이미지를 덜 기울여 가며
        ret = [lean_forward(base, k, lean=8 * scale) for k in (0.7, 0.45, 0.2)]
    else:
        ret = [unlean(src, k, lean=8 * scale) for k in (0.7, 0.45, 0.2)]
    return lunge[:peak + 1] + ret + [base]


def scale_body(im, sx=1.0, sy=1.0, up=0, glow=0.0, dark=0.0):
    """바닥 중앙 기준 늘이기/누르기, 위로 띄우기, 밝게(glow) / 진하게(dark)"""
    x0, y0, x1, y1 = im.getbbox()
    body = im.crop((x0, y0, x1, y1))
    w, h = round((x1 - x0) * sx), round((y1 - y0) * sy)
    body = body.resize((w, h), Image.NEAREST)
    a = body.getchannel("A")
    rgb = body.convert("RGB")
    if dark:
        deep = Image.new("RGB", rgb.size, (10, 40, 150)).point(lambda v: min(255, v + 40))
        rgb = Image.blend(rgb, ImageChops.multiply(rgb, deep), dark)
    if glow:
        rgb = Image.blend(rgb, Image.new("RGB", rgb.size, (170, 230, 255)), glow)
    body = Image.merge("RGBA", (*rgb.split(), a))
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    out.alpha_composite(body, ((x0 + x1) // 2 - w // 2, y1 - h - up))
    return out


def build_defend(base):
    steps = [(1.00, 0.00), (0.96, 0.25), (0.92, 0.50), (0.90, 0.70), (0.90, 0.70), (0.90, 0.70),
             (0.90, 0.70), (0.90, 0.70), (0.93, 0.45), (0.97, 0.20), (1.00, 0.00)]
    return [scale_body(base, s, s, dark=d) for s, d in steps]


def keep_decor(frames, base, rect):
    """base 의 rect 안 몸 색이 아닌 픽셀(갑옷 등)을, 각 프레임에서 몸 색인 자리에 다시 그림"""
    def blue(a):
        rgb = a[..., :3] / 255
        mx, mn = rgb.max(-1), rgb.min(-1)
        d = np.maximum(mx - mn, 1e-6)
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
        s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
        return (a[..., 3] > 128) & (s > 0.3) & (h > 180) & (h < 240) & (mx > 0.25)
    b = np.asarray(base).astype(int)
    x0, y0, x1, y1 = rect
    box = np.zeros(b.shape[:2], bool)
    box[y0:y1, x0:x1] = True
    deco = box & (b[..., 3] > 128) & ~blue(b)
    out = []
    for f in frames:
        a = np.asarray(f).astype(int).copy()
        m = deco & blue(a)
        a[m] = b[m]
        out.append(Image.fromarray(a.astype(np.uint8), "RGBA"))
    return out


def paste_eyes(frames, base, rect, pad=6):
    """base 의 rect(뜬 눈)를 각 프레임에 붙임. 위치는 rect 둘레(투구 아래·갑옷띠)를 맞춰 찾음"""
    b = np.asarray(base).astype(np.float32)
    x0, y0, x1, y1 = rect
    H, W = b.shape[:2]
    ring = np.zeros((H, W), bool)
    ring[max(0, y0 - pad):y1 + pad, max(0, x0 - pad):x1 + pad] = True
    ring[y0:y1, x0:x1] = False
    out = []
    for f in frames:
        a = np.asarray(f).astype(np.float32)
        best = None
        for dy in range(-4, 5):
            for dx in range(-3, 4):
                sh = np.roll(np.roll(b, dy, 0), dx, 1)
                rg = np.roll(np.roll(ring, dy, 0), dx, 1) & (a[..., 3] > 128) & (sh[..., 3] > 128)
                if rg.sum() < 50:
                    continue
                err = np.abs(a[..., :3] - sh[..., :3])[rg].mean()
                if best is None or err < best[0]:
                    best = (err, dx, dy)
        _, dx, dy = best
        res = a.copy()
        patch = b[y0:y1, x0:x1]
        tgt = res[y0 + dy:y1 + dy, x0 + dx:x1 + dx]
        m = patch[..., 3] > 128
        tgt[m] = patch[m]
        out.append(Image.fromarray(res.astype(np.uint8), "RGBA"))
    return out


def build_special(base, scale):
    steps = [(1, 1, 0, 0), (1.08, 0.9, 0, 0), (0.94, 1.08, 4, 0), (1, 1, 10, 0), (1, 1, 14, 0.35),
             (1, 1, 15, 0.6), (1, 1, 14, 0.35), (1, 1, 10, 0), (0.94, 1.08, 4, 0), (1.08, 0.9, 0, 0), (1, 1, 0, 0)]
    return [scale_body(base, sx, sy, round(up * scale), glow * 0.6) for sx, sy, up, glow in steps]


def fade_sink(im, steps, scale):
    out = []
    for k in range(1, steps + 1):
        t = k / steps
        f = Image.new("RGBA", im.size, (0, 0, 0, 0))
        f.alpha_composite(im, (0, round(3 * scale * t)))
        f.putalpha(f.getchannel("A").point(lambda v: int(v * (1 - t))))
        out.append(f)
    return out


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
    cfg = json.load(open(sys.argv[1], encoding="utf-8"))
    name, obj = cfg["name"], cfg["object"]
    out_dir = os.path.join(ROOT, name)
    os.makedirs(out_dir, exist_ok=True)

    if cfg.get("base_image"):
        base = L(Image.open(os.path.join(out_dir, cfg["base_image"])).convert("RGBA"))
    else:
        base = L(fetch(f"{obj}/rotations/unknown.png"))
    scale = base.width / 64
    base.save(os.path.join(out_dir, f"{name}_{base.width}.png"))
    base.resize((512, 512), Image.NEAREST).save(os.path.join(out_dir, f"{name}_preview_512.png"))

    # 생성본 원본도 보관 (나중에 다시 조립할 수 있게)
    src_dir = os.path.join(out_dir, "_pixellab_src")
    raw = {}
    for anim in ("idle", "attack", "hit", "death", "defend", "special"):
        if anim not in cfg:
            continue
        raw[anim] = fetch_anim(obj, cfg[anim])
        d = os.path.join(src_dir, anim)
        os.makedirs(d, exist_ok=True)
        for i, f in enumerate(raw[anim]):
            L(f).save(os.path.join(d, f"frame_{i}.png"))

    idle = [L(f) for f in raw["idle"]]
    if cfg.get("idle_eyes"):
        idle = paste_eyes(idle, base, cfg["idle_eyes"])
    save_anim(out_dir, name, "idle", idle)
    if cfg.get("attack_mode") == "raw":
        save_anim(out_dir, name, "attack", [L(f) for f in raw["attack"]])
    else:
        save_anim(out_dir, name, "attack", build_attack(raw["attack"], base, scale, cfg.get("attack_extra_lean", False)))
    save_anim(out_dir, name, "hit", build_hit([L(f) for f in raw["hit"][:9]]))
    if cfg.get("defend_mode") == "raise":
        up = [L(f) for f in raw["defend"]]
        if cfg.get("defend_keep"):
            up = keep_decor(up, base, cfg["defend_keep"])
        save_anim(out_dir, name, "defend", up + [up[-1]] * 3 + up[-2::-1])
    else:
        save_anim(out_dir, name, "defend", [L(f) for f in raw["defend"]] if "defend" in raw else build_defend(base))
    save_anim(out_dir, name, "special", [L(f) for f in raw["special"]] if "special" in raw else build_special(base, scale))
    death = [L(f) for f in raw["death"]]
    save_anim(out_dir, name, "death", death + fade_sink(death[-1], 4, scale))


if __name__ == "__main__":
    main()
