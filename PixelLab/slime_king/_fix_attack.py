"""슬라임 킹 공격 다시 조립 (생성본 검이 뒤쪽으로 휘둘러져서: "검 궤도가 일반적인 공격과 반대").

    python -I PixelLab/slime_king/_fix_attack.py
    python -I PixelLab/tools/build_slime.py PixelLab/slime_king/_build.json
    python -I PixelLab/tools/clean_body.py slime_king slime_big_blue per_frame

- 생성본 공격 0~4 (검을 머리 위로 드는 데까지)
- animate_image: 4번(머리 위) -> 앞으로 내려친 자세(inpaint) 보간 = 내려치기
- animate_image: 내려친 자세 -> 기본 자세 보간 = 복귀
- 머리 위 프레임의 뒤로 그어진 옛 궤적은 지우고, 내려치는 프레임 앞에 흰 호를 그림
결과: _pixellab_src/attack_fixed/frame_N.png (오른쪽을 봄). _build.json 의 attack_local 이 이걸 씀
"""
import io
import os
import urllib.request

import numpy as np
from PIL import Image, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "_pixellab_src")
SLASH_JOB = "bed1b6ec-e347-448a-a3b5-eed4cd23b0ea"    # 머리 위 -> 앞으로 내려침
RETURN_JOB = "4802f572-dd41-414e-a6e3-c3b9d3582198"   # 내려친 자세 -> 기본 자세


def get(job, i):
    req = urllib.request.Request(f"https://api.pixellab.ai/mcp/images/{job}/download?index={i}",
                                 headers={"User-Agent": "curl/8.0"})
    with urllib.request.urlopen(req) as r:
        return Image.open(io.BytesIO(r.read())).convert("RGBA")


def hsv(a):
    rgb = a[..., :3] / 255
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    return h, s, mx


def erase_old_trail(img):
    """뒤로 그어진 흰·하늘색 궤적 (투구 위쪽, 그리고 몸 뒤 왼쪽 끝 조각). 검날은 회색이라 남음"""
    a = np.asarray(img).astype(np.float32)
    h, s, v = hsv(a)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    vis = a[..., 3] > 0
    white = vis & (s < 0.25) & (v > 0.85)
    cyan = vis & (s > 0.2) & (h > 165) & (h < 205) & (v > 0.8)
    pale = vis & (s < 0.35) & (v > 0.8)
    trail = ((white | cyan) & (yy < 36)) | (pale & (xx < 26) & (yy < 56))
    a[trail, 3] = 0
    return Image.fromarray(a.astype(np.uint8), "RGBA")


def slash_arc(img, alpha, a0, a1=75, cx=96, cy=70, R=40):
    """앞(오른쪽)에 위에서 아래로 그어지는 흰 호. 끝으로 갈수록 두꺼움"""
    a = np.asarray(img).astype(np.float32)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    ang = np.degrees(np.arctan2(yy - cy, xx - cx))
    r = np.hypot(xx - cx, yy - cy)
    t = np.clip((ang - a0) / (a1 - a0), 0, 1)
    inside = (ang >= a0) & (ang <= a1)
    w = 2 + 6 * t
    core = inside & (r <= R) & (r >= R - w)
    edge = inside & (r <= R + 1) & (r >= R - w - 1) & ~core
    for m, col, al in ((edge, (150, 220, 255), 0.85), (core, (255, 255, 255), 1.0)):
        k = np.where(m, al * alpha * (0.7 + 0.3 * t), 0)[..., None]
        oa = a[..., 3:4] / 255
        na = k + oa * (1 - k)
        a[..., :3] = (np.array(col, np.float32) * k + a[..., :3] * oa * (1 - k)) / np.maximum(na, 1e-6)
        a[..., 3:4] = na * 255
    return Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8), "RGBA")


def main():
    head = [ImageOps.mirror(Image.open(os.path.join(SRC, "attack", f"frame_{i}.png")).convert("RGBA")) for i in range(5)]
    head[3:] = [erase_old_trail(f) for f in head[3:]]
    slash = [erase_old_trail(get(SLASH_JOB, i)) for i in range(1, 5)]
    ret = [get(RETURN_JOB, i) for i in range(1, 5)]
    # 내려치는 프레임(slash 1~3)에 앞쪽 호: 밝게 -> 흐리게
    slash[1] = slash_arc(slash[1], 1.0, -80)
    slash[2] = slash_arc(slash[2], 0.7, -40)
    slash[3] = slash_arc(slash[3], 0.35, 0)
    frames = head + slash + [slash[-1]] + ret
    out = os.path.join(SRC, "attack_fixed")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        os.remove(os.path.join(out, f))
    for i, f in enumerate(frames):
        f.save(os.path.join(out, f"frame_{i}.png"))
    print("attack_fixed", len(frames))


if __name__ == "__main__":
    main()
