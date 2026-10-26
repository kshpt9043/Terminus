# PixelLab → 언리얼(PaperZD) 몬스터 스프라이트 파이프라인

파란 슬라임(2026-10-20)을 만들면서 겪은 시행착오를 줄인 순서다. 다른 몬스터도 이 순서대로 하면 된다.

## 한눈에 보기

| 단계 | 누가 / 무엇으로 | 결과 |
|---|---|---|
| 0. 기획 확인 | Notion 커넥터 `던전 테마` | 외형 + 6개 동작 설명 |
| 1. 기본 외형 | PixelLab `create_1_direction_object` | 64px 후보 16개 중 1개 |
| 2. 동작 생성 | PixelLab `animate_object` (v3) + 일부 직접 제작 | 6개 동작 프레임 |
| 3. 내보내기 | 파이썬(PIL) | `PixelLab/<이름>/` 프레임·시트·GIF |
| 4. 언리얼 에셋 | 언리얼 MCP | 텍스처·스프라이트·플립북 |
| 5. PaperZD | **사람이 에디터에서** + MCP | AS, 시퀀스, ABP |
| 6. 연결 / 확인 | MCP + PIE | BP_Monster, 로그 확인 |

---

## 0. 기획 먼저 다시 읽기

- 생성하기 **직전에** Notion `기획안 데모 > 던전 테마`를 다시 읽는다. 작업 도중에도 바뀐다(슬라임은 생성 중에 "X자 눈"이 빠지고 공격에 "[몸통박치기]"가 붙었다).
- 동작마다 기획 문장을 표로 옮겨 두고 결과와 하나씩 대조한다.
- 동작 6개 기준: **Idle / 공격 / 피격 / 방어 / 스페셜 / 사망**
  - 게임 쪽 연결: 공격 = 공격 의도, 방어 = 방어 의도, 스페셜 = 버프·디버프 의도, 피격 = 체력이 깎일 때, 사망 = 죽을 때

## 1. 기본 외형

```
create_1_direction_object(
  description="cute round <색> slime monster, ... side view, dark outline, cartoon fantasy RPG enemy",
  size=64, view="sidescroller")
```

- 64px이면 후보 16개, 10 generations. `get_object`로 보고 → 확대 비교 시트를 만들어 사람이 고르게 한다 → `select_object_frames`.
- **방향은 프롬프트로 정해지지 않는다.** "facing left"라고 써도 후보가 전부 **오른쪽**을 봤다. 그대로 둔다(아래 "방향 규칙").
- 같은 종류의 변종(힐/레드/포이즌…)은 이 기본형을 기준으로 만든다. 무표정·장식 없는 후보가 색 바꾸기 쉽다.

## 2. 동작 생성

### 방향 규칙 (가장 많이 헷갈린 것)

- PixelLab 생성본 = **오른쪽을 봄**. 언리얼 `TerminusMonster`도 **오른쪽 그림 기준**으로 받아서 `Visual` 스케일로 뒤집는다.
- 그러니 **언리얼에 넣는 그림은 뒤집지 않는다.** 기획서처럼 왼쪽을 보는 건 미리보기 GIF에서만 뒤집어 보여준다.
- 프롬프트의 좌우도 **생성본 기준**으로 쓴다. 기획서의 "왼쪽(앞)"은 프롬프트에서 "right side(front)", "오른쪽으로 밀림"은 "pushed to the left".

### v3 애니메이션이 잘하는 것 / 못하는 것

`animate_object(object_id, animation_description, frame_count, mode="v3")` — 6개를 **한 번에 병렬로** 걸고 `wait_for_jobs`로 기다린다(각 1~3분). 결과 프레임 수는 `frame_count + 1`(0번이 기본 이미지).

| 동작 | 결과 | 최적 방법 |
|---|---|---|
| Idle | 잘 됨 | v3 그대로 |
| 피격 | 잘 됨 | v3. 프롬프트에 "eyes stay normal, no X eyes, body does not burst" 처럼 **원치 않는 것을 명시** |
| 공격(몸통박치기) | 텍스트만으론 몸이 안 늘어나거나 뿔처럼 솟음 | **끝 프레임 지정**: 기본 이미지를 PIL로 "윗부분이 앞으로 기울고 늘어난 자세"로 변형 → `end_frame_base64`로 넘겨 보간 → 내보낼 때 프레임을 **거꾸로 이어 붙여**(`f + f[-2::-1]`) 원위치로 복귀 |
| 방어(작아지며 진해짐) | 순서가 뒤섞이고 원래대로 안 돌아옴 | **직접 제작**(PIL): 바닥 중앙 기준 축소 + 진한 파랑 섞기 → 유지 → 복귀. 생성 비용 0 |
| 스페셜(점프) | v3는 몸이 바닥에서 안 떨어짐 | **직접 제작**: 기본 이미지를 위로 0→15px 이동 + 찌그러짐/늘어남 + 최고점 발광 |
| 사망 | 무너지긴 하지만 완전히 안 사라짐 | v3 + 마지막에 **알파 페이드 + 아래로 2~3px** 프레임 4장 덧붙임 |

