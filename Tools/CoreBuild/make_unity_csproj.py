# 유니티를 켜지 않고 Terminus.Game / Terminus.UI 를 컴파일해 보는 개인용 csproj 만들기
# 유니티가 만든 <어셈블리>.csproj 는 파일을 하나하나 적어 둬서 새 파일이 안 들어감 -> 와일드카드로 바꾸고
# 프로젝트 참조는 유니티가 이미 컴파일한 dll(Library/ScriptAssemblies) 참조로 바꿈
#
# 사용:
#   python make_unity_csproj.py Terminus.Game <출력 폴더>
#   python make_unity_csproj.py Terminus.UI <출력 폴더> --ref Terminus.Game=<Game 출력 폴더>\out\Terminus.Game.dll  (Game 을 고쳤을 때)
#   cd <출력 폴더> && dotnet build <어셈블리>.Check.csproj -o out -p:BaseIntermediateOutputPath=obj/
import argparse
import glob
import os
import re

ROOT = r"C:\Unity Projects\Terminus"
SOURCES = {
    "Terminus.Game": r"Assets\Terminus\Runtime\Game\**\*.cs",
    "Terminus.UI": r"Assets\Terminus\Runtime\UI\**\*.cs",
    "Terminus.Core": r"Assets\Terminus\Runtime\Core\**\*.cs",
    "Terminus.Art": r"Assets\Terminus\Runtime\Art\**\*.cs",
}

ap = argparse.ArgumentParser()
ap.add_argument("assembly", choices=sorted(SOURCES))
ap.add_argument("out")
ap.add_argument("--ref", action="append", default=[], help="Name=dll 경로 (기본 Library/ScriptAssemblies 대신)")
args = ap.parse_args()

overrides = dict(r.split("=", 1) for r in args.ref)
s = open(os.path.join(ROOT, args.assembly + ".csproj"), encoding="utf-8").read()
s = re.sub(r'\s*<Compile Include="[^"]+" />', "", s)


def to_ref(m):
    name = os.path.splitext(os.path.basename(m.group(1)))[0]
    path = overrides.get(name, os.path.join(ROOT, "Library", "ScriptAssemblies", name + ".dll"))
    return f'<Reference Include="{name}"><HintPath>{path}</HintPath></Reference>'


s = re.sub(r'<ProjectReference Include="([^"]+)"[^>]*>.*?</ProjectReference>', to_ref, s, flags=re.S)
s = re.sub(r'<ProjectReference Include="([^"]+)"\s*/>', to_ref, s)
# 이미 dll 로 참조된 것도 덮어쓰기
for name, path in overrides.items():
    s = re.sub(rf'(<Reference Include="{re.escape(name)}"[^>]*>\s*<HintPath>)[^<]+(</HintPath>)', lambda m, p=path: m.group(1) + p + m.group(2), s)

extra = [f'    <Compile Include="{os.path.join(ROOT, SOURCES[args.assembly])}" />']
facepunch = glob.glob(os.path.join(ROOT, r"Library\PackageCache\com.community.netcode.transport.facepunch@*\Runtime\Facepunch\Facepunch.Steamworks.Win64.dll"))
if facepunch and "Facepunch.Steamworks.Win64" not in s:
    extra.append(f'    <Reference Include="Facepunch.Steamworks.Win64"><HintPath>{facepunch[0]}</HintPath></Reference>')
transport = os.path.join(ROOT, r"Library\ScriptAssemblies\Facepunch Transport for Netcode for GameObjects.dll")
if os.path.exists(transport) and "Facepunch Transport" not in s:
    extra.append(f'    <Reference Include="FacepunchTransport"><HintPath>{transport}</HintPath></Reference>')
s = s.replace("</Project>", "  <ItemGroup>\n" + "\n".join(extra) + "\n  </ItemGroup>\n</Project>")

os.makedirs(args.out, exist_ok=True)
dst = os.path.join(args.out, args.assembly + ".Check.csproj")
open(dst, "w", encoding="utf-8").write(s)
print(dst)
