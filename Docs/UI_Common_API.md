# 공통 UI 부품 API (`Terminus.UI.Common`)

코드: `Assets/Terminus/Runtime/UI/Common/`, USS: `Assets/Terminus/UI/Common/Resources/UI/Common/{Common,Skill,Relic}.uss`
(각 부품이 `UiStyle.Use(this, "Common", ...)` 로 자기 시트를 붙임. 클래스 접두사 `tc-`, 색은 테마 `--t-*` 변수).
언리얼 원본: `Widgets/Common`, `Widgets/Skill`, `Widgets/Relic` + WBP_SkillSlot / ItemSlot / ItemCard / ItemPickPanel / ChoiceCard / ConfirmPopup / Loading / SkillCard / SkillTooltip / StartSkillPick / RelicBar / RelicTooltip / StartRelicPick.

텍스처 경로 주의: 언리얼 `/Game/UI/Textures/...` 는 `Resources/UI/Textures/UI/Textures/...` 에 있음 → `UiTextures.FromUnrealPath("/Game/...")` 를 쓰면 맞춰 줌.

## 도구

- **UiColor**: `FromLinear(r,g,b,a=1)` (언리얼 덤프의 선형 색 → sRGB), `LinearToSrgb`, `Hex("C9A45C", alpha)`, `WithAlpha`, `Clear`. 팔레트 정적 색 Inset, Board, Section, Card, Disabled, Line, TextBody/Sub/Aux/Faint, GoldHover, Gold, GoldPressed, Ready, Alert, Primary/Secondary/CurrencyFill, DefenseTeal, Dim, DimStrong, Window.
- **UiTextures**: `Load(resPath)` (캐시된 Texture2D), `LoadSprite`, `FromUnrealPath("/Game/A/T_X.T_X")` → `"UI/Textures/A/T_X"`, `LoadUnreal(uePath)`, `SetBackground(el, resPath)`. 상수 `Hex(kind,state)`, `GoldDivider`, `HexDisabled`, `Coin`, `SelectGlow`, `GoldLine`.
- **UiStyle** (UiKit 짝): `Text(text, size=16, color?, align, bold)`, `WrapText`, `Title(text, size=40)`, `SetBold`, `AddShadow`, `SetTextIfChanged`, `SetLine`(빈 글이면 숨김),
  `HexButton(label, HexKind.Primary|Secondary|Currency, onClick?, fontSize=17)` (진짜 T_UI_Hex_* 텍스처, 양 끝 9-slice), `StyleHex`, `FlatButton(label, onClick?, size, FlatKind.Board|Card, fill?)`, `StyleFlat`,
  `DimRoot(color?)`, `Panel(padH, padV, fill?)`, `GoldDivider(title)`, `Row()`, `Column()`, `AddRow`, `FlexSpacer()`, `SetShown`, `IsShown`, `IgnorePicking`, `Padding`, `Border`, `Radius`, `Use(el, sheets)`. 글자 크기 상수 ScreenTitle / Section / Subtitle / Body / Aux.
- **UiText** (언리얼 TerminusUIText): `SkillTypeLabel`, `TargetTypeLabel`, `SkillOwnerLabel`, `RelicTierLabel`, `RoomTypeLabel`, `GoldText(n)` ("1,234 G"), `SkillTypeLine` ("공격 · 적 1명"), `SkillCostText`, `SkillCardCostText`, `FixNewlines`, `RelicTierColor`, `SkillTypeColor`.
- **UiTooltip**: 마우스를 따라다니는 공용 툴팁. `Attach(el, Func<VisualElement> build)` (null 이면 안 띄움), `SetText(el, "제목\n본문")`, `Detach`, `Hide`, `MakeTextTooltip`. 화면 끝에서 뒤집힘, 클릭 / 나감 / 떼어짐에 숨김.
- **Throbber(radius=28, pieces=6, color?)**: 도는 표시. `Period` 로 속도.

## 부품 (전부 VisualElement)

