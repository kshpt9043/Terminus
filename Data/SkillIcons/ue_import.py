"""
기획 스킬 아이콘(Data/SkillIcons/T_Icon_SK_*.png) -> 언리얼 텍스처 + DT_Skill / DT_Relic 다시 채우기

에디터를 끈 상태에서 커맨드릿으로
    UnrealEditor-Cmd.exe Terminus.uproject -run=pythonscript -script="Data/SkillIcons/ue_import.py" -unattended -nop4 -nosplash

1) PNG 를 /Game/KSH/Asset/SkillIcon/ 에 임포트 (64x64 픽셀 아트: Nearest, UI 그룹, 밉 없음)
2) 시스템 파이썬으로 Data/xlsx_to_csv.py -> 텍스처가 생겼으니 DT_Skill.csv 의 IconID 가 SkillID 로 채워짐
3) DT_Skill / DT_Relic 를 CSV 로 다시 채우고 저장

아이콘 PNG 는 기획 드라이브 "테르미누스/스킬 아이콘" 폴더 (직업/계층 하위 폴더) 를 평평하게 복사한 것
"""
import glob
import os
import subprocess

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.dirname(HERE)
FOLDER = "/Game/KSH/Asset/SkillIcon"
TABLES = {"/Game/KSH/Data/DT_Skill": "DT_Skill.csv", "/Game/KSH/Data/DT_Relic": "DT_Relic.csv"}

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# 1) 텍스처
pngs = sorted(glob.glob(os.path.join(HERE, "T_Icon_*.png")))
tasks = []
for f in pngs:
    task = unreal.AssetImportTask()
    task.filename = f
    task.destination_path = FOLDER
    task.destination_name = os.path.splitext(os.path.basename(f))[0]
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
tools.import_asset_tasks(tasks)

for f in pngs:
    path = "%s/%s" % (FOLDER, os.path.splitext(os.path.basename(f))[0])
    tex = unreal.load_asset(path)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    eal.save_loaded_asset(tex, False)
unreal.log("SkillIcon: textures %d" % len(pngs))

# 2) CSV (IconID 채우기). 언리얼 파이썬엔 openpyxl 이 없어서 시스템 파이썬으로
subprocess.check_call(["python", "-X", "utf8", os.path.join(DATA, "xlsx_to_csv.py")])

# 3) DT
for asset, csv_name in TABLES.items():
    dt = unreal.load_asset(asset)
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(dt, os.path.join(DATA, csv_name))
    eal.save_loaded_asset(dt, False)
    unreal.log("SkillIcon: %s <- %s %s" % (asset, csv_name, "OK" if ok else "FAILED"))
