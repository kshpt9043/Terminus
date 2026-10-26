"""
PixelLab 몬스터 시트 -> 언리얼 텍스처 / 스프라이트 / 플립북 (ABP 없이 DT_MonsterVisual 로 쓰는 방식)

1) 시트 준비 (일반 파이썬, PIL)
    python -I PixelLab/tools/ue_monster_flipbooks.py prepare slime_red slime_heal ...
   -> Saved/MonsterSheets/ 에 T_<이름>_<동작>_Sheet.png + manifest.json
      PixelLab 시트는 왼쪽을 봄 -> 프레임마다 좌우만 뒤집음 (언리얼 몬스터는 오른쪽 그림을 받아 반전)

2) 언리얼 에셋 (에디터를 끈 상태에서 커맨드릿)
    UnrealEditor-Cmd.exe Terminus.uproject -run=pythonscript -script="PixelLab/tools/ue_monster_flipbooks.py" -unattended -nop4 -nosplash
   -> /Game/KSH/Characters/Monster/Sprites/<이름>/ 에 텍스처 6 + 스프라이트 + 플립북 6

3) Data/DT_MonsterVisual.csv 에 몬스터 행 추가 -> DT_MonsterVisual 다시 임포트
   자동 테스트 Terminus.Data.MonsterVisuals 가 칸 / 플립북을 검사함

주의: 스프라이트는 기존 것을 복제해서 만듦. 텍스처를 바꾸면 언리얼이 예전 텍스처 크기 비율로 UV 를 다시 맞춰서
      옆 프레임과 섞여 보임 -> source_texture_dimension 을 새 텍스처 크기로 먼저 정하고 UV 를 넣음
"""
import json
import os
import sys

ACTIONS = {"idle": "Idle", "attack": "Attack", "hit": "Hit", "defend": "Defend", "special": "Special", "death": "Death"}
HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(PROJECT, "Saved", "MonsterSheets")


def camel(name):
    return "".join(p.capitalize() for p in name.split("_"))   # slime_big_blue -> SlimeBigBlue


def prepare(names):
    from PIL import Image, ImageOps

    os.makedirs(OUT, exist_ok=True)
    manifest = {}
    for n in names:
        c = camel(n)
        manifest[c] = {"source": n, "actions": {}}
        for a, A in ACTIONS.items():
            sheet = Image.open(os.path.join(PROJECT, "PixelLab", n, "%s_%s_sheet.png" % (n, a))).convert("RGBA")
            h = sheet.height
            count = sheet.width // h
            out = Image.new("RGBA", sheet.size, (0, 0, 0, 0))
            for i in range(count):
                out.paste(ImageOps.mirror(sheet.crop((i * h, 0, (i + 1) * h, h))), (i * h, 0))
            fn = "T_%s_%s_Sheet.png" % (c, A)
            out.save(os.path.join(OUT, fn))
            manifest[c]["actions"][A] = {"file": fn, "frames": count, "size": h}
        print(c, sum(x["frames"] for x in manifest[c]["actions"].values()), "frames")
    json.dump(manifest, open(os.path.join(OUT, "manifest.json"), "w"), indent=1)


def make_assets():
    import unreal

    root = "/Game/KSH/Characters/Monster/Sprites"
    template_sprite = "/Game/KSH/Characters/Monster/SlimeBlue/S_SlimeBlue_Idle_0"
    template_flipbook = "/Game/KSH/Characters/Monster/SlimeBlue/FB_SlimeBlue_Idle"
    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    manifest = json.load(open(os.path.join(OUT, "manifest.json")))

    def get_or_duplicate(src, dst):
        return unreal.load_asset(dst) if eal.does_asset_exist(dst) else eal.duplicate_asset(src, dst)

    made = []
    for c, info in manifest.items():
        folder = "%s/%s" % (root, c)
        for action, a in info["actions"].items():
            # 텍스처 (픽셀 아트 설정)
            tex_name = "T_%s_%s_Sheet" % (c, action)
            task = unreal.AssetImportTask()
            task.filename = os.path.join(OUT, a["file"])
            task.destination_path = folder
            task.destination_name = tex_name
            task.automated = True
            task.replace_existing = True
            tools.import_asset_tasks([task])
            tex = unreal.load_asset("%s/%s" % (folder, tex_name))
            tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            made.append(tex)

            # 스프라이트 (바닥 가운데 피벗, 1px = 1cm)
            size = a["size"]
            frames = []
            for i in range(a["frames"]):
                sp = get_or_duplicate(template_sprite, "%s/S_%s_%s_%d" % (folder, c, action, i))
                sp.set_editor_property("source_texture", tex)
                sp.set_editor_property("source_texture_dimension", unreal.Vector2D(size * a["frames"], size))
                sp.set_editor_property("source_uv", unreal.Vector2D(size * i, 0))
                # 한 번 바꿨다 되돌려 렌더 데이터를 다시 굽게 (PIPELINE.md)
                sp.set_editor_property("source_dimension", unreal.Vector2D(size - 1, size))
                sp.set_editor_property("source_dimension", unreal.Vector2D(size, size))
                frames.append(sp)
                made.append(sp)

            # 플립북 (10fps)
            fb = get_or_duplicate(template_flipbook, "%s/FB_%s_%s" % (folder, c, action))
            keys = []
            for sp in frames:
                k = unreal.PaperFlipbookKeyFrame()
                k.set_editor_property("sprite", sp)
                k.set_editor_property("frame_run", 1)
                keys.append(k)
            fb.set_editor_property("key_frames", keys)
            fb.set_editor_property("frames_per_second", 10.0)
            made.append(fb)
            unreal.log("MADE %s %s frames=%d" % (c, action, len(frames)))

    for asset in made:
        eal.save_loaded_asset(asset, False)
    unreal.log("SAVED %d assets" % len(made))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "prepare":
        prepare(sys.argv[2:])
    else:
        make_assets()