정리: **"원래대로 돌아온다" / "사라진다" / "땅에서 뜬다"는 v3 혼자 못 한다.** 거꾸로 붙이기, 페이드 덧붙이기, 직접 제작으로 처리한다.

팀 피드백으로 고친 것 (2026-10-26):
- **공격은 한 번만 박는다.** 생성본을 거꾸로 붙이면 이펙트가 두 번 나오고, 생성본 끝 프레임이 살짝 물러나 있으면 두 번 박는 것처럼 보인다. 앞으로 → 박은 자세 유지 → 이펙트 없는 복귀 프레임(직접 제작)으로.
- **피격은 과장한다.** `tools/make_slime_hit.py`: 하얗게 번쩍 + 뒤로 젖혀짐 + 젤리 가시가 뒤·위로 삐죽 + 반동으로 앞으로 튕김. 생성본 원본 프레임은 `slime_blue/_pixellab_src/hit/` 에 보관.

## 3. 내보내기 (`PixelLab/<이름>/`)

- 다운로드: PixelLab 파일 URL(backblaze)은 파이썬 기본 `urllib`이면 **403**. `User-Agent: curl/8.0` 헤더를 붙인다.
- 폴더 구조(기존 `slime_brown`과 같게):
  - `<이름>_64.png`, `<이름>_preview_512.png`
  - `<이름>_<동작>/frame_N.png`
  - `<이름>_<동작>_sheet.png` (가로 한 줄, 64×N)
  - `<이름>_<동작>_preview.gif` (4배, 어두운 배경, 0.1초/프레임) — **사람 검토용, 왼쪽을 보게 뒤집음**
- 사람에게 보여주기 전에 **동작별 한 줄 접촉 시트**(3배 확대)를 만들어 직접 보고 기획 표와 대조한다.
- 언리얼용 시트는 **오른쪽을 보는 원본**으로 따로 만든다(`T_<이름>_<동작>_Sheet.png`).

## 4. 언리얼 에셋 (언리얼 MCP)

폴더: `/Game/KSH/Characters/Monster/<이름>/`

1. **텍스처**: `TextureTools.import_file` → 설정 `Filter=TF_Nearest, CompressionSettings=TC_EditorIcon, LODGroup=TEXTUREGROUP_Pixels2D, MipGenSettings=TMGS_NoMipmaps`
2. **스프라이트**: 스프라이트를 새로 만드는 도구가 없다. 기존 스프라이트(`S_Slime_Idle_0`)를 `duplicate` → `SourceTexture`, `SourceUV(64*i,0)`, `SourceDimension(64,64)` 설정.
   - ⚠️ 한 번에 설정하면 **렌더 데이터가 시트 전체 크기로 구워져 공격 때 시트가 통째로 보인다.** 설정 뒤에 `SourceDimension`을 `63,64`로 바꿨다가 `64,64`로 되돌려서 **다시 굽게** 한다.
3. **플립북**: 기존 플립북을 `duplicate` → `KeyFrames`, `FramesPerSecond=10`.
   - ⚠️ 배열 길이가 바뀌면서 내용도 바뀌면 `ArrayAdd ... ambiguous` 오류. **기존 항목은 그대로 두고 뒤에만 붙여** 길이를 먼저 맞춘 다음, 같은 길이로 전체를 덮어쓴다.
4. 저장: `AssetTools.save_assets`. 에셋 수 확인(텍스처 6 + 스프라이트 Σ프레임 + 플립북 6).

## 5. PaperZD (여기는 사람이 해야 함)

MCP로 안 되는 것: 시퀀스의 `animSource` 변경(읽기 전용), PaperZD 그래프 노드 생성, ABP의 애니메이션 소스 지정. 복제로 우회해도 참조가 원본(Monk)을 가리킨 채로 남는다 — **시도하지 말 것.**

