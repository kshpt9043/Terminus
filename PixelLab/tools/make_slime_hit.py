"""기본 슬라임 피격을 역동적으로 다시 만든다 (PixelLab 생성 프레임을 바탕으로 변형).

    python -I make_slime_hit.py <원본 피격 프레임 폴더(9장)> <출력 이름, 예: slime_blue>

- 맞는 순간 몸 전체가 하얗게 번쩍 -> 뒤로 밀리며 젖혀짐 -> 반동으로 앞으로 튕김 -> 자리 잡기
- 맞는 순간 젤리 몸에서 가시가 뒤/위쪽으로 삐죽 튀어나왔다가 들어감
- 그림은 왼쪽을 봄 (앞 = 왼쪽, 뒤 = 오른쪽)
- 다른 슬라임은 이 결과를 recolor_monster.py 로 색만 바꿔 씀
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/
OUTLINE = (0, 0, 8, 255)


def transform(im, dx=0, lean=0, sx=1.0, sy=1.0, flash=0.0):
    """dx>0 = 뒤로 밀림, lean>0 = 윗부분이 뒤로 젖혀짐. 바닥 중앙 기준 찌그러짐. flash = 하얗게"""
    x0, y0, x1, y1 = im.getbbox()
    body = im.crop((x0, y0, x1, y1))
    w, h = round((x1 - x0) * sx), round((y1 - y0) * sy)
    body = body.resize((w, h), Image.NEAREST)
    if flash:
        a = np.asarray(body).astype(np.float32)
        m = (a[..., 3] > 0) & (a[..., :3].max(-1) > 30)      # 외곽선은 그대로
        a[..., :3] = np.where(m[..., None], a[..., :3] * (1 - flash) + 255 * flash, a[..., :3])
        body = Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    cx = (x0 + x1) // 2
    bx, by = cx - w // 2 + dx, y1 - h
    for y in range(h):
        t = 1 - y / h                                         # 위 1, 바닥 0
        out.alpha_composite(body.crop((0, y, w, y + 1)), (bx + round(lean * t ** 1.3), by + y))
    return out


# 가시 방향 (도, 0 = 오른쪽(뒤), -90 = 위). 뒤-위쪽으로 부채꼴. 캔버스 오른쪽 끝에 몸이 붙어서 정면 뒤(0도)는 피함
SPIKES = [(-20, 0.8), (-45, 1.0), (-70, 0.9), (-100, 0.7), (-130, 0.5)]


def add_spikes(im, length, white=False, half=2.8):
    """몸 실루엣 가장자리에서 바깥으로 가시를 그린다. 가시는 몸 뒤에 깔아서 밑동이 몸에 묻히게"""
    if length <= 0:
        return im
    a = np.asarray(im)
    mask = a[..., 3] > 128
    ys, xs = np.nonzero(mask)
    cx, cy = xs.mean(), ys.mean() + (ys.max() - ys.min()) * 0.1
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for ang, scale in SPIKES:
        r = math.radians(ang)
        ux, uy = math.cos(r), math.sin(r)
        # 중심에서 바깥으로 걸어가 실루엣 끝을 찾음
        ex, ey = cx, cy
        for step in range(64):
            px, py = int(round(cx + ux * step)), int(round(cy + uy * step))
            if not (0 <= px < im.width and 0 <= py < im.height) or not mask[py, px]:
                break
            ex, ey = px, py
        L = max(2, round(length * scale))
        tip = (ex + ux * L, ey + uy * L)
        base_in = 3                                            # 밑동은 몸 안쪽으로 들어가게
        b1 = (ex - ux * base_in - uy * half, ey - uy * base_in + ux * half)
        b2 = (ex - ux * base_in + uy * half, ey - uy * base_in - ux * half)
        # 가시 색 = 가장자리 안쪽 몸 색. 외곽선(어두움)이나 하이라이트(흰색)를 집으면 더 안쪽으로
        col = None
        for back in range(2, 12):
            sx_, sy_ = int(round(ex - ux * back)), int(round(ey - uy * back))
            if not (0 <= sx_ < im.width and 0 <= sy_ < im.height) or not mask[sy_, sx_]:
                continue
            c = a[sy_, sx_][:3].astype(int)
            if 40 < c.max() < 245 and c.max() - c.min() > 40:
                col = tuple(int(v) for v in c)
                break
        if col is None:
            col = (57, 149, 217)
        if white:
            col = (255, 255, 255)
        d.polygon([b1, tip, b2], fill=col + (255,), outline=OUTLINE)
        # 끝 하이라이트 한 점
        hx, hy = int(round(ex + ux * L * 0.55)), int(round(ey + uy * L * 0.55))
        if 0 <= hx < im.width and 0 <= hy < im.height and L >= 4:
            layer.putpixel((hx, hy), (235, 248, 255, 255))
    layer.alpha_composite(im)                                  # 몸을 위에
    return layer


def build_hit(O):
    """PixelLab 피격 프레임 9장(왼쪽을 봄) -> 역동적인 피격 10장. 64px 기준 값을 크기에 맞춰 키움"""
    k = O[0].width / 64

    # (원본 프레임, 뒤로 밀림, 젖힘, 가로, 세로, 번쩍임, 가시 길이)
    plan = [
        (0, 0, 0, 1.00, 1.00, 0.00, 0),   # 평소
        (2, 2, 5, 0.90, 1.06, 0.85, 11),   # 맞는 순간: 하얗게, 눌리며 뒤로, 가시가 튀어나옴
        (2, 3, 8, 0.90, 1.06, 0.35, 16),  # 가장 많이 젖혀짐, 가시 최대
        (3, 3, 7, 0.94, 1.04, 0.00, 10),
        (4, 2, 4, 1.00, 1.00, 0.00, 4),   # 가시가 들어감
        (5, 2, -3, 1.04, 0.97, 0.00, 0),  # 반동으로 앞으로
        (6, 1, -4, 1.05, 0.96, 0.00, 0),
        (7, 0, 1, 1.02, 0.98, 0.00, 0),
        (8, 0, 0, 1.00, 1.00, 0.00, 0),
        (0, 0, 0, 1.00, 1.00, 0.00, 0),
    ]
    frames = []
    for i, dx, l, sx, sy, fl, spike in plan:
        f = transform(O[i], round(dx * k), round(l * k), sx, sy, fl)
        frames.append(add_spikes(f, round(spike * k), white=fl > 0.5, half=2.8 * k))
    return frames


def main():
    src, name = sys.argv[1], sys.argv[2]
    O = [Image.open(os.path.join(src, f"frame_{i}.png")).convert("RGBA") for i in range(9)]
    frames = build_hit(O)

    out_dir = os.path.join(ROOT, name)
    d = os.path.join(out_dir, f"{name}_hit")
    os.makedirs(d, exist_ok=True)
    for old in os.listdir(d):
        os.remove(os.path.join(d, old))
    for i, f in enumerate(frames):
        f.save(os.path.join(d, f"frame_{i}.png"))
    n = len(frames)
    w, h = frames[0].size
    sheet = Image.new("RGBA", (w * n, h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, (i * w, 0))
    sheet.save(os.path.join(out_dir, f"{name}_hit_sheet.png"))
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), (40, 40, 48, 255)), f)
           .resize((w * 4, h * 4), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(os.path.join(out_dir, f"{name}_hit_preview.gif"), save_all=True, append_images=big[1:],
                duration=100, loop=0, disposal=2)
    print(n)


if __name__ == "__main__":
    main()
