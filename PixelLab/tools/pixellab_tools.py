"""PixelLab 몬스터 스프라이트 후처리 도구. 사용법은 PixelLab/PIPELINE.md 참고.

    python PixelLab/tools/pixellab_tools.py export <config.json> [--out DIR] [--ue-dir DIR] [--review PNG]
    python PixelLab/tools/pixellab_tools.py lunge-end <기본 이미지 경로|URL> <out.png>
    python PixelLab/tools/pixellab_tools.py ue-sheets <PixelLab/이름 폴더> <out DIR>

export 설정 파일 예: PixelLab/tools/configs/slime_blue.json
  animations[].kind
    pixellab : PixelLab 애니메이션 프레임을 받음. uuid 필수, frames 는 비우면 404 날 때까지 받음
               pingpong=true  -> 끝까지 갔다가 거꾸로 돌아옴 (공격처럼 원위치 복귀)
               fade_out=N     -> 마지막 프레임 뒤에 서서히 가라앉으며 사라지는 N 장 (사망)
    defend   : 기본 이미지로 직접 제작. 작아지며 진해졌다가 원래대로
    hop      : 기본 이미지로 직접 제작. 위로 점프, 최고점에서 발광 (스페셜)

PixelLab 생성본은 오른쪽을 본다. PixelLab/<이름>/ 에는 기획서대로 왼쪽을 보게 뒤집어 저장하고,
언리얼용 시트(--ue-dir, ue-sheets)는 뒤집지 않은 오른쪽 그림으로 만든다.
"""
import argparse
import base64
import io
import json
import os
import sys
import urllib.error
import urllib.request

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageOps

# PixelLab 파일 서버는 파이썬 기본 User-Agent 를 403 으로 막음
USER_AGENT = "curl/8.0"
PREVIEW_BG = (40, 40, 48, 255)


# ---------------------------------------------------------------- 받기

def load_image(src):
    """경로 또는 URL 에서 RGBA 이미지"""
    if src.startswith(("http://", "https://")):
        req = urllib.request.Request(src, headers={"User-Agent": USER_AGENT})
        with urllib.request.urlopen(req) as r:
            return Image.open(io.BytesIO(r.read())).convert("RGBA")
    return Image.open(src).convert("RGBA")


def fetch_animation(object_url, uuid, frames=None):
    """1방향 오브젝트 애니메이션 프레임. frames 가 없으면 없는 번호가 나올 때까지"""
    out = []
    i = 0
    while frames is None or i < frames:
        try:
            out.append(load_image(f"{object_url}/animations/{uuid}/unknown/{i}.png"))
        except urllib.error.HTTPError as e:
            if frames is None and e.code in (403, 404):
                break
            raise
        i += 1
    if not out:
        raise RuntimeError(f"애니메이션 {uuid} 프레임을 하나도 못 받음")
    return out


# ---------------------------------------------------------------- 직접 제작

