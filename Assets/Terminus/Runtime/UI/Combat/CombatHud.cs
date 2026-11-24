// 전투 HUD (언리얼 UCombatHUDWidget + WBP_CombatUI). 내가 보고 있는 구역(AreaView.ShownArea)에서 전투가 진행 중일 때만 보임
// DungeonView 가 UILayer.CombatHUD 에 하나 만들어 둠. 보일지 말지는 스스로 정함 (매 프레임 구역 상태를 봄)
//
// 기획 전투 UI 시안 배치 (WBP_CombatUI 덤프 그대로):
//  - 왼쪽 위: "n층 n번째 방 · 사이클 n" 판 (RoomPlate)
//  - 위 가운데: 턴 띠 760x64 (양끝이 흐려지는 띠 + 위아래 금선). 내 턴 / 기다림 / 적의 턴 / 배신자의 턴 / 승리 / 패배마다 색이 다름
//  - 왼쪽 아래 판: 에너지 / 스킬 에너지 구슬 + 칸 92x84 (오른쪽 위 비용 원, 왼쪽 위 층 마름모, 강화 +N)
//  - 아래 가운데: 안내 판 (대상 고르기 / 드래그 / 예약)
//  - 오른쪽 아래: 유물 줄, 턴 종료 판 (봉랍 + 글자, 내 턴 아니면 "대기 중…"), 스탯 줄
//  - 캐릭터 / 몬스터를 따라다니는 표시 (TagLayer): 머리 위 행동 예고 / "대기", 발밑 보호막 4px + 체력 7px + 숫자 + 상태이상 칸,
//    대상 선택 칸, 몬스터 정보 카드(툴팁), 동료의 행동 계획 / 예상 피해 (언리얼은 UPartyCommsWidget 이 그렸음)
//
// 스킬 사용: 적 1명 / 아군 1명 스킬은 누르면 대상 고르기 모드 -> 대상(또는 그 스프라이트) 클릭. 우클릭이나 같은 스킬을 다시 누르면 취소
//  - 드래그: 스킬 칸을 누른 채 끌어서 대상 위에서 놓으면 바로 사용 (대상 밖에서 놓으면 취소, Shift 를 누른 채 놓으면 예약만)
//  - Shift + 대상 클릭 (또는 자신 / 전체 스킬을 Shift + 클릭) = 쓰지 않고 '예약' 만 파티에 공유. 이번 사이클 동안 유지
// 관전 중(내가 없는 구역)이면 보기만 함
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Combat
{
    public sealed partial class CombatHud : VisualElement
    {
        // 칸 크기 / 간격 (언리얼 SetSlotSize(92, 84), SkillSlotSpacing 8)
        static readonly Vector2 SkillSlotSize = new Vector2(92f, 84f);
        const float SkillSlotSpacing = 8f;

        // ---- 위젯 (WBP_CombatUI 이름 그대로)
        readonly VisualElement _content;      // 보일 때만 켜는 전체 (루트는 늘 붙어 있어 매 프레임 갱신이 돎)
        readonly VisualElement _catcher;      // 대상 고르는 중에만 클릭을 받는 바탕 (우클릭 취소 / 스프라이트 클릭)
        readonly VisualElement _tagLayer;     // TagLayer: 따라다니는 표시
        readonly Label _roomText;
        readonly VisualElement _phaseBandFill;
        readonly VisualElement _phaseLineTop;
        readonly VisualElement _phaseLineBottom;
        readonly Label _phaseText;
        readonly VisualElement _energyOrbs;
        readonly VisualElement _skillEnergyOrbs;
        readonly Label _energyText;
        readonly Label _skillEnergyText;
        readonly VisualElement _skillBox;
        readonly VisualElement _enhanceSkillBox;
        readonly VisualElement _hintPlate;
        readonly Label _hintText;
        readonly Button _endTurnButton;
        readonly VisualElement _endTurnSeal;
        readonly Label _endTurnLabel;
        readonly VisualElement _statusRow;
        readonly RelicBarWidget _relicBar;
        readonly VisualElement _debugRow;

        // ---- 그림
        readonly Texture2D _orbEnergy, _orbEnergyEmpty, _orbSkill, _orbSkillEmpty, _seal, _sealOff;
        readonly Texture2D[] _intentTextures = new Texture2D[4];

        // ---- 상태
        bool _visible;
        int _areaIndex = -1;
        Vector2Int _shownEnergy = new Vector2Int(-1, -1);
        Vector2Int _shownSkillEnergy = new Vector2Int(-1, -1);
        string _shownStatusLine;
        CombatLooks.PhaseStyle? _shownPhaseStyle;
        Texture2D _shownSeal;
        bool _endTurnHovered;

        // 스킬 칸: 목록 순서 = 칸 번호 (0~2 기본, 3~ 강화)
        sealed class SkillDeco
        {
            public VisualElement Holder;
            public ItemSlotWidget Slot;
            public VisualElement CostBadge;
            public Label CostText;
            public VisualElement UpgradeBadge;
            public Label UpgradeText;
            public VisualElement TierPips;
            public string ShownSkill = "\u0001";
            public int ShownLevel = -1;
        }
        readonly List<SkillDeco> _skillSlots = new List<SkillDeco>();
        int _numEnhanceSlots = -1;
        int _hoveredSlot = -1;

        // 내 런 규칙 (클라에 RunPlayer 를 하나 두고 받은 RunState 를 넣어 Core 규칙을 그대로: 칸 행 이름 / 강화 단계 / 칸 수)
        readonly RunPlayer _myRun = new RunPlayer(new System.Random(0));
        RunState _myRunSource;

        // 이번 프레임 계산 결과
        sealed class Frame
        {
            public AreaInfo Area;
            public PlayerInfo Me;
            public bool Spectating;
            public bool MeAlive;
            public bool MyTurn;
            public CombatStats MyStats;
        }
        readonly Frame _frame = new Frame();

        public CombatHud()
        {
            CombatLooks.Use(this);
            UiStyle.Use(this, "Common");
            AddToClassList("ch-root");
            pickingMode = PickingMode.Ignore;
            CombatPlanBoard.Hook();

            _orbEnergy = CombatLooks.Tex("T_Orb_Energy");
            _orbEnergyEmpty = CombatLooks.Tex("T_Orb_EnergyEmpty");
            _orbSkill = CombatLooks.Tex("T_Orb_Skill");
            _orbSkillEmpty = CombatLooks.Tex("T_Orb_SkillEmpty");
            _seal = CombatLooks.Tex("T_EndTurn_Seal");
            _sealOff = CombatLooks.Tex("T_EndTurn_SealOff");
            for (int i = 0; i < _intentTextures.Length; ++i) _intentTextures[i] = CombatLooks.Tex(CombatLooks.IntentTextureNames[i]);

            _content = new VisualElement { name = "CombatContent", pickingMode = PickingMode.Ignore };
            _content.AddToClassList("ch-fill");
            Add(_content);

            _catcher = new VisualElement { name = "TargetCatcher", pickingMode = PickingMode.Ignore };
            _catcher.AddToClassList("ch-fill");
            _catcher.RegisterCallback<PointerDownEvent>(OnCatcherPointerDown);
            _content.Add(_catcher);

            // [0] CanvasPanel TagLayer (전체 화면). 따라다니는 표시는 맨 뒤
            _tagLayer = new VisualElement { name = "TagLayer", pickingMode = PickingMode.Ignore };
            _tagLayer.AddToClassList("ch-fill");
            _content.Add(_tagLayer);

            // [1] RoomPlate (왼쪽 위 28, 24)
            var roomPlate = new VisualElement { name = "RoomPlate", pickingMode = PickingMode.Ignore };
            roomPlate.AddToClassList("ch-roomplate");
            _roomText = UiStyle.Text("", 16, CombatLooks.MapText, TextAnchor.MiddleLeft);
            _roomText.name = "RoomText";
            roomPlate.Add(_roomText);
            _content.Add(roomPlate);

            // [3] PhaseBand (위 가운데 760x64, 위 20)
            var phaseBand = new VisualElement { name = "PhaseBand", pickingMode = PickingMode.Ignore };
            phaseBand.AddToClassList("ch-phaseband");
            _phaseBandFill = new VisualElement { name = "PhaseBandFill", pickingMode = PickingMode.Ignore };
            _phaseBandFill.AddToClassList("ch-phaseband__fill");
            _phaseLineTop = new VisualElement { name = "PhaseLineTop", pickingMode = PickingMode.Ignore };
            _phaseLineTop.AddToClassList("ch-phaseband__line");
            _phaseLineTop.AddToClassList("ch-phaseband__line--top");
            _phaseLineBottom = new VisualElement { name = "PhaseLineBottom", pickingMode = PickingMode.Ignore };
            _phaseLineBottom.AddToClassList("ch-phaseband__line");
            _phaseLineBottom.AddToClassList("ch-phaseband__line--bottom");
            _phaseText = UiStyle.Text("", 28, UiColor.Hex("FFEDA0"), TextAnchor.MiddleCenter, true);
            _phaseText.name = "PhaseText";
            _phaseText.pickingMode = PickingMode.Ignore;
            _phaseText.AddToClassList("ch-phasetext");
            _phaseText.style.textShadow = new TextShadow { offset = new Vector2(0f, 2f), blurRadius = 0f, color = Color.black };
            phaseBand.Add(_phaseBandFill);
            phaseBand.Add(_phaseLineTop);
            phaseBand.Add(_phaseLineBottom);
            phaseBand.Add(_phaseText);
            _content.Add(phaseBand);

            // [8] SkillPanel (왼쪽 아래 28, -28)
            var skillPanel = new VisualElement { name = "SkillPanel" };
            skillPanel.AddToClassList("ch-skillpanel");
            var skillRows = new VisualElement { name = "SkillRows", pickingMode = PickingMode.Ignore };
            skillPanel.Add(skillRows);
            skillRows.Add(MakeSkillRow("SkillRow0", "에너지", CombatLooks.EnergyTextColor, out _energyOrbs, out _energyText, out _skillBox));
            _skillBox.name = "SkillBox";
            var row1 = MakeSkillRow("SkillRow1", "스킬 에너지", CombatLooks.SkillEnergyTextColor, out _skillEnergyOrbs, out _skillEnergyText, out _enhanceSkillBox);
            _enhanceSkillBox.name = "EnhanceSkillBox";
            row1.AddToClassList("ch-skillrow--second");
            skillRows.Add(row1);
            _content.Add(skillPanel);

            // [24] HintPlate (아래 가운데, -40)
            _hintPlate = new VisualElement { name = "HintPlate", pickingMode = PickingMode.Ignore };
            _hintPlate.AddToClassList("ch-hintplate");
            _hintText = UiStyle.Text("", 17, CombatLooks.MapText, TextAnchor.MiddleCenter);
            _hintText.name = "HintText";
            _hintText.pickingMode = PickingMode.Ignore;
            _hintPlate.Add(_hintText);
            UiStyle.SetShown(_hintPlate, false);
            _content.Add(_hintPlate);

            // 보유 유물 줄 (WBP 에 없어서 언리얼 C++ 이 오른쪽 아래 -28, -176 에 붙임)
            _relicBar = new RelicBarWidget();
            _relicBar.AddToClassList("ch-relicbar");
            _content.Add(_relicBar);

            // [26] RightColumn (오른쪽 아래 -28, -28): 턴 종료 260x76 + 스탯 판
            var rightColumn = new VisualElement { name = "RightColumn", pickingMode = PickingMode.Ignore };
            rightColumn.AddToClassList("ch-rightcolumn");
            _endTurnButton = new Button(HandleEndTurnClicked) { name = "EndTurnButton" };
            _endTurnButton.AddToClassList("ch-endturn");
            _endTurnButton.RegisterCallback<PointerEnterEvent>(_ => _endTurnHovered = true);
            _endTurnButton.RegisterCallback<PointerLeaveEvent>(_ => _endTurnHovered = false);
            _endTurnSeal = new VisualElement { name = "EndTurnSeal", pickingMode = PickingMode.Ignore };
            _endTurnSeal.AddToClassList("ch-endturn__seal");
            _endTurnLabel = UiStyle.Text("턴 종료", 26, UiColor.Hex("F0E8D6"), TextAnchor.MiddleCenter, true);
            _endTurnLabel.name = "EndTurnLabel";
            _endTurnLabel.pickingMode = PickingMode.Ignore;
            _endTurnLabel.AddToClassList("ch-endturn__label");
            _endTurnLabel.style.textShadow = new TextShadow { offset = new Vector2(0f, 2f), blurRadius = 0f, color = Color.black };
            _endTurnButton.Add(_endTurnSeal);
            _endTurnButton.Add(_endTurnLabel);
            rightColumn.Add(_endTurnButton);

            var statusPlate = new VisualElement { name = "StatusPlate", pickingMode = PickingMode.Ignore };
            statusPlate.AddToClassList("ch-statusplate");
            _statusRow = new VisualElement { name = "StatusRow", pickingMode = PickingMode.Ignore };
            _statusRow.AddToClassList("ch-statusrow");
            statusPlate.Add(_statusRow);
            rightColumn.Add(statusPlate);
            _content.Add(rightColumn);

            // 테스트 버튼 (언리얼 Server_DebugWinCombat / LoseCombat). 개발 빌드에서만
            _debugRow = UiStyle.Row();
            _debugRow.name = "DebugRow";
            _debugRow.AddToClassList("ch-debugrow");
            _debugRow.Add(UiStyle.FlatButton("승리(테스트)", () => NetBus.Request(new DebugWinCombatReq()), UiStyle.Aux));
            _debugRow.Add(UiStyle.FlatButton("패배(테스트)", () => NetBus.Request(new DebugLoseCombatReq()), UiStyle.Aux));
            // 4번 칸 강화 스킬을 직업 스킬 전체에서 돌리고 에너지를 채움 (스킬 이펙트 시험)
            _debugRow.Add(UiStyle.FlatButton("◀ 스킬(테스트)", () => NetBus.Request(new DebugCycleSkillReq { Step = -1 }), UiStyle.Aux));
            _debugRow.Add(UiStyle.FlatButton("스킬 ▶(테스트)", () => NetBus.Request(new DebugCycleSkillReq { Step = 1 }), UiStyle.Aux));
            UiStyle.SetShown(_debugRow, false);
            _content.Add(_debugRow);

            BuildSkillSlots(RunPlayer.NumEnhanceSkills);
            RegisterInput();

            UiStyle.SetShown(_content, false);
            schedule.Execute(Tick).Every(0);
            RegisterCallback<DetachFromPanelEvent>(_ => HideHud());
        }

        VisualElement MakeSkillRow(string name, string label, Color textColor, out VisualElement orbs, out Label text, out VisualElement box)
        {
            var row = new VisualElement { name = name, pickingMode = PickingMode.Ignore };
            row.AddToClassList("ch-skillrow");

            // EnergyCol: 84 폭, 오른쪽 12
            var col = new VisualElement { pickingMode = PickingMode.Ignore };
            col.AddToClassList("ch-energycol");
            var title = UiStyle.Text(label, 12, CombatLooks.MapTextSub, TextAnchor.MiddleCenter);
            title.pickingMode = PickingMode.Ignore;
            col.Add(title);
            orbs = new VisualElement { pickingMode = PickingMode.Ignore };
            orbs.AddToClassList("ch-orbs");
            col.Add(orbs);
            text = UiStyle.Text("0/0", 13, textColor, TextAnchor.MiddleCenter, true);
            text.pickingMode = PickingMode.Ignore;
            col.Add(text);
            row.Add(col);

            box = new VisualElement { pickingMode = PickingMode.Ignore };
            box.AddToClassList("ch-skillbox");
            row.Add(box);
            return row;
        }

        // =====================================================================
        // 스킬 칸
        // =====================================================================

        // 기본 칸 3개 + 강화 칸 enhanceCount 개를 다시 만듦 (강화 칸 수가 캐릭터 EnhanceSlots 와 다를 때만)
        void BuildSkillSlots(int enhanceCount)
        {
            enhanceCount = Mathf.Clamp(enhanceCount, 0, RunPlayer.NumEnhanceSkills);
            if (enhanceCount == _numEnhanceSlots) return;
            _numEnhanceSlots = enhanceCount;

            _skillSlots.Clear();
            _skillBox.Clear();
            _enhanceSkillBox.Clear();
            _hoveredSlot = -1;

            for (int i = 0; i < RunPlayer.NumBasicSkills; ++i) _skillSlots.Add(AddSkillSlot(_skillBox, i, i == 0));
            for (int i = 0; i < enhanceCount; ++i) _skillSlots.Add(AddSkillSlot(_enhanceSkillBox, RunPlayer.NumBasicSkills + i, i == 0));
        }

        SkillDeco AddSkillSlot(VisualElement box, int slotIndex, bool first)
        {
            var deco = new SkillDeco();

            // 칸을 겹침 판으로 감싸고 그 위에 시안 장식: 왼쪽 위 층 마름모 / 오른쪽 위 비용 원 / 오른쪽 강화 +N
            var holder = new VisualElement { pickingMode = PickingMode.Ignore };
            holder.AddToClassList("ch-slotholder");
            holder.style.marginLeft = first ? 0f : SkillSlotSpacing;
            deco.Holder = holder;

            var slot = new ItemSlotWidget(SkillSlotSize) { SlotIndex = slotIndex };
            slot.Clicked += s => OnSkillClicked(s.SlotIndex);
            slot.HoverChanged += (s, hovered) =>
            {
                if (hovered) _hoveredSlot = s.SlotIndex;
                else if (_hoveredSlot == s.SlotIndex) _hoveredSlot = -1;
            };
            // 드래그용 누름 기록 (Clickable 이 전파를 막아도 받게 TrickleDown)
            slot.RegisterCallback<PointerDownEvent>(e => OnSlotPointerDown(slot, e), TrickleDown.TrickleDown);
            holder.Add(slot);
            deco.Slot = slot;

            var pips = new VisualElement { pickingMode = PickingMode.Ignore };
            pips.AddToClassList("ch-slot__pips");
            holder.Add(pips);
            deco.TierPips = pips;

            var cost = new VisualElement { pickingMode = PickingMode.Ignore };
            cost.AddToClassList("ch-slot__cost");
            var costText = UiStyle.Text("", 13, Color.white, TextAnchor.MiddleCenter, true);
            costText.pickingMode = PickingMode.Ignore;
            costText.AddToClassList("ch-slot__costtext");
            cost.Add(costText);
            UiStyle.SetShown(cost, false);
            holder.Add(cost);
            deco.CostBadge = cost;
            deco.CostText = costText;

            var upgrade = new VisualElement { pickingMode = PickingMode.Ignore };
            upgrade.AddToClassList("ch-slot__upgrade");
            var upgradeText = UiStyle.Text("", 11, UiColor.Hex("1A1410"), TextAnchor.MiddleCenter, true);
            upgradeText.pickingMode = PickingMode.Ignore;
            upgrade.Add(upgradeText);
            UiStyle.SetShown(upgrade, false);
            holder.Add(upgrade);
            deco.UpgradeBadge = upgrade;
            deco.UpgradeText = upgradeText;

            box.Add(holder);
            return deco;
        }

        // =====================================================================
        // 매 프레임
        // =====================================================================

        void Tick()
        {
            var area = AreaView.ShownArea;
            bool show = area != null && area.bInUse && area.Phase != "None";

            // 구역이 바뀜 (새 방 / 관전 대상 바뀜): 대상 고르기 / 예약 / 공유 계획 지움 (언리얼 SetArea)
            int index = area != null ? area.AreaIndex : -1;
            if (index != _areaIndex)
            {
                _areaIndex = index;
                CancelTargeting();
                ClearDeclaredPlan();
                ClearCombatPlan();
                CombatPlanBoard.Clear();
                ClearTags();
            }

            if (!show)
            {
                if (_visible) HideHud();
                return;
            }
            if (!_visible)
            {
                _visible = true;
                UiStyle.SetShown(_content, true);
            }

            BuildFrame(area);

            // 내 턴이 아니면 대상 고르기 취소
            if (_pendingSkillIndex >= 0 && !_frame.MyTurn) CancelTargeting();

            RebuildTagsIfNeeded(area);
            UpdateSkillDrag();
            UpdateTags(Time.unscaledDeltaTime);
            UpdatePanels();
            UpdateSharedPlan();
            UpdateCatcher();
        }

        void HideHud()
        {
            _visible = false;
            CancelTargeting();
            ClearDeclaredPlan();
            ClearCombatPlan();
            UiStyle.SetShown(_content, false);
            UpdateCatcher();
        }

        void BuildFrame(AreaInfo area)
        {
            var f = _frame;
            f.Area = area;
            f.Me = ClientState.Me;
            var me = f.Me;

            if (me != null && me.Run != null && !ReferenceEquals(me.Run, _myRunSource))
            {
                _myRunSource = me.Run;
                _myRun.SetRunState(me.Run);
            }

            // 관전 중 (내가 들어 있지 않은 구역을 보는 중): 내 턴 아님, 스킬 못 씀
            f.Spectating = me == null || !area.Occupants.Contains(me.Id);
            f.MyStats = me != null && me.bHasBattler ? ClientCombat.MakeStats(me.Stats, me.Combat) : null;
            f.MeAlive = f.MyStats != null && !f.MyStats.IsDead();
            f.MyTurn = !f.Spectating && f.MeAlive && ClientCombat.IsTurnOf(area, me.Id) && !ClientCombat.HasEndedTurn(area, me.Id);
        }

        // =====================================================================
        // 판 (위 / 왼쪽 아래 / 오른쪽 아래)
        // =====================================================================

        void UpdatePanels()
        {
            var f = _frame;
            var area = f.Area;
            var me = f.Me;
            bool betrayal = area.bBetrayal;
            string phase = area.Phase;

            // ---- 위: "n층 n번째 방  ·  사이클 n" (기획 시안)
            int floor = ClientState.Map != null ? ClientState.Map.Floor : 1;
            UiStyle.SetTextIfChanged(_roomText, betrayal
                ? $"{floor}층 배신 전투  ·  사이클 {area.Cycle}"
                : $"{floor}층 {area.Room.Row + 1}번째 방  ·  사이클 {area.Cycle}");

            // ---- 턴 띠
            string label = "";
            bool isTurnOfMe = me != null && ClientCombat.IsTurnOf(area, me.Id);
            switch (phase)
            {
                case "PlayerTurn":
                    if (betrayal && !f.MyTurn && f.MeAlive && !f.Spectating && !isTurnOfMe)
                    {
                        label = area.bBetrayerTurn ? "배신자의 턴" : "상대 편의 턴";
                        break;
                    }
                    label = f.Spectating ? "플레이어 턴" : (!f.MeAlive ? "쓰러짐" : (f.MyTurn ? "내 턴" : "다른 플레이어를 기다리는 중"));
                    break;
                case "MonsterTurn":
                    label = "적의 턴";
                    break;
                case "Victory":
                    // 배신 전투: 내 편이 이겼는지
                    label = !betrayal || f.Spectating || ((me != null && me.Id == area.BetrayerId) == ClientCombat.GuessBetrayerWon(area)) ? "승리!" : "패배";
                    break;
                case "Defeat":
                    label = "패배";
                    break;
            }
            if (f.Spectating) label = "관전 중 · " + label;
            UiStyle.SetTextIfChanged(_phaseText, label);

            // 띠 색: 승리 / 내 턴 금, 기다림 회색, 적의 턴 붉음, 배신자 진홍, 패배 어두움
            var style = CombatLooks.PhaseStyle.Mine;
            switch (phase)
            {
                case "PlayerTurn":
                    style = label.Contains("배신자") || label.Contains("상대 편") ? CombatLooks.PhaseStyle.Betrayer
                        : (f.MyTurn || f.Spectating) ? CombatLooks.PhaseStyle.Mine : CombatLooks.PhaseStyle.Waiting;
                    break;
                case "MonsterTurn": style = CombatLooks.PhaseStyle.Enemy; break;
                case "Victory": style = label.Contains("패배") ? CombatLooks.PhaseStyle.Defeat : CombatLooks.PhaseStyle.Victory; break;
                case "Defeat": style = CombatLooks.PhaseStyle.Defeat; break;
            }
            ApplyPhaseStyle(style);

            // ---- 안내 (없으면 판째 숨김)
            UiStyle.SetTextIfChanged(_hintText, _draggingSkill ? "대상 위에서 놓으면 사용  (대상 밖에서 놓으면 취소 · Shift: 쓰지 않고 예약만 공유)"
                : _pendingSkillIndex >= 0 ? "대상을 선택하세요  (우클릭: 취소 · Shift+클릭: 쓰지 않고 예약만 공유)"
                : _declaredSkillIndex >= 0 ? "예약을 파티에 공유 중  (같은 스킬 Shift+클릭: 예약 취소)"
                : "");
            UiStyle.SetShown(_hintPlate, !string.IsNullOrEmpty(_hintText.text));

            UiStyle.SetShown(_debugRow, Debug.isDebugBuild && !f.Spectating);

            // ---- 왼쪽 아래: 에너지 + 스킬
            var myStats = f.MyStats;
            if (myStats != null)
            {
                var state = myStats.State;
                var stats = myStats.Stats;

                // 구슬이 있으니 숫자만 ("2/3")
                UiStyle.SetTextIfChanged(_energyText, state.Energy + "/" + stats.MaxEnergy);
                UiStyle.SetTextIfChanged(_skillEnergyText, state.SkillEnergy + "/" + stats.MaxSkillEnergy);
                RefreshOrbs(_energyOrbs, ref _shownEnergy, state.Energy, stats.MaxEnergy, _orbEnergy, _orbEnergyEmpty);
                RefreshOrbs(_skillEnergyOrbs, ref _shownSkillEnergy, state.SkillEnergy, stats.MaxSkillEnergy, _orbSkill, _orbSkillEmpty);

                // 강화 칸 수가 캐릭터 스텟과 다르면 다시 만듦
                BuildSkillSlots(stats.EnhanceSlots);

                foreach (var deco in _skillSlots)
                {
                    int slotIndex = deco.Slot.SlotIndex;
                    var skill = _myRun.GetCombatSkill(slotIndex);
                    // 과욕이면 에너지 +1
                    bool affordable = skill != null && state.Energy >= myStats.GetEffectiveEnergyCost(skill.EnergyCost) && state.SkillEnergy >= skill.SkillEnergyCost;

                    // 강화 단계(훈련소 / 상점)를 더한 수치로 툴팁
                    var row = _myRun.GetCombatSkillRow(slotIndex);
                    if (skill != null)
                    {
                        var upgraded = skill.Clone();
                        _myRun.ApplyBasicSkillUpgrade(slotIndex, upgraded);
                        deco.Slot.SetSkill(row, upgraded);
                    }
                    else
                    {
                        deco.Slot.SetSkill(row);
                    }
                    deco.Slot.SetUsable(skill != null && f.MyTurn && affordable);
                    bool targeting = _pendingSkillIndex == slotIndex;
                    deco.Slot.SetSelected(targeting);   // 대상 고르는 중인 칸 강조

                    // 시안 장식: 비용 원 / 강화 / 층 마름모, 칸 띄우기 (호버 4px / 대상 고르는 중 10px), 내 턴 아니면 흐림
                    RefreshSkillDeco(deco, skill, row, slotIndex, affordable);
                    float lift = targeting ? -10f : (skill != null && f.MyTurn && _hoveredSlot == slotIndex) ? -4f : 0f;
                    deco.Holder.style.translate = new Translate(0f, lift);
                    deco.Holder.style.opacity = skill != null && !f.MyTurn ? 0.42f : 1f;
                }

                // ---- 오른쪽 아래: 상태. 기획 시안 "체력 [최대/현재] 공격 방어 회피"
                // 공격 / 방어는 상태(용기 / 허약 / 가호 / 용암, 유물이 거는 것 포함)까지 더한 실제 값, 회피도 실제 값
                int shownAttack = SkillExecutor.GetEffectiveStat(myStats, ScalingStat.ATK);
                int shownDefense = SkillExecutor.GetEffectiveStat(myStats, ScalingStat.DEF);
                int shownEvasion = myStats.GetEvasion();
                RefreshStatusRow(state.Health, stats.MaxHealth, state.Shield, shownAttack, shownDefense, shownEvasion);
            }

            // ---- 턴 종료 판: 내 턴이면 붉은 봉랍 + "턴 종료", 아니면 꺼진 봉랍 + "대기 중…"
            _endTurnButton.SetEnabled(f.MyTurn);
            UiStyle.SetTextIfChanged(_endTurnLabel, f.MyTurn ? "턴 종료" : "대기 중…");
            bool hover = f.MyTurn && _endTurnHovered;
            _endTurnLabel.style.color = UiColor.Hex(!f.MyTurn ? "6A6470" : hover ? "FFF6D0" : "F0E8D6");
            var sealTex = f.MyTurn ? _seal : _sealOff;
            if (sealTex != null && sealTex != _shownSeal)
            {
                _shownSeal = sealTex;
                _endTurnSeal.style.backgroundImage = new StyleBackground(sealTex);
            }
        }

        void ApplyPhaseStyle(CombatLooks.PhaseStyle style)
        {
            if (_shownPhaseStyle == style) return;
            _shownPhaseStyle = style;
            var look = CombatLooks.GetPhaseLook(style);
            _phaseText.style.color = look.Text;
            _phaseBandFill.style.unityBackgroundImageTintColor = look.Band;
            _phaseLineTop.style.unityBackgroundImageTintColor = look.Line;
            _phaseLineBottom.style.unityBackgroundImageTintColor = look.Line;
        }

        // 구슬 18px (그림은 빛 여백까지 27px). 찬 것 / 빈 것. 바뀌었을 때만 다시 그림
        static void RefreshOrbs(VisualElement box, ref Vector2Int shown, int current, int max, Texture2D full, Texture2D empty)
        {
            var wanted = new Vector2Int(current, max);
            if (wanted == shown) return;
            shown = wanted;

            box.Clear();
            for (int i = 0; i < max; ++i)
            {
                var orb = new VisualElement { pickingMode = PickingMode.Ignore };
                orb.AddToClassList("ch-orb");
                var tex = i < current ? full : empty;
                if (tex != null) orb.style.backgroundImage = new StyleBackground(tex);
                box.Add(orb);
            }
        }

        void RefreshSkillDeco(SkillDeco deco, SkillRow skill, string row, int slotIndex, bool affordable)
        {
            if (skill == null)
            {
                UiStyle.SetShown(deco.CostBadge, false);
                UiStyle.SetShown(deco.UpgradeBadge, false);
                deco.TierPips.Clear();
                deco.ShownSkill = "";
                deco.ShownLevel = -1;
                return;
            }

            // 비용 원: 스킬 에너지가 드는 스킬은 보라, 아니면 에너지 파랑. 모자라면 어두운 원 + 붉은 숫자
            bool skillEnergy = skill.SkillEnergyCost > 0;
            deco.CostBadge.style.backgroundColor = !affordable ? UiColor.Hex("1A1820") : UiColor.Hex(skillEnergy ? "7C45C8" : "2F6BD0");
            UiStyle.SetTextIfChanged(deco.CostText, (skillEnergy ? skill.SkillEnergyCost : skill.EnergyCost).ToString());
            deco.CostText.style.color = affordable ? Color.white : UiColor.Hex("FF4A3A");
            UiStyle.SetShown(deco.CostBadge, true);

            // 강화 +N: 기본 칸은 훈련소 단계, 강화 칸은 픽업 스킬 단계
            int level;
            if (slotIndex < RunPlayer.NumBasicSkills)
            {
                var levels = _myRun.RunState.BasicSkillLevels;
                level = levels != null && slotIndex < levels.Count ? levels[slotIndex] : 0;
            }
            else
            {
                level = _myRun.GetPickupSkillLevel(row);
            }
            if (row == deco.ShownSkill && level == deco.ShownLevel) return;
            deco.ShownSkill = row;
            deco.ShownLevel = level;

            deco.UpgradeText.text = "+" + level;
            UiStyle.SetShown(deco.UpgradeBadge, level > 0);

            // 층 마름모: 표층 1 흐림 / 중층 2 금 / 심층 3 적
            deco.TierPips.Clear();
            int count = (int)skill.SkillTier + 1;
            var pipColor = UiColor.Hex(skill.SkillTier == SkillTier.Deep ? "FF4A3A" : skill.SkillTier == SkillTier.Mid ? "FFD978" : "A89F8C");
            for (int i = 0; i < count; ++i)
            {
                var pip = new VisualElement { pickingMode = PickingMode.Ignore };
                pip.AddToClassList("ch-slot__pip");
                pip.style.backgroundColor = pipColor;
                pip.style.marginLeft = i == 0 ? 0f : 3f;
                deco.TierPips.Add(pip);
            }
        }

        // 시안: "체력 21/30  보호막 6  공격 5  방어 3  회피 1" 이름은 흐리게, 값은 굵게
        void RefreshStatusRow(int health, int maxHealth, int shield, int attack, int defense, int evasion)
        {
            var pairs = new List<KeyValuePair<string, string>> { new KeyValuePair<string, string>("체력", health + "/" + maxHealth) };
            if (shield > 0) pairs.Add(new KeyValuePair<string, string>("보호막", shield.ToString()));
            pairs.Add(new KeyValuePair<string, string>("공격", attack.ToString()));
            pairs.Add(new KeyValuePair<string, string>("방어", defense.ToString()));
            pairs.Add(new KeyValuePair<string, string>("회피", evasion.ToString()));

            var key = "";
            foreach (var p in pairs) key += p.Key + p.Value + "|";
            if (key == _shownStatusLine) return;
            _shownStatusLine = key;

            _statusRow.Clear();
            for (int i = 0; i < pairs.Count; ++i)
            {
                var name = UiStyle.Text(pairs[i].Key, 14, CombatLooks.MapTextSub, TextAnchor.MiddleLeft);
                name.style.marginLeft = i == 0 ? 0f : 14f;
                name.style.marginRight = 4f;
                bool shieldPair = pairs[i].Key == "보호막";
                var value = UiStyle.Text(pairs[i].Value, 14, shieldPair ? CombatLooks.ShieldText : CombatLooks.MapText, TextAnchor.MiddleLeft, true);
                _statusRow.Add(name);
                _statusRow.Add(value);
            }
        }
    }
}