사람이 할 일 (5분):
1. `AS_<이름>` 열기 (MCP로 `AS_Monk` 복제해 만들어 둬도 됨)
2. 시퀀스 6개 추가: `Idle, Attack, Hit, Defend, Special, Death` (플립북은 비워 둬도 됨)
3. `AS_<이름>` 우클릭 → **Create AnimBP** → `ABP_<이름>`
4. AnimGraph: `Locomotion(상태머신, Idle 상태가 Idle 재생) → Override Slot('DefaultSlot', 'DefaultGroup') → Output Animation`
5. 컴파일, 저장

그다음 MCP: 각 시퀀스의 `animData[0].animation`에 `FB_<이름>_<동작>` 지정 → 저장.

## 6. 연결 / 확인

- `BP_Monster` (또는 몬스터별 BP) CDO:
  - `:Animation` 컴포넌트 `AnimInstanceClass = ABP_<이름>_C`
  - `AttackSequences[]`, `DefendSequence`, `SpecialSequence`, `HitSequence`, `DeathSequence`
- ⚠️ **`Sprite` 컴포넌트의 `SourceFlipbook`은 비워 둔다.** 들어 있으면 ABP 연출이 화면에 안 나왔다.
- ⚠️ BP 이벤트 그래프에 죽을 때 숨기거나 지우는 노드가 있으면 사망 연출이 안 보인다. 숨김은 코드(`HideAfterDeath`)가 사망 시퀀스 길이만큼 기다렸다가 한다.
- 확인은 PIE 후 `Saved/Logs/Terminus.log`에서:
  ```
  grep -E "ActionAnim|\[Died\]|몬스터 [0-9] ->" Saved/Logs/Terminus.log
  ```
  `[ActionAnim] BP_Monster_C_0 -> Attack/Defend/Hit/Death` 가 찍히면 코드 쪽은 정상. 안 찍히면 시퀀스 연결, 찍히는데 안 보이면 Sprite/BP 노드 쪽.

## 재활용 먼저 (새로 뽑기 전에)

기획서의 외형·동작을 기존 몬스터와 비교해 먼저 나눈다.

| 분류 | 방법 | 예 |
|---|---|---|
| 색만 다름 | `tools/recolor_monster.py` 로 프레임·시트·GIF 전부 색 바꾸기. 생성 비용 0 | 레드(0도), 힐(120도) |
| 색 + 이펙트 | 색 바꾸기 + 연기/냉기 같은 것만 덧그리기 | 포이즌, 아이스 |
| 질감만 다름 | **PixelLab 으로 "같은 모양 + 그 질감" 후보를 뽑고** `tools/skin_from_candidate.py <_skin.json>` 으로 후보의 몸 질감을 모든 동작 프레임에 입힘(몸 밖 돌출물·불티 포함, 방어 때 다른 질감으로 바꾸기 가능). 코드로 만든 질감(`skin_slime.py lava\|mud`)은 밋밋해서 탈락. 단순 금속은 `skin_slime.py ... metal` | 라바(방어 때 바위 껍질), 머드(나뭇가지), 메탈 |
| 장식만 다름 | PixelLab 으로 "장식 붙은 같은 몸" 후보를 뽑고 `tools/decor_overlay.py <후보> <원본> <새이름>` 으로 장식만 오려 모든 프레임에 붙임. **코드로 직접 그린 장식(돌/이끼)은 스티커처럼 보여 탈락** | 에이션트 |
| 새로 생성 | 1~3단계 그대로 | 크기가 다르거나(가디언/보스), 동작이 다를 때 |

가디언(96px)은 `tools/build_slime.py <설정.json>` 으로 큰 기본 슬라임 하나를 조립하고(공격 단타·피격 가시·방어·점프·사망 페이드 포함), 위 방법들로 변종을 만든다. 메탈은 팀 결정으로 구르는 공 대신 "같은 모양 + 강철 질감"(`skin_slime.py ... metal`).

```
python -I PixelLab/tools/recolor_monster.py slime_blue slime_red 0
python -I PixelLab/tools/recolor_monster.py slime_blue slime_heal 120
```

- 인자: 원본 이름, 새 이름, 목표 색상(도, 0=빨강 120=초록 240=파랑 280=보라), [채도 배수], [밝기 배수]
- 외곽선·눈동자(아주 어두움)와 흰 하이라이트(무채색)는 안 바뀐다.
- `HUE_SPREAD`(0.3): 원본 색끼리의 색상 차이를 줄여 남긴다. 1이면 파랑→빨강에서 진한 색(방어)이 갈색으로 밀렸다.
- 언리얼에 넣을 때도 텍스처만 새로 넣고, 플립북·시퀀스·ABP 는 기존 몬스터 것을 복제해 쓴다.

