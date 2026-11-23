# 언리얼 → 유니티 이식 규칙

원본: `C:\Unreal Projects\Terminus` (UE 5.8, C++ 약 3만 줄). 기준 소스는 **KSH + origin/JSY 를 합친 트리**
(`git merge-tree --write-tree KSH origin/JSY` → `ecf9ea53`, 충돌 없음). 작업 사본은 KSH 브랜치라 JSY 의 9커밋이 빠져 있으니
원본을 읽을 땐 합친 트리를 꺼낸 곳(또는 `git show ecf9ea53:<경로>`)을 본다.

결정 사항 (2026-11-23, 팀 결정으로 유니티 이식)
- 엔진: Unity 6000.6.10f1, URP 2D
- 네트워크: Netcode for GameObjects 2.14 + Steam (Facepunch 트랜스포트, 나중에 추가). 언리얼처럼 리슨 서버 / 서버 권한
- UI: UI Toolkit (UXML/USS)
- 저장소: `kshpt9043/Terminus` 의 `unity` 브랜치 (이 폴더)

## 구조

```
Assets/Terminus/
  Runtime/Core/      Terminus.Core   엔진 없는 순수 C# (noEngineReferences). 데이터 / 규칙 / 계산 전부 여기
    Data/            표 행 타입, CSV 로더, GameData (언리얼 Data/*)
    Combat/          전투 수치 / 스킬 실행 (언리얼 Combat/*)
    Map/             지도 생성 / 진행 (언리얼 Map/MapManager 의 로직)
    Run/             런 / 퀘스트 / 프로필 / 저장 규칙 (언리얼 Game/* 의 로직)
    Dungeon/         방 진행 / 보상 / 상점 / 이벤트 / 던전 전투 흐름 (언리얼 Dungeon/*)
  Runtime/Game/      Terminus.Game   MonoBehaviour, NGO 네트워크, 씬, 표현
  Resources/Data/    DT_*.csv (언리얼 Data/*.csv 그대로. 기획 엑셀 → xlsx_to_csv.py → 여기)
  Tests/EditMode/    NUnit 테스트. 엔진 API 안 씀 → dotnet test 로도 돔
Tools/CoreBuild/     유니티 밖 빌드 (Core/Terminus.Core.csproj, Tests/Terminus.Core.Tests.csproj)
```

빠른 확인: `cd Tools/CoreBuild && dotnet test Tests/Terminus.Core.Tests.csproj`
(테스트 프로젝트는 net48 + NUnit 3.5 = 유니티 Mono / com.unity.ext.nunit 과 같은 조합. `Core/Terminus.Core.csproj` 는 netstandard2.1 로 API 확인)
유니티에서: `unity command test run --mode EditMode` 또는 Test Runner 창

## 코드 규칙

- **C# 9** 까지만 (유니티 컴파일러). 파일 범위 namespace, record struct, global using, 구조체 필드 초기화, `required` 금지.
- Core 는 `UnityEngine` 을 절대 참조하지 않음. 랜덤은 `System.Random` 을 주입받음 (서버가 시드 고정 가능하게).
- 이름: 언리얼 접두사(F/E/U/A) 뺌. `FSkillRow` → `SkillRow`, `EStatusEffect` → `StatusEffect`.
  필드 이름은 언리얼 그대로 (CSV 열 이름과 같아야 해서). `bOneTimePerRun` 같은 b 접두사도 유지.