def transform(im, sx=1.0, sy=1.0, up=0, glow=0.0, glow_color=(170, 230, 255)):
    """바닥 중앙 기준으로 늘리고/누르고, 위로 띄우고, 밝게 빛나게"""
    x0, y0, x1, y1 = im.getbbox()
    body = im.crop((x0, y0, x1, y1))
    w, h = round((x1 - x0) * sx), round((y1 - y0) * sy)
    body = body.resize((w, h), Image.NEAREST)
    if glow:
        a = body.getchannel("A")
        lit = Image.blend(body.convert("RGB"), Image.new("RGB", body.size, tuple(glow_color)), glow)
        lit = ImageEnhance.Brightness(lit).enhance(1 + glow * 0.3)
        body = Image.merge("RGBA", (*lit.split(), a))
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    cx = (x0 + x1) // 2
    out.alpha_composite(body, (cx - w // 2, y1 - h - up))
    return out


# (가로 배율, 세로 배율, 위로 px, 발광 0~1)
HOP_STEPS = [
    (1, 1, 0, 0), (1.08, 0.9, 0, 0), (0.94, 1.08, 4, 0), (1, 1, 10, 0), (1, 1, 14, 0.35),
    (1, 1, 15, 0.6), (1, 1, 14, 0.35), (1, 1, 10, 0), (0.94, 1.08, 4, 0), (1.08, 0.9, 0, 0), (1, 1, 0, 0),
]


def hop_frames(base, steps=HOP_STEPS, glow_color=(170, 230, 255)):
    """스페셜: 살짝 웅크렸다 점프, 최고점에서 발광, 착지"""
    return [transform(base, *s, glow_color=glow_color) for s in steps]


# (크기, 진해짐 0~1)
DEFEND_STEPS = [
    (1.00, 0.00), (0.96, 0.25), (0.92, 0.50), (0.90, 0.70), (0.90, 0.70), (0.90, 0.70),
    (0.90, 0.70), (0.90, 0.70), (0.93, 0.45), (0.97, 0.20), (1.00, 0.00),
]


def defend_frames(base, steps=DEFEND_STEPS, deep=(10, 40, 150)):
    """방어: 바닥 중앙 기준으로 작아지며 진해짐 -> 유지 -> 원래 크기/색"""
    x0, y0, x1, y1 = base.getbbox()
    cx = (x0 + x1) // 2
    # 밝기를 낮추고 진한 색 쪽으로 섞음 (외곽선/눈동자 같은 어두운 픽셀은 거의 그대로)
    tint = tuple(min(255, v + 40) for v in deep)
    frames = []
    for scale, dark in steps:
        body = base.crop((x0, y0, x1, y1))
        w, h = round((x1 - x0) * scale), round((y1 - y0) * scale)
        body = body.resize((w, h), Image.NEAREST)
        a = body.getchannel("A")
        rgb = body.convert("RGB")
        if dark:
            rgb = Image.blend(rgb, ImageChops.multiply(rgb, Image.new("RGB", rgb.size, tint)), dark)
        body = Image.merge("RGBA", (*rgb.split(), a))
        out = Image.new("RGBA", base.size, (0, 0, 0, 0))
        out.alpha_composite(body, (cx - w // 2, y1 - h))
        frames.append(out)
    return frames


def fade_sink(im, steps, sink=3):
    """마지막 프레임이 바닥으로 sink px 가라앉으며 사라지는 steps 장"""
    frames = []
    for k in range(1, steps + 1):
        t = k / steps
        f = Image.new("RGBA", im.size, (0, 0, 0, 0))
        f.alpha_composite(im, (0, round(sink * t)))
        f.putalpha(f.getchannel("A").point(lambda v: int(v * (1 - t))))
        frames.append(f)
    return frames


def lunge_end_frame(base, stretch=0.12, lean=8, left=1):
    """공격(몸통박치기) 끝 자세. 바닥은 고정, 윗부분일수록 앞(오른쪽)으로 늘어나고 기울어짐.
    animate_object 의 end_frame_base64 로 넘겨 보간시킨 뒤 pingpong 으로 원위치 복귀"""
    x0, y0, x1, y1 = base.getbbox()
    h, bw = y1 - y0, x1 - x0
    body = base.crop((x0, y0, x1, y1))
    out = Image.new("RGBA", base.size, (0, 0, 0, 0))
    for y in range(h):
        t = 1 - y / h   # 위쪽 1, 바닥 0
        row = body.crop((0, y, bw, y + 1)).resize((round(bw * (1 + stretch * t)), 1), Image.NEAREST)
        out.alpha_composite(row, (left + round(lean * t * t), y0 + y))
    return out


# ---------------------------------------------------------------- 쓰기

def write_sheet(frames, path):
    w, h = frames[0].size
    sheet = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, (i * w, 0))
    sheet.save(path)


def write_gif(frames, path, scale, fps):
    w, h = frames[0].size
    big = [Image.alpha_composite(Image.new("RGBA", (w, h), PREVIEW_BG), f)
           .resize((w * scale, h * scale), Image.NEAREST).convert("P", palette=Image.ADAPTIVE) for f in frames]
    big[0].save(path, save_all=True, append_images=big[1:], duration=round(1000 / fps), loop=0, disposal=2)


def write_review(rows, path, cell=128):
    """동작별 한 줄 접촉 시트 (사람 검토용)"""
    m = max(len(f) for _, f in rows)
    rev = Image.new("RGBA", (80 + m * (cell + 3), len(rows) * (cell + 6) + 6), (230, 230, 235, 255))
    dr = ImageDraw.Draw(rev)
    for r, (a, frames) in enumerate(rows):
        y = 6 + r * (cell + 6)
        dr.text((6, y + cell // 2), f"{a} ({len(frames)})", fill=(0, 0, 0, 255))
        for i, f in enumerate(frames):
            x = 80 + i * (cell + 3)
            dr.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(250, 250, 252, 255))
            rev.alpha_composite(f.resize((cell, cell), Image.NEAREST), (x, y))
    rev.save(path)


# ---------------------------------------------------------------- 명령

def build_animation(spec, base, object_url):
    kind = spec["kind"]
    if kind == "pixellab":
        frames = fetch_animation(object_url, spec["uuid"], spec.get("frames"))
        if spec.get("pingpong"):
            frames = frames + frames[-2::-1]
        n = spec.get("fade_out", 0)
        if n and frames[-1].getbbox():
            frames = frames + fade_sink(frames[-1], n)
        return frames
    if kind == "defend":
        return defend_frames(base, deep=tuple(spec.get("deep", (10, 40, 150))))
    if kind == "hop":
        return hop_frames(base, glow_color=tuple(spec.get("glow_color", (170, 230, 255))))
    raise ValueError(f"모르는 kind: {kind}")


def cmd_export(args):
    with open(args.config, encoding="utf-8") as f:
        cfg = json.load(f)
    name = cfg["name"]
    object_url = cfg["object_url"].rstrip("/")
    fps = cfg.get("fps", 10)
    scale = cfg.get("preview_scale", 4)
    out_root = args.out or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), name)

    base = load_image(f"{object_url}/rotations/unknown.png")   # 오른쪽을 봄
    flip = ImageOps.mirror

    os.makedirs(out_root, exist_ok=True)
    flip(base).save(os.path.join(out_root, f"{name}_64.png"))
    flip(base).resize((512, 512), Image.NEAREST).save(os.path.join(out_root, f"{name}_preview_512.png"))
    if args.ue_dir:
        os.makedirs(args.ue_dir, exist_ok=True)

    rows = []
    for spec in cfg["animations"]:
        a = spec["name"]
        frames = build_animation(spec, base, object_url)
        if args.ue_dir:
            write_sheet(frames, os.path.join(args.ue_dir, f"T_{cfg.get('ue_name', name)}_{a.capitalize()}_Sheet.png"))

        frames = [flip(f) for f in frames]   # 기획서대로 왼쪽
        d = os.path.join(out_root, f"{name}_{a}")
        if os.path.isdir(d):
            for old in os.listdir(d):
                os.remove(os.path.join(d, old))
        os.makedirs(d, exist_ok=True)
        for i, f in enumerate(frames):
            f.save(os.path.join(d, f"frame_{i}.png"))
        write_sheet(frames, os.path.join(out_root, f"{name}_{a}_sheet.png"))
        write_gif(frames, os.path.join(out_root, f"{name}_{a}_preview.gif"), scale, fps)
        rows.append((a, frames))

    if args.review:
        write_review(rows, args.review)
    print("ok", out_root, [(a, len(f)) for a, f in rows])


def cmd_lunge_end(args):
    out = lunge_end_frame(load_image(args.base), args.stretch, args.lean, args.left)
    out.save(args.out)
    out.resize((out.width * 4, out.height * 4), Image.NEAREST).save(os.path.splitext(args.out)[0] + "_x4.png")
    with open(args.out, "rb") as f:
        b64 = base64.b64encode(f.read()).decode()
    print(f"saved {args.out} (+ _x4 미리보기). end_frame_base64 길이 {len(b64)}:")
    print(b64)


def cmd_ue_sheets(args):
    """PixelLab/<이름>/ 의 왼쪽 시트를 프레임별로 다시 뒤집어 오른쪽 그림 시트로"""
    src = args.src.rstrip("/\\")
    name = os.path.basename(src)
    prefix = args.ue_name or name
    os.makedirs(args.out, exist_ok=True)
    made = []
    for fn in sorted(os.listdir(src)):
        if not (fn.startswith(name + "_") and fn.endswith("_sheet.png")):
            continue
        anim = fn[len(name) + 1:-len("_sheet.png")]
        sh = load_image(os.path.join(src, fn))
        size = sh.height
        frames = [ImageOps.mirror(sh.crop((i * size, 0, i * size + size, size))) for i in range(sh.width // size)]
        out = os.path.join(args.out, f"T_{prefix}_{anim.capitalize()}_Sheet.png")
        write_sheet(frames, out)
        made.append((os.path.basename(out), len(frames)))
    print("ok", made)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("export", help="설정 파일대로 받아서 PixelLab/<이름>/ 에 프레임·시트·GIF 저장")
    e.add_argument("config")
    e.add_argument("--out", help="기본: PixelLab/<name>")
    e.add_argument("--ue-dir", help="오른쪽 그림 언리얼 시트(T_<ue_name>_<동작>_Sheet.png)도 여기에")
    e.add_argument("--review", help="동작별 접촉 시트 PNG 경로")
    e.set_defaults(func=cmd_export)

    l = sub.add_parser("lunge-end", help="공격 끝 자세 PNG + base64 (animate_object end_frame_base64 용)")
    l.add_argument("base", help="오른쪽을 보는 기본 이미지 경로 또는 URL (…/rotations/unknown.png)")
    l.add_argument("out")
    l.add_argument("--stretch", type=float, default=0.12, help="맨 위가 가로로 늘어나는 비율")
    l.add_argument("--lean", type=int, default=8, help="맨 위가 앞으로 기우는 px")
    l.add_argument("--left", type=int, default=1, help="몸통 왼쪽 끝 x (캔버스 밖으로 안 나가게)")
    l.set_defaults(func=cmd_lunge_end)

    u = sub.add_parser("ue-sheets", help="PixelLab/<이름>/ 시트로 오른쪽 그림 언리얼 시트 만들기")
    u.add_argument("src", help="PixelLab/<이름> 폴더")
    u.add_argument("out")
    u.add_argument("--ue-name", help="T_<이것>_<동작>_Sheet.png. 기본: 폴더 이름")
    u.set_defaults(func=cmd_ue_sheets)

    args = p.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    sys.exit(main())