질감·장식 입힐 때 걸렸던 것 (도구에 이미 반영됨):

- **감은 눈**: 원본 공격 프레임의 감은 눈 선은 검정이 아니라 진한 파랑이라 몸으로 잡힌다. 몸 안의 어두운 선은 질감의 어두운 색으로 그린다. 외곽선에 갇힌 파란 눈동자도 질감 대신 어두운 눈 색으로.
- **후보의 눈**: 질감 후보에 그려진 눈(라바는 빛 테두리까지)은 질감에서 지우고 주변으로 메운다. 안 그러면 눈 감은 프레임에 유령 눈이 남는다. 돌출물 후보의 눈·반사광도 돌출물로 딸려오지 않게 눈 자리는 뺀다.
- **사망 끝 반투명 프레임**: 몸 판정에서 알파 기준을 빼야 마지막 프레임이 원본 파랑으로 돌아가지 않는다.
- **유리 같은 질감**: 후보의 하얀 반사광 + 원본의 광택 띠 때문. `_skin.json` 에 `"matte": 0.7` (반사광 죽이고 명암 누르고 흙 알갱이).
- **몸 색도 바꾸기**: `decor_overlay.py <후보> <원본> <새이름> body=95,0.5,0.75` (에이션트: 이끼·돌과 어울리는 올리브). 후보 png 는 오른쪽을 보는 원본(생성본)을 줄 것.
- **방어 동작이 칼 팔까지 같이 움직일 때** (장군): 프롬프트로는 안 고쳐진다. 기본 이미지에서 방패 든 자세 한 장을 inpaint 로 만들고, `custom_start_frame` + `end_frame` 으로 사이만 생성한 뒤 `_build.json` 에 `"defend_mode": "raise"` (올리기 → 유지 → 거꾸로 내리기). 사라지는 장식(갑옷띠)은 `"defend_keep": [x0,y0,x1,y1]` 로 기본 이미지에서 다시 그림.

색 + 이펙트는 색을 바꾼 **다음에** `tools/add_slime_fx.py` (결과 폴더를 그 자리에서 고침 → 다시 돌릴 땐 recolor 부터):

```
python -I PixelLab/tools/recolor_monster.py slime_blue slime_poison 285 0.85 0.72
python -I PixelLab/tools/add_slime_fx.py slime_poison poison    # 바닥 양 끝 연기
python -I PixelLab/tools/recolor_monster.py slime_blue slime_ice 192 0.5 1.15
python -I PixelLab/tools/add_slime_fx.py slime_ice ice          # 반투명 + 냉기 + 공격 때 얼음 조각
```

- 이펙트는 6개 동작 전부에 들어가고, 사망에서는 몸이 흐려지는 만큼 같이 흐려진다.
- 공격 때 몸이 캔버스 왼쪽 끝까지 늘어나서, 앞에 튀는 이펙트는 몸 바깥이 아니라 **앞쪽 가장자리 위에 겹쳐** 그려야 잘리지 않는다.
- 연기/냉기 같은 작은 이펙트는 64px에서 쉽게 묻힌다. 반드시 4배 확대 + 회색 배경으로 확인할 것.

## 기타 요령

- **헤더(UPROPERTY) 고치면** 에디터를 끄고 빌드해야 한다(Live Coding이 막음). `Build-Editor.bat`은 `pause`가 있어서 자동화엔 엔진 `Build.bat`을 직접 부른다:
  `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" TerminusEditor Win64 Development "-Project=...\Terminus.uproject" -WaitMutex -FromMsBuild`
- **에디터 화면을 눈으로 확인**하려면: `CaptureViewport` 결과의 base64를 같은 스크립트 안에서 `AssetTools.write_file`로 `Saved/xxx.txt`에 쓰고(확장자는 .txt만 됨), 밖에서 디코딩해 PNG로 본다. 임시 액터는 하늘(z=50000)에 세우고 끝나면 지운다. **레벨은 저장하지 않는다.**
- 끄기 전에 전체 저장(`save_assets([])`)하면 손대지 않은 `Lv_Lobby`, `ABP_Monk` 같은 것도 다시 저장돼 git에 뜬다. 커밋할 때 빼거나, 필요한 에셋만 저장한다.
- 크레딧: `get_balance`로 확인. 기본 외형은 10 generations, v3 애니메이션은 그보다 훨씬 싸다. 직접 제작한 동작(방어/점프)은 0.
