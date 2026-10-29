# 유물 아이템 본체를 픽셀랩 pixflux 로 생성 (raw/<RowName>.png, 투명 배경 56x56)
# 사용: PIXELLAB_KEY=... python gen.py [RowName ...]
import base64, json, os, sys, time, urllib.request, urllib.error
from concurrent.futures import ThreadPoolExecutor
from prompts import P, STYLE

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "raw")
os.makedirs(RAW, exist_ok=True)
KEY = os.environ["PIXELLAB_KEY"]
SIZE = 56

def gen(row):
    out = os.path.join(RAW, row + ".png")
    if os.path.exists(out):
        return row, "skip"
    body = json.dumps({
        "description": STYLE.format(P[row]),
        "image_size": {"width": SIZE, "height": SIZE},
        "no_background": True,
        "outline": "single color black outline",
        "shading": "medium shading",
        "detail": "highly detailed",
        "text_guidance_scale": 8,
    }).encode()
    for attempt in range(5):
        req = urllib.request.Request("https://api.pixellab.ai/v2/create-image-pixflux", data=body,
            headers={"Authorization": "Bearer " + KEY, "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=180) as r:
                d = json.load(r)
            b64 = d["image"]["base64"].split(",")[-1]
            open(out, "wb").write(base64.b64decode(b64))
            return row, "ok"
        except urllib.error.HTTPError as e:
            msg = f"HTTP {e.code} {e.read()[:200]}"
            if e.code in (429, 529, 500, 502, 503):
                time.sleep(10 * (attempt + 1)); continue
            return row, msg
        except Exception as e:
            msg = repr(e); time.sleep(5)
    return row, "fail " + msg

rows = sys.argv[1:] or list(P)
with ThreadPoolExecutor(6) as ex:
    futs = []
    for r in rows:
        futs.append(ex.submit(gen, r)); time.sleep(0.5)
    for f in futs:
        print(*f.result(), flush=True)
