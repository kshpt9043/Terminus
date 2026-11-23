// 던전 화면 (언리얼 UMapScreenWidget + UCombatHUDWidget 의 최소판)
//  - 지도에 있을 땐: 상단바 + 지도 (방 고르기)
//  - 구역 안 전투 중엔: 전투 HUD (몬스터 / 예고 / 내 스킬 / 턴 종료)
//  - 방 패널(보상 / 휴식 / 상점 / 이벤트 / 구출 / 층 투표 / 정산)은 DungeonPanels
// 언리얼 위젯을 그대로 옮긴 정식 화면은 다음 단계. 여기는 한 바퀴 돌려 보기 위한 기능 화면
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Map;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Screens
{
    public sealed partial class DungeonScreen : VisualElement
    {
        const float RoomSize = 64;
        const float ColSpacing = 150;
        const float RowSpacing = 78;

        readonly Label _topFloor;
        readonly Label _topStats;
        readonly VisualElement _mapLayer;
        readonly VisualElement _mapCanvas;
        readonly Label _mapHint;
        readonly VisualElement _combatLayer;
        readonly Label _phase;
        readonly VisualElement _monsters;
        readonly VisualElement _allies;
        readonly VisualElement _skills;
        readonly Label _energy;
        readonly Button _endTurn;
        readonly VisualElement _modalLayer;
        readonly Label _toast;

        int _selectedSkill = -1;
        IVisualElementScheduledItem _toastHide;

        public DungeonScreen()
        {
            pickingMode = PickingMode.Ignore;

            // ---- 상단바
            var top = UiKit.Row();
            top.style.position = Position.Absolute;
            top.style.left = 0;
            top.style.right = 0;
            top.style.top = 0;
            top.style.height = 56;
            top.style.backgroundColor = UiKit.Hex("313131");
            top.style.paddingLeft = 24;
            top.style.paddingRight = 24;
            top.style.justifyContent = Justify.SpaceBetween;
            _topFloor = UiKit.Text("", "t-section t-gold");
            _topStats = UiKit.Text("", "t-body");
            top.Add(_topFloor);
            top.Add(_topStats);
            Add(top);

            // ---- 지도
            _mapLayer = UiKit.FullScreen();
            _mapLayer.style.top = 56;
            _mapLayer.style.backgroundColor = UiKit.Hex("0E0E10");
            _mapLayer.style.alignItems = Align.Center;
            var scroll = new ScrollView(ScrollViewMode.Vertical);
            scroll.style.flexGrow = 1;
            scroll.style.width = Length.Percent(100);
            _mapCanvas = new VisualElement();
            _mapCanvas.style.alignSelf = Align.Center;
            _mapCanvas.generateVisualContent += DrawEdges;
            scroll.Add(_mapCanvas);
            _mapHint = UiKit.Text("", "t-aux");
            _mapHint.style.position = Position.Absolute;
            _mapHint.style.right = 24;
            _mapHint.style.top = 12;
            _mapLayer.Add(scroll);
            _mapLayer.Add(_mapHint);
            Add(_mapLayer);

            // ---- 전투 HUD (아래쪽. 위쪽 절반은 월드 무대가 보이게 비움)
            _combatLayer = UiKit.FullScreen();
            _combatLayer.pickingMode = PickingMode.Ignore;
            _combatLayer.style.top = 56;
            var hud = UiKit.Column();
            hud.style.position = Position.Absolute;
            hud.style.left = 0;
            hud.style.right = 0;
            hud.style.bottom = 0;
            hud.style.backgroundColor = new Color(0.07f, 0.075f, 0.09f, 0.92f);
            hud.style.paddingLeft = 24;
            hud.style.paddingRight = 24;
            hud.style.paddingTop = 12;
            hud.style.paddingBottom = 16;
            _phase = UiKit.Text("", "t-subtitle");
            hud.Add(_phase);
            var row = UiKit.Row();
            row.style.alignItems = Align.FlexStart;
            _allies = UiKit.Column();
            _allies.style.width = 420;
            _monsters = UiKit.Row();
            _monsters.style.flexGrow = 1;
            _monsters.style.flexWrap = Wrap.Wrap;
            row.Add(_allies);
            row.Add(_monsters);
            hud.Add(row);
            var bottom = UiKit.Row();
            bottom.style.marginTop = 10;
            _energy = UiKit.Text("", "t-section");
            _energy.style.width = 260;
            bottom.Add(_energy);
            _skills = UiKit.Row();
            _skills.style.flexGrow = 1;
            bottom.Add(_skills);
            _endTurn = UiKit.Hex("턴 종료", () => NetBus.Request(new EndTurnReq()), "primary");
            bottom.Add(_endTurn);
            var dbg = UiKit.Column();
            dbg.style.marginLeft = 12;
            dbg.Add(UiKit.Button("승리(테스트)", () => NetBus.Request(new DebugWinCombatReq())));
            dbg.Add(UiKit.Button("패배(테스트)", () => NetBus.Request(new DebugLoseCombatReq())));
            bottom.Add(dbg);
            hud.Add(bottom);
            _combatLayer.Add(hud);
            Add(_combatLayer);

            // ---- 알림 / 패널
            _toast = UiKit.Text("", "t-section t-wrap");
            _toast.style.position = Position.Absolute;
            _toast.style.top = 80;
            _toast.style.alignSelf = Align.Center;
            _toast.style.backgroundColor = new Color(0, 0, 0, 0.75f);
            _toast.style.paddingLeft = 20;
            _toast.style.paddingRight = 20;
            _toast.style.paddingTop = 8;
            _toast.style.paddingBottom = 8;
            _toast.style.display = DisplayStyle.None;
            _toast.pickingMode = PickingMode.Ignore;
            Add(_toast);

            _modalLayer = UiKit.FullScreen();
            _modalLayer.pickingMode = PickingMode.Ignore;
            Add(_modalLayer);

            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.PartyChanged += Refresh;
                ClientState.MapChanged += Refresh;
                ClientState.AreasChanged += Refresh;
                HookPanels();
                Refresh();
            });
            RegisterCallback<DetachFromPanelEvent>(_ =>
            {
                ClientState.PartyChanged -= Refresh;
                ClientState.MapChanged -= Refresh;
                ClientState.AreasChanged -= Refresh;
            });
        }

        public void Toast(string text, float seconds = 2.5f)
        {
            if (string.IsNullOrEmpty(text)) return;
            _toast.text = text;
            _toast.style.display = DisplayStyle.Flex;
            _toastHide?.Pause();
            _toastHide = schedule.Execute(() => _toast.style.display = DisplayStyle.None).StartingIn((long)(seconds * 1000));
        }

        // ------------------------------------------------------------ 갱신

        void Refresh()
        {
            var me = ClientState.Me;
            var map = ClientState.Map;
            _topFloor.text = $"{map.Floor}층  {map.TierName} · {map.ThemeName}";
            if (me != null && me.bHasBattler)
            {
                _topStats.text = $"{me.Name}  체력 {me.Combat.Health}/{me.Stats.MaxHealth}   던전 재화 {me.Run.Currency}   유물 {me.Run.Relics.Count}";
            }

            var area = ClientState.MyArea;
            bool inArea = area != null && area.bInUse;
            bool inCombat = inArea && area.Phase != "None";
            _mapLayer.style.display = inArea ? DisplayStyle.None : DisplayStyle.Flex;
            _combatLayer.style.display = inCombat ? DisplayStyle.Flex : DisplayStyle.None;

            if (!inArea)
            {
                // 방에서 나왔으면 방 패널은 다 닫음 (서버가 닫으라고 안 보낸 경로 대비)
                foreach (var key in new[] { "reward", "rest", "shop", "event", "rescue" }) CloseModal(key);
                RefreshMap();
            }
            if (inCombat) RefreshCombat(area);
            RefreshStartPick();
            RefreshFloorVote();
        }

        // ------------------------------------------------------------ 지도

        static string RoomLabel(RoomType t)
        {
            switch (t)
            {
                case RoomType.MONSTER: return "몬스터";
                case RoomType.BREAK: return "휴식";
                case RoomType.STORE: return "상점";
                case RoomType.GUARDIAN: return "가디언";
                case RoomType.EVENT: return "이벤트";
                case RoomType.BOSS: return "보스";
                case RoomType.QUEST: return "퀘스트";
                default: return t.ToString();
            }
        }

        static Color RoomColor(RoomType t)
        {
            switch (t)
            {
                case RoomType.BREAK: return UiKit.Hex("3F7F86");
                case RoomType.STORE: return UiKit.Hex("A88100");
                case RoomType.GUARDIAN: return UiKit.Hex("7A3F86");
                case RoomType.EVENT: return UiKit.Hex("4A6A3A");
                case RoomType.BOSS: return UiKit.Hex("A8322C");
                case RoomType.QUEST: return UiKit.Hex("5B8BD9");
                default: return UiKit.Hex("262A32");
            }
        }

        Vector2 RoomPos(RoomNode r, int levels)
        {
            float x = 60 + r.Col * ColSpacing;
            float y = 40 + (levels - 1 - r.Row) * RowSpacing;
            return new Vector2(x, y);
        }

        int _lastMapHash;

        void RefreshMap()
        {
            var map = ClientState.Map;
            var me = ClientState.Me;
            int levels = map.Rooms.Count == 0 ? 1 : map.Rooms.Max(r => r.Row) + 1;
            int maxCol = map.Rooms.Count == 0 ? 3 : map.Rooms.Max(r => r.Col);
            _mapCanvas.style.width = 120 + maxCol * ColSpacing + RoomSize;
            _mapCanvas.style.height = 80 + (levels - 1) * RowSpacing + RoomSize;

            // 다른 사람 선택
            var picks = new Dictionary<int, List<string>>();
            foreach (var p in ClientState.Party.Players)
            {
                if (p.Run.SelectedRoomId < 0) continue;
                if (!picks.TryGetValue(p.Run.SelectedRoomId, out var list)) picks[p.Run.SelectedRoomId] = list = new List<string>();
                list.Add(p.Name);
            }

            _mapCanvas.Clear();
            foreach (var r in map.Rooms)
            {
                var room = r;
                var pos = RoomPos(r, levels);
                var b = new Button(() => NetBus.Request(new SelectRoomReq { RoomId = room.RoomId }));
                b.AddToClassList("t-button");
                b.style.position = Position.Absolute;
                b.style.left = pos.x;
                b.style.top = pos.y;
                b.style.width = RoomSize + 40;
                b.style.height = RoomSize;
                b.style.backgroundColor = RoomColor(r.Type);
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.fontSize = 14;

                bool visited = me != null && me.Run.VisitedRoomIds.Contains(r.RoomId);
                bool current = me != null && me.Run.CurrentRoomId == r.RoomId;
                bool reachable = me != null && IsReachable(me.Run, r, map.Rooms);
                bool mine = me != null && me.Run.SelectedRoomId == r.RoomId;

                string label = RoomLabel(r.Type) + (r.MaxPlayers < 4 ? $"\n{r.MaxPlayers}인" : "");
                if (picks.TryGetValue(r.RoomId, out var names)) label += "\n" + string.Join(",", names);
                b.text = label;

                b.style.opacity = visited && !current ? 0.45f : 1f;
                var border = mine ? UiKit.Hex("C8352E") : current ? UiKit.Hex("ECE3CF") : reachable ? UiKit.Hex("FFEDA0") : UiKit.Hex("3A3F4A");
                float bw = mine || reachable || current ? 3 : 1;
                b.style.borderTopColor = b.style.borderBottomColor = b.style.borderLeftColor = b.style.borderRightColor = border;
                b.style.borderTopWidth = b.style.borderBottomWidth = b.style.borderLeftWidth = b.style.borderRightWidth = bw;
                _mapCanvas.Add(b);
            }
            _mapCanvas.MarkDirtyRepaint();

            _mapHint.text = me == null ? "" :
                me.Run.SelectedRoomId >= 0 ? "다른 사람이 고르길 기다리는 중... (같은 방을 다시 누르면 취소)" : "등불이 켜진 방을 고르세요";
        }

        // 언리얼 AMapManager::IsValidNextRoom 과 같은 규칙 (화면 표시용. 판정은 서버)
        static bool IsReachable(RunState run, RoomNode target, List<RoomNode> rooms)
        {
            if (run.CurrentRoomId < 0) return target.Row == rooms.Min(r => r.Row);
            var cur = rooms.Find(r => r.RoomId == run.CurrentRoomId);
            return cur != null && cur.ConnectedRoomIds.Contains(target.RoomId);
        }

        void DrawEdges(MeshGenerationContext ctx)
        {
            var map = ClientState.Map;
            if (map.Rooms.Count == 0) return;
            int levels = map.Rooms.Max(r => r.Row) + 1;
            var me = ClientState.Me;
            var p = ctx.painter2D;
            p.lineWidth = 3;
            foreach (var r in map.Rooms)
            {
                var a = RoomPos(r, levels) + new Vector2((RoomSize + 40) / 2, 0);
                foreach (var id in r.ConnectedRoomIds)
                {
                    var t = map.Rooms.Find(x => x.RoomId == id);
                    if (t == null) continue;
                    var b = RoomPos(t, levels) + new Vector2((RoomSize + 40) / 2, RoomSize);
                    bool taken = me != null && me.Run.VisitedRoomIds.Contains(r.RoomId) && me.Run.VisitedRoomIds.Contains(id);
                    p.strokeColor = taken ? UiKit.Hex("C9A45C") : new Color(0.7f, 0.68f, 0.65f, 0.45f);
                    p.BeginPath();
                    p.MoveTo(a);
                    p.LineTo(b);
                    p.Stroke();
                }
            }
        }

        // ------------------------------------------------------------ 전투 HUD

        SkillRow SkillAt(PlayerInfo me, int slot)
        {
            if (slot < RunPlayerConst.NumBasicSkills)
            {
                var basics = GameData.FindBasicSkills(me.Class);
                return slot < basics.Count ? basics[slot] : null;
            }
            int e = slot - RunPlayerConst.NumBasicSkills;
            return e < me.Run.EnhanceSkills.Count ? GameData.FindSkillRow(me.Run.EnhanceSkills[e]) : null;
        }

        void RefreshCombat(AreaInfo area)
        {
            var me = ClientState.Me;
            if (me == null) return;
            bool myTurn = area.Phase == "PlayerTurn" && !area.EndedTurn.Contains(me.Id) && me.Combat.Health > 0;
            string phase = area.Phase == "PlayerTurn" ? "플레이어 턴" : area.Phase == "MonsterTurn" ? "몬스터 턴" :
                           area.Phase == "Victory" ? "승리!" : area.Phase == "Defeat" ? "패배..." : area.Phase;
            _phase.text = $"{RoomLabel(area.Room.Type)} 방 · {area.Cycle}사이클 · {phase}" + (myTurn ? "" : area.Phase == "PlayerTurn" ? "  (다른 사람을 기다리는 중)" : "");

            // 아군
            _allies.Clear();
            for (int i = 0; i < area.Occupants.Count; ++i)
            {
                var p = ClientState.Party.Find(area.Occupants[i]);
                if (p == null) continue;
                int allyIndex = i;
                var line = UiKit.Button($"{p.Name}  {p.Combat.Health}/{p.Stats.MaxHealth}" + (p.Combat.Shield > 0 ? $"  보호막 {p.Combat.Shield}" : "") + StatusText(p.Combat),
                    () => UseSelected(allyIndex, true));
                line.style.unityTextAlign = TextAnchor.MiddleLeft;
                line.style.marginBottom = 4;
                if (area.EndedTurn.Contains(p.Id)) line.style.opacity = 0.6f;
                _allies.Add(line);
            }

            // 몬스터
            _monsters.Clear();
            foreach (var m in area.Monsters.Where(x => x.State.Health > 0))
            {
                var mon = m;
                var card = UiKit.Button("", () => UseSelected(mon.Index, false));
                card.style.width = 260;
                card.style.height = 110;
                card.style.marginRight = 10;
                card.style.whiteSpace = WhiteSpace.Normal;
                card.style.unityTextAlign = TextAnchor.UpperLeft;
                string intent = !m.bHasIntent ? "" :
                    m.Intent.Kind == "Attack" ? $"\n예고: 공격 {m.Intent.Amount}" + (m.Intent.HitCount > 1 ? $" x{m.Intent.HitCount}" : "") :
                    m.Intent.Kind == "Defend" ? "\n예고: 방어" : m.Intent.Kind == "Buff" ? "\n예고: 강화" : "\n예고: 약화";
                card.text = $"{m.Name}\n체력 {m.State.Health}/{m.Stats.MaxHealth}" + (m.State.Shield > 0 ? $"  보호막 {m.State.Shield}" : "") + StatusText(m.State) + intent;
                _monsters.Add(card);
            }

            // 스킬
            _energy.text = $"에너지 {me.Combat.Energy}/{me.Stats.MaxEnergy}\n스킬 에너지 {me.Combat.SkillEnergy}/{me.Stats.MaxSkillEnergy}";
            _skills.Clear();
            int slots = RunPlayerConst.NumBasicSkills + Mathf.Max(0, me.Stats.EnhanceSlots);
            for (int i = 0; i < slots; ++i)
            {
                var skill = SkillAt(me, i);
                int slot = i;
                var b = UiKit.Button(skill == null ? "(빈 칸)" : $"{skill.DisplayName_KR}\n{CostText(skill)}", () => SelectSkill(slot));
                b.style.width = 150;
                b.style.height = 64;
                b.style.marginRight = 8;
                b.style.whiteSpace = WhiteSpace.Normal;
                b.SetEnabled(myTurn && skill != null);
                if (slot == _selectedSkill)
                {
                    b.style.borderTopColor = b.style.borderBottomColor = b.style.borderLeftColor = b.style.borderRightColor = UiKit.Hex("E6C47A");
                    b.style.borderTopWidth = b.style.borderBottomWidth = b.style.borderLeftWidth = b.style.borderRightWidth = 3;
                }
                if (skill != null) b.tooltip = skill.Description_KR;
                _skills.Add(b);
            }
            _endTurn.SetEnabled(myTurn);
            if (!myTurn) _selectedSkill = -1;
        }

        static string CostText(SkillRow s)
        {
            var parts = new List<string>();
            if (s.EnergyCost > 0) parts.Add($"에너지 {s.EnergyCost}");
            if (s.SkillEnergyCost > 0) parts.Add($"스킬 에너지 {s.SkillEnergyCost}");
            return string.Join(" · ", parts);
        }

        static string StatusText(CombatState s)
        {
            if (s.Statuses == null || s.Statuses.Count == 0) return "";
            return "\n" + string.Join(" ", s.Statuses.Select(x => StatusInfo.GetDisplayName(x.Type) + (x.Value != 0 ? x.Value.ToString() : "")));
        }

        // 스킬을 누르면: 대상이 필요 없으면 바로, 적 1명 / 아군 1명이면 대상을 누를 때까지 대기
        void SelectSkill(int slot)
        {
            var me = ClientState.Me;
            var skill = me != null ? SkillAt(me, slot) : null;
            if (skill == null) return;
            if (skill.TargetType == TargetType.SingleEnemy || skill.TargetType == TargetType.SingleAlly)
            {
                _selectedSkill = slot;
                Toast(skill.TargetType == TargetType.SingleEnemy ? "대상 몬스터를 고르세요" : "대상 아군을 고르세요", 1.5f);
                Refresh();
                return;
            }
            NetBus.Request(new UseSkillReq { SkillIndex = slot, TargetIndex = 0 });
            _selectedSkill = -1;
        }

        void UseSelected(int targetIndex, bool ally)
        {
            var me = ClientState.Me;
            if (_selectedSkill < 0 || me == null) return;
            var skill = SkillAt(me, _selectedSkill);
            if (skill == null) return;
            if (ally != (skill.TargetType == TargetType.SingleAlly)) return;
            NetBus.Request(new UseSkillReq { SkillIndex = _selectedSkill, TargetIndex = targetIndex });
            _selectedSkill = -1;
        }
    }

    // RunPlayer 상수 (Core.Run 에 있음. UI 에서 짧게 쓰려고)
    static class RunPlayerConst
    {
        public const int NumBasicSkills = Terminus.Run.RunPlayer.NumBasicSkills;
    }
}