- `FName` / 행 이름 참조 → `string`, 없음 = `""` (`Ids.IsNone()` 로 검사). CSV 의 "None" 은 `[NameField]` 붙은 필드만 "" 로 바뀜.
- `FText` / `FString` → `string`. `TArray` → `List<T>`. `TMap` → `Dictionary`.
- 언리얼 `USTRUCT` 는 값 복사였음 → C# 에선 `sealed class` + `Clone()`. 복사가 필요한 곳은 반드시 Clone.
- 언리얼 `UActorComponent` / 서브시스템의 **규칙과 계산**은 Core 의 일반 클래스로, **복제 / RPC / 액터 수명**은 Game 쪽 NetworkBehaviour 로 나눈다.
  Core 클래스는 상태를 들고 이벤트(C# event / 콜백)로 바뀜을 알린다. Game 쪽이 그걸 NetworkVariable / RPC 로 보낸다.
- 언리얼 주석(한국어, 이유 설명)은 같이 옮긴다. 기획 결정 날짜 / 이유가 들어 있음.
- 언리얼 자동 테스트(`Private/Tests/*`)는 NUnit 으로 옮긴다.
- 언리얼 설정값(`UDeveloperSettings`, DefaultGame.ini)은 Core 의 설정 클래스 기본값으로.

## 데이터 메모

- `DT_CharacterClass.csv` 는 언리얼 에디터 DataTable 을 MCP 로 읽어 옮긴 값 (2026-11-23). 언리얼에서 Export as CSV 한 파일도 그대로 읽힌다 (BaseStats 구조체 리터럴 지원).
- 저장 파일: 암호 봉투(AES-256 ECB + HMAC-SHA1, "TMS1" 헤더)는 언리얼과 같지만 안의 내용이 JSON 이라 언리얼 세이브와 호환 안 됨.

## 아트

- 몬스터: `Art/Monsters/<PixelLab 이름>/<동작>/frame_N.png` → 메뉴 `Terminus/Art/Build Monster Visuals` → 플립북 + `Resources/Visuals/Monsters/<이름>.asset`.
  DT_MonsterVisual 의 Source 열로 찾음 (`MonsterVisualSet.LoadBySource`). 그림은 왼쪽을 봄, 10fps, PPU 64 (보스 128px = 2유닛).
- 플레이어 직업: `Art/Characters/<직업>/idle` — 언리얼 TempSprites 임시 그림(대기 4프레임)을 커맨드렛으로 내보낸 것. 오른쪽을 봄.
- 아이콘: `Resources/Icons/Relics/T_Relic_*.png`, `Resources/Icons/Skills/T_Icon_*.png` → `GameIcons`. 종류별 기본 스킬 아이콘(Default_Attack 등)은 아직 없음.
- 폰트: Noto Sans KR (`UI/Fonts`). UI 테마 `UI/Theme/Terminus.uss` (언리얼 TerminusUIColors / UIBuilder 팔레트, 글자 크기).
- 확인용 씬: `Scenes/Dev/Dev_MonsterGallery.unity` (메뉴 `Terminus/Art/Open Monster Gallery`).

## 진행 상황

| 모듈 | 언리얼 원본 | 상태 |
|---|---|---|
| Data | Data/* | 완료 (CSV 9개 경고 없이 로드) |
| Combat | Combat/CombatStatsComponent, SkillExecutor, Tests | 완료 (언리얼 테스트 6개 이식) |
| Map | Map/MapManager | 완료 (생성 / 방 선택 / 층 투표 / 배신 결과) |
| Run / Save | Game/TerminusQuests, ProfileSubsystem, SaveSubsystem, SaveCrypto, RunSubsystem, PlayerState 규칙 | 완료 |
| Dungeon 전투 | Dungeon/DungeonCombatComponent, Character/* 규칙 | 완료 (`DungeonCombat`, `IAreaCombat` 구현) |
| Dungeon 구역 / 방 | Dungeon/DungeonArea, AreaSet, AreaSubsystem, ThemeData, GameMode 이공간 | 완료 (휴식 / 상점 / 이벤트 / 퀘스트 방 / 구출 / 이공간) |
| Game (씬 / 서버 / NGO 래퍼) | Game/*GameMode, Player/*, Map/Dungeon 액터 | 1차 완료 (아래 "게임 쪽 구조") |
| 네트워크 | Online/SessionSubsystem (Steam), Player/*(RPC), Comms, Chat | 완료: IP(Unity Transport) + Steam(Facepunch 트랜스포트, 로비 방 목록 / 비밀번호 / 진행 중 재합류), 핑 / 퀵챗 / 전투 계획 공유. Steam 실제 2인은 미확인 (PC 두 대 필요) |
| UI | Widgets/* (약 40화면) | 완료: 언리얼 WBP 그대로 (메뉴 / 주점 / 지도 / 전투 HUD / 보상 / 휴식 / 상점 / 이벤트 / 구출 / 정산 / 채팅 / 핑). 기능 화면은 지움 |
| 아트 | PixelLab 몬스터, 아이콘, 폰트, 직업 임시 그림 | 완료 |

테스트: 유니티 Test Runner 320개 전부 통과 (Core + Comms + 에셋 연결, 2026-11-23)

## 게임 쪽 구조 (2026-11-23)

- 씬: `Scenes/MainMenu` → (방장이 방을 열면) `Tavern` → (전원 준비 후 출발) `Dungeon`. 서버가 NetworkSceneManager 로 전환.
  던전 / 주점 씬을 에디터에서 바로 Play 하면 혼자 방장으로 그 자리에서 시작 (언리얼 PIE 던전 직행)
- 네트워크: `Game/Net` — NetworkManager 를 코드로 생성 (`NetworkRoot`), 플레이어 프리팹 하나(`TerminusPlayer`)에
  범용 통로 3개 (요청 / 개인 알림 / 전체 알림). 메시지는 `NetMessages.cs` 의 [Serializable] 클래스를 JSON 으로 (`NetBus`)
  언리얼 Server_* = `XxxReq`, Client_* = `XxxMsg`. 서버는 요청 보낸 연결로 플레이어를 정함
- 서버: `ServerSession`(판 전체, 씬 넘어 유지: 사람마다 RunPlayer, 주점 요청, 파티 스냅샷) + `DungeonServer`(지도 / 구역 / 전투 Core 를 돌리고 스냅샷 0.1초마다 바뀌면 전송)
- 클라: `ClientState`(받은 스냅샷) → `UI/Screens`(UI Toolkit, 코드로 그림) + `AreaView`(무대 스프라이트)
- 저장: `FileSaveStorage`(persistentDataPath/Saves) + `JsonSaveSerializer`, 프로필은 각자 컴퓨터 (`LocalProfile`)
- 에셋 만들기: 메뉴 `Terminus/Setup/Build Project Assets` (플레이어 프리팹, UI 패널 설정, 씬 3개, 빌드 씬 목록)

### 테스트하는 법
- 혼자: `MainMenu` 씬 Play → 혼자 시작. 또는 `Dungeon` 씬을 바로 Play
- 멀티 (에디터 하나로): Multiplayer Play Mode 시나리오 `Scenes/Dev/TwoPlayers.asset` (메인 + 복제 1) 를 활성으로 두고 Play.
  메인 에디터에서 방 만들기. 복제 플레이어는 `DevAutoGuest`(에디터 전용)가 자동 손님으로 참가 / 준비 / 방장 방 따라가기 / 턴 종료
- 빌드 실행 (언리얼 Run-Standalone.bat 자리): 프로젝트 루트 `Run-Standalone.bat` — 그냥 = Steam, `nosteam` = IP, `2` = 같은 PC 에 IP 모드 2개(프로필 p1/p2),
  `build` = 에디터가 꺼져 있으면 배치 빌드 후 실행 (에디터가 열려 있으면 메뉴 `Terminus/Build/Windows`). 결과물 `Builds/Windows/Terminus.exe`, 로그 `Logs/standalone*.log`
- Steam 2인은 PC 두 대 / 계정 두 개 (Steam 클라이언트 하나 = 계정 하나). 개발용 AppID 480
- 2026-11-23 확인: 혼자 한 바퀴 (주점 → 퀘스트 방 → 전투 → 보상 → 지도), 2인 (참가 → 준비 → 출발 → 파티 분리 동시 전투 → 전멸 → 구출 투표 시간 초과 → 지도)
- 2026-11-23 정식 UI 확인: 혼자 (퀘스트 / 전투 / 보상 / 휴식 / 상점 / 이벤트 / 가디언 / 보스 / 전멸 → 정산 → 메인), MPPM 2인 (주점 → 퀘스트 → 전투 → 보상 → 지도),
  빌드 `Run-Standalone.bat 2` 로 창 두 개 (방 열기 → 127.0.0.1 참가 → 준비 → 출발 → 지도)
- 2026-11-23 이어하기 확인: 혼자 (성기사 / 방 3개 / 체력 39 / 스킬 / 유물 / 퀘스트 / 지도 그대로 → 다음 전투도 체력 39), MPPM 2인 (방장이 세이브를 열면 손님이 이공간으로 재합류 → 두 사람 상태 그대로 → 같이 다음 방, 같은 슬롯에 자동 저장)
- 에디터는 도메인 리로드를 꺼 둠 → static 은 Play 마다 `RuntimeInitializeOnLoadMethod(SubsystemRegistration)` 에서 되돌릴 것 (NetBus 처리기는 NetBus 가 비움)
- 에디터 Game 뷰가 16:9 가 아니면 (예 932x712) 기준 1920x1080 고정 좌표 화면이 오른쪽으로 넘쳐 보임. 빌드 1280x720 에서는 정상

## 다음 단계 (게임 쪽)

Core 는 서버에서 도는 순수 로직이고, 각 모듈 보고의 "언리얼 글루" 목록이 게임 쪽이 할 일이다.
- 이벤트 / 콜백: `FloorProgression`(지도), `DungeonAreaManager.PlayerAreaChanged / AreaChanged / RoomsFinished / PartyWiped / BetrayalFinished / SpecialRoomInjectionRequested`,
  `IAreaNotifier`(휴식 / 상점 / 이벤트 / 구출 / 이공간 화면), `DungeonCombat.StateChanged / MonsterSpawned / MonsterActing / RoomRewardOffered`, `RunPlayer.RunStateChanged / Notice`
- 매 프레임: `DungeonTimers.Tick(serverTime)`, 구역마다 `DungeonCombat.Tick(serverTime)`, `FloorProgression.Tick(serverTime)`
- 저장: `ISaveStorage` (파일) + `ISaveSerializer` (JsonUtility)
- 남은 것: Steam 실제 2인 확인 (PC 두 대), 플레이어 직업 정식 그림 (지금은 언리얼과 같은 임시 그림: 머리 위 흰 네모 1~4개는 그림 안의 프레임 번호 표시)
- 몬스터 그림: 슬라임 왕국 13마리만 있음. 나머지 54마리는 픽셀랩 세션이 작업 중 (일반 64 / 가디언 96 / 보스 128px)

### 각 모듈 보고에서 나온 "언리얼 그대로 옮긴 의심스러운 동작" (기획 확인용)
- 전투: 죽는 한 대를 맞은 대상도 반격함 / 반동·중독 피해에도 자기 급소(+25%)·보호막이 적용됨 / 불굴+철벽이면 피해 0 가능
- 지도: 싱글도 층 투표 15초 타이머가 돌아 안 고르면 무작위로 탈출될 수 있음 / 퀘스트 특수 방 주입이 방 배치 규칙을 다시 안 봄
- 런: 강화 칸 기본값 폴백이 5 (주석은 3)
- 던전 전투: 전투 끝 부활이 피해 경로를 타서 체술 암살자는 1 이 아니라 1+회피 로 일어남 / 몬스터의 OnSkillUsed 유물(RLC_Monster_LIZ03, SIL03)은 안 터짐 / 보상 선택이 거절돼도 다시 고를 기회 없음 / 사원소의 성배는 회복마다 +16 까지 연쇄
- 구역: 이벤트 대가를 툴팁 대괄호 글자에서 읽음 (CH_CMN_011_B, 012_B 는 대가 열과 다름) / 상점 강화는 지금 계층 스킬만 / 구출·난입하면 원래 방이 지나온 길에 안 남음
- 몬스터 기획 메모만 있고 코드 없음: 에이션트 슬라임 부활 때 공격·방어 누적 변동, 홉고블린 대주술사·알파 다이어울프 동반 스폰

## 정식 UI 이식 규칙 (언리얼 위젯 그대로)

언리얼 위젯은 두 겹이다: C++ 클래스(`Docs/UnrealRef/Widgets/`)가 로직 + 기본 레이아웃(`BuildDefaultLayout`), 그 위에 WBP(UMG 디자이너)가
`BindWidgetOptional` 로 같은 이름의 위젯을 갖고 있으면 WBP 디자인이 쓰인다. WBP 디자인은 MCP 로 꺼낸 트리 덤프 `Docs/UnrealRef/WidgetDumps/WBP_*.txt`
(위젯 종류 / 이름 / 앵커 / 오프셋 / 크기 / 색 / 글꼴 / 텍스처 경로가 다 있음). 어느 WBP 를 쓰는지는 언리얼 `Config/DefaultGame.ini` 의 TerminusUISettings
(예: Shop = WBP_ShopWidget, CombatHUD = WBP_CombatUI, ItemSlot = WBP_SkillSlot).

- **레이아웃 = WBP 덤프 우선**, 덤프에 없는 부분(코드가 채우는 목록 / 카드 / 툴팁)은 C++ `BuildDefaultLayout` / `Refresh` 를 따른다.
- **UI Toolkit, 코드로 VisualElement 를 만든다** (UXML 안 씀). 화면마다 USS 하나 (`Assets/Terminus/UI/Screens/<화면>.uss`), 공통 변수 / 클래스는 `UI/Theme/Terminus.uss`.
  언리얼 CanvasPanel 앵커(0~1) / 오프셋은 position:absolute + left/top/right/bottom 퍼센트 / px 로 그대로 옮긴다 (기준 해상도 1920x1080, PanelSettings 가 맞춰 줌).
- **색**: 덤프의 SpecifiedColor 는 선형(linear) 값이다 → sRGB 로 바꿔서 쓴다 (c^(1/2.2) 근사 말고 정확히: c<=0.0031308 ? 12.92c : 1.055c^(1/2.4)-0.055). 팔레트에 같은 색이 있으면 USS 변수를 쓴다.
- **글꼴**: Noto Sans KR (Regular / Bold). TypefaceFontName=Bold 면 `.t-bold`.
- **텍스처**: 언리얼 경로 `/Game/X/Y/T_Z` → `Resources/UI/Textures/X/Y/T_Z.png` (스프라이트, 부드러운 필터). `UiTextures.Load("UI/Textures/Map/T_Map_Paper")` 처럼 Resources 로.
  Brush 의 DrawAs=Box / Margin 이 있으면 USS `-unity-slice-left/right/top/bottom` (9-slice).
- **데이터 / 행동**: 화면은 `ClientState`(스냅샷) 를 읽고 `NetBus.Request(new XxxReq{...})` 로 서버에 요청, `NetBus.On<XxxMsg>` 로 알림을 받는다.
  언리얼 PC 의 Client_* / Server_* 이름은 `Game/Net/NetMessages.cs` 에 대응. 없는 메시지가 필요하면 **자기 partial 파일**에 추가 (아래 소유권).
- **레이어**: `UIRoot.Instance.Show(UILayer.X, element)` (언리얼 AddToViewport ZOrder = `UILayer`).
- 화면은 지금의 기능 화면(`UI/Screens/*Screen.cs`, `DungeonPanels.cs`)을 대체한다. 대체가 끝나면 기능 화면은 지운다.