- **ItemSlotWidget** `()` 84x104 (WBP_SkillSlot) / `(Vector2 size)`. `SetEmpty`, `SetSkill(row, SkillRow upgraded=null)`, `SetRelic(row)`, `SetItem(bool relic, row, upgraded)`, `SetSelected`, `SetUsable`, `SetShowName`, `SetSlotSize`, `SetIconOnly`.
  속성 Kind(Empty/Skill/Relic), ItemRow, SlotIndex, ShownSkill, IsSelected, IsUsable, IsPressed, TooltipEnabled. 이벤트 `Clicked(slot)`, `HoverChanged(slot,bool)`. 툴팁은 스스로 붙임.
- **ItemCardWidget**: `SetItem(relic, row, slotSize, showName=true, upgradedSkill)`, `SetCaption`, `SetAction(label, enabled, CardActionStyle.Currency|Primary|Secondary|Flat)`, `SetActionLabelColor`, `SetCaptionStyle`. 속성 Slot, IsRelic, ItemRow. 이벤트 `ActionClicked`, `Updated`.
- **ItemPickPanelWidget**: `CreateFloating(out host)` (가운데 뜨는 판), `ShowEnhanceSkills(guide, run)`, `ShowRelics(guide, filter, run)`, `ShowItems(guide, relic, rows)`, `Begin(guide)` + `AddSlot()`, `Hide()`, `SetCancelVisible`. 이벤트 `Picked(slot)`, `Cancelled` (스스로 안 닫음).
- **ChoiceCardWidget** `(ChoiceCardStyle style=null)`: `SetTexts(title, desc, footer)`, `SetItem(relic, row)`, `SetChoiceId`, `SetState(Normal|Selected|Highlight|Danger)`, `SetChoiceEnabled`, `ApplyStyle`. 이벤트 `Chosen(card)`, `Updated`.
- **ConfirmPopupWidget**: `Show(title, msg, onConfirm, confirmLabel="확인", cancelLabel="취소", onCancel)` (UILayer.Popup), `Alert(title, msg, onOk)`. Enter/Space 확인, Esc 취소.
- **LoadingScreenWidget**: `Show(message, LoadingStyle.Full|Light)` (다시 부르면 글만 바꿈), `Hide()`, `Current`, `IsShowing`, `SetMessage`, `SetTitle`. 모든 레이어 위.
- **SkillTooltipWidget**: `SetSkill(shown, original)`, `SetSkill(row, upgraded)`, `For(row, upgraded)`, `BuildDescription`, `BuildStatusLines`. 강화 반영("(5+" → 수치), 상태이상 설명 옆 패널.
- **SkillCardWidget** `()` 300x400 / `(size)`: `SetSkill(row)`, `SetIconSize`, `SetCardEnabled`, `SetSelected`. 이벤트 `Clicked(row)`, `HoverChanged`.
- **StartSkillPickWidget**: `OpenForLocalPlayer()` (프로필 보유 스킬 중 3개, UILayer.Pick, 이미 골랐으면 null), `SetChoices`, `Finish(row)` → `ChooseStartSkillReq`. 이벤트 `Finished`.
- **RelicTooltipWidget**: `SetRelic(row)`, `For(row)`.
- **RelicBarWidget** `(bool autoBind=true)`: 내 유물 자동 표시 (ClientState). `SetRelics`, `IconSize`, `Spacing`, `Slots`. 이벤트 `SlotClicked`.
- **StartRelicPickWidget**: `OpenForLocalPlayer()` (창고 유물. 없으면 빈 요청을 바로 보내고 null), `SetCandidates`, `GetChosen`, `CanEquip`. 이벤트 `Finished`, `SelectionChanged`.

## 언리얼과 다른 점
- 블러 없음 (UI Toolkit 미지원) → 어둡게만.
- EscapeStack 미이식 → 팝업이 Esc 를 직접 처리.
- 강화된 스킬 사본은 화면이 넘겨야 함 (`upgraded` 인자). 클라엔 강화 계산 도우미가 없음 → `RunPlayer.ApplyBasicSkillUpgrade` 규칙을 쓰려면 `RunState` 로 계산.
