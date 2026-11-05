"""생성본 몸의 지저분한 질감(줄무늬·반짝이 점)을 정리해서, 다른 슬라임과 같은 깔끔한 단계 명암으로 바꾼다.
몸 밝기를 부드럽게 한 뒤, 기준 슬라임 몸에 쓰인 색들로 '같은 비율'이 되게 다시 칠한다 (히스토그램 맞추기).

    python -I clean_body.py <이름> <기준 슬라임 이름> [흐림 반경] [per_frame]

예) python -I PixelLab/tools/clean_body.py slime_king slime_big_blue per_frame   # 킹: "질감이 slop 같다"

- 이름 폴더의 모든 동작 프레임·시트·GIF 를 그 자리에서 고친다 (다시 하려면 build_slime 부터)
- 몸 = 채도 있는 파랑. 외곽선, 진한 선(눈썹), 흰 번쩍임, 왕관·칼 같은 다른 색, 외곽선에 갇힌 눈동자는 그대로
- 반투명(사망 페이드)은 알파를 그대로 둔다
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # -I 로 실행해도 같은 폴더 도구를 씀
from skin_from_candidate import hsv, save_anim  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # PixelLab/


def blue_body(a):
    """몸 = 채도 있는 파랑. 몸 안의 진한 선(눈썹·눈 테두리)은 빼서 그대로 남김"""
    h, s, v = hsv(a)
    m = (a[..., 3] > 0) & (s > 0.3) & (v > 0.2) & (h > 185) & (h < 245)
    if m.any():
        m &= v >= 0.55 * np.median(v[m])
    return m


def enclosed(m, frac=0.04):
    """몸 색 덩어리 중 작은 것(눈동자 등, 전체의 frac 미만). 가장 큰 덩어리와 큰 조각은 몸으로 둠"""
    H, W = m.shape
    lab = np.zeros((H, W), np.int32)
    sizes = []
    for y0, x0 in zip(*np.nonzero(m)):
        if lab[y0, x0]:
            continue
        k = len(sizes) + 1
        stack, n = [(y0, x0)], 0
        lab[y0, x0] = k
        while stack:
            y, x = stack.pop()
            n += 1
            for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= yy < H and 0 <= xx < W and m[yy, xx] and not lab[yy, xx]:
                    lab[yy, xx] = k
                    stack.append((yy, xx))
        sizes.append(n)
    if not sizes:
        return np.zeros_like(m)
    small = [i + 1 for i, n in enumerate(sizes) if n < frac * sum(sizes)]
    return np.isin(lab, small)


def lum(rgb):
    return rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11


def box_blur(x, m, r):
    """m 안에서만 평균 (몸 밖 픽셀은 안 섞임)"""
    acc = np.zeros_like(x)
    cnt = np.zeros_like(x)
    for oy in range(-r, r + 1):
        for ox in range(-r, r + 1):
            acc += np.roll(np.roll(x * m, oy, 0), ox, 1)
            cnt += np.roll(np.roll(m.astype(np.float32), oy, 0), ox, 1)
    return np.where(m, acc / np.maximum(cnt, 1), x)


def main():
    name, ref_name = sys.argv[1], sys.argv[2]
    rest = sys.argv[3:]
    per_frame = "per_frame" in rest
    nums = [x for x in rest if x.isdigit()]
    r = int(nums[0]) if nums else 2
    out_dir = os.path.join(ROOT, name)

    # 기준 팔레트: 기준 슬라임 대기 0번의 몸 색, 밝기 순. 각 색이 몸에서 차지하는 비율도
    ref = np.asarray(Image.open(os.path.join(ROOT, ref_name, f"{ref_name}_idle", "frame_0.png")).convert("RGBA"))
    rb = blue_body(ref) & ~enclosed(blue_body(ref))
    cols, cnt = np.unique(ref[rb][:, :3], axis=0, return_counts=True)
    keep = cnt >= 3
    cols, cnt = cols[keep].astype(np.float32), cnt[keep]
    order = np.argsort(lum(cols))
    cols, cnt = cols[order], cnt[order]
    edges = np.cumsum(cnt) / cnt.sum()            # 밝기 순위 -> 색 경계

    anims = sorted(d[len(name) + 1:] for d in os.listdir(out_dir)
                   if os.path.isdir(os.path.join(out_dir, d)) and d.startswith(name + "_"))
    # 순위 기준은 대기 0번 몸 밝기 분포로 고정 (방어 때 진해지는 등 프레임 간 밝기 변화를 살림)
    base = np.asarray(Image.open(os.path.join(out_dir, f"{name}_idle", "frame_0.png")).convert("RGBA")).astype(np.float32)
    bb = blue_body(base.astype(np.uint8))
    bb &= ~enclosed(bb)
    base_l = np.sort(box_blur(lum(base[..., :3]), bb, r)[bb])

    for anim in anims:
        d = os.path.join(out_dir, f"{name}_{anim}")
        n = len([p for p in os.listdir(d) if p.startswith("frame_")])
        frames = []
        for i in range(n):
            a = np.asarray(Image.open(os.path.join(d, f"frame_{i}.png")).convert("RGBA")).astype(np.float32)
            body = blue_body(a.astype(np.uint8))
            if not body.any():
                frames.append(Image.fromarray(a.astype(np.uint8), "RGBA"))
                continue
            body &= ~enclosed(body)
            sm = box_blur(lum(a[..., :3]), body, r)
            # 밝기 순위 0~1. 기본은 대기 0번 기준(방어 때 진해지는 변화를 살림),
            # per_frame 이면 프레임마다 자기 기준 (따로 생성해서 몸 밝기가 다른 프레임이 섞였을 때: 킹 칼 꽂기)
            ref_l = np.sort(sm[body]) if per_frame else base_l
            q = np.searchsorted(ref_l, sm[body]) / max(1, len(ref_l))
            idx = np.clip(np.searchsorted(edges, q), 0, len(cols) - 1)
            a[body, :3] = cols[idx]
            frames.append(Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8), "RGBA"))
        save_anim(out_dir, name, anim, frames)
    idle0 = Image.open(os.path.join(out_dir, f"{name}_idle", "frame_0.png"))
    idle0.save(os.path.join(out_dir, f"{name}_{idle0.width}.png"))
    idle0.resize((512, 512), Image.NEAREST).save(os.path.join(out_dir, f"{name}_preview_512.png"))


if __name__ == "__main__":
    main()
