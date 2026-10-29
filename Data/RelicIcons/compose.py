# raw/<RowName>.png (투명 아이템) -> out/T_Relic_<RowName>.png (64x64, 어두운 배경 + 등급색 후광)
import csv, os, sys
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
RAW, OUT = os.path.join(HERE, "raw"), os.path.join(HERE, "out")
os.makedirs(OUT, exist_ok=True)
CSV = sys.argv[1]
S = 64

# 등급별 후광 색
TIER = {
    "Basic":   (205, 214, 228),  # 은백
    "Upgrade": (255, 196, 64),   # 금
    "Surface": (88, 210, 110),   # 초록 (표층)
    "Mid":     (70, 150, 255),   # 파랑 (중층)
    "Deep":    (186, 92, 255),   # 보라 (심층)
}
BG_IN, BG_OUT = (30, 26, 38), (12, 10, 16)

def lerp(a, b, t): return tuple(round(x + (y - x) * t) for x, y in zip(a, b))

def compose(item, col):
    # 어두운 배경: 가운데가 살짝 밝은 계단식 비네트
    img = Image.new("RGB", (S, S))
    px = img.load()
    c = (S - 1) / 2
    for y in range(S):
        for x in range(S):
            d = max(abs(x - c), abs(y - c)) / c  # 사각 거리
            r = ((x - c) ** 2 + (y - c) ** 2) ** .5 / (c * 1.42)
            t = min(1, round(r * 6) / 6)
            px[x, y] = lerp(BG_IN, BG_OUT, t)
    # 등급색 원형 은은한 광원 (단계 끊어서 픽셀 느낌)
    for y in range(S):
        for x in range(S):
            r = ((x - c) ** 2 + (y - c) ** 2) ** .5 / c
            g = max(0, 1 - r) ** 1.6
            g = round(g * 5) / 5 * 0.30
            px[x, y] = lerp(px[x, y], col, g)

    # 아이템 놓을 자리
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bb = item.getbbox()
    item = item.crop(bb)
    layer.paste(item, ((S - item.width) // 2, (S - item.height) // 2))
    a = layer.split()[3].point(lambda v: 255 if v > 40 else 0)

    # 외곽 후광: 실루엣을 바깥으로 1px 씩 키운 고리 3겹, 바깥으로 갈수록 흐리게
    rings = [(a.filter(ImageFilter.MaxFilter(2 * k + 1)), k) for k in (1, 2, 3, 4)]
    strength = {1: 0.95, 2: 0.65, 3: 0.40, 4: 0.20}
    prev = a
    for m, k in rings:
        mp, pp = m.load(), prev.load()
        for y in range(S):
            for x in range(S):
                if mp[x, y] and not pp[x, y]:
                    px[x, y] = lerp(px[x, y], col if k > 1 else lerp(col, (255, 255, 255), .35), strength[k])
        prev = m

    img = img.convert("RGBA")
    img.alpha_composite(layer)
    # 1px 테두리
    bp = img.load()
    edge = lerp((8, 6, 12), col, .25)
    for i in range(S):
        for p in ((i, 0), (i, S - 1), (0, i), (S - 1, i)):
            bp[p] = edge + (255,)
    return img.convert("RGB")

rows = list(csv.DictReader(open(CSV, encoding="utf-8-sig")))
only = set(sys.argv[2:])
n = 0
for r in rows:
    name = r["RowName"]
    if only and name not in only: continue
    src = os.path.join(RAW, name + ".png")
    if not os.path.exists(src): continue
    compose(Image.open(src).convert("RGBA"), TIER[r["RelicTier"]]).save(os.path.join(OUT, f"T_Relic_{name}.png"))
    n += 1
print("composed", n)
