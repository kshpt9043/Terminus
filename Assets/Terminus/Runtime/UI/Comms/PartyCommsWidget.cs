// 파티 소통 도구 화면 (언리얼 UPartyCommsWidget / WBP_PartyComms, 멀티 전용). 채팅 말고 전투 중 빠르게 의사를 나누는 것들 (UILayer.Comms)
//
//  1. 행동 계획 공유: 유니티에선 전투 HUD(CombatHud)가 머리 위에 그림 (언리얼은 이 위젯이 그렸음)
//  2. 핑: 전투 중 배틀러를 휠 클릭(또는 Alt + 클릭) -> 메뉴(적: 집중 공격 / 위험, 아군: 보호 필요 / 회복 필요)
//  3. 퀵챗: T -> 문구 메뉴 (숫자 키로 바로). 채팅창에 한 줄 + 보낸 사람 머리 위 말풍선
//  4. 재촉: 내 턴을 끝냈는데 아직 안 끝낸 사람이 있으면 오른쪽 아래 '재촉' 버튼 -> 그 사람 화면에 알림
//
// 화면은 코드가 전부 만듦 (WBP 는 빈 캔버스). 바탕은 클릭을 통과시킴 -> 아래 전투 HUD / 지도 조작을 막지 않음
// 키 / 마우스는 패널 맨 위(visualTree)에서 먼저 봄 (언리얼 Slate 입력 전처리기 자리)
// 글을 쓰는 중(입력칸 포커스)이거나, 팝업이 떠 있거나, 로딩 화면이면 가로채지 않음
//
// 배틀러 위치는 AreaView.TryGetMonsterBounds / TryGetPlayerBounds (언리얼은 액터 위치를 화면에 투영)
using System.Collections.Generic;
using System.Linq;
using Terminus.Comms;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Chat;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Comms
{
    public sealed class PartyCommsWidget : VisualElement
    {
        // ---- 입력
        public KeyCode QuickChatKey = KeyCode.T;

        // 핑 메뉴: 휠 클릭. AltClickPing 이면 Alt + 왼쪽 클릭도 (휠 없는 마우스 / 노트북)
        public bool AltClickPing = true;

        // 배틀러 몸 중심에서 이 거리(화면 px) 안을 누르면 그 배틀러를 고른 걸로
        public float PickRadius = 70f;

        // ---- 배치
        // 머리 위 표시를 머리보다 얼마나 더 위에 둘지 (화면 px). 몬스터 행동 예고 글자 위로 올리려고
        public float HeadScreenOffset = 30f;

        // 재촉 버튼 위치 (화면 오른쪽 아래 기준). 전투 HUD 의 턴 종료 / 유물 줄 위
        public Vector2 NudgeButtonPosition = new Vector2(-24f, -200f);

        public int FontSize = 15;

        // ---- 시간 (초)
        public float PingDuration = 8f;
        public float BubbleDuration = 3.5f;
        public float ToastDuration = 3f;

        // 재촉 버튼 다시 누를 수 있기까지. 서버도 따로 막음
        public float NudgeCooldown = 10f;

        // 배틀러 하나의 머리 위 표시 묶음 (위에서부터 말풍선 / 핑)
        sealed class CommsTag
        {
            public CommsTarget Battler;
            public VisualElement Box;
            public VisualElement Bubble;
            public Label BubbleText;
            public Label PingText;
        }

        sealed class ActivePing
        {
            public PartyPing Ping;
            public int AreaIndex;
            public string SenderName;
            public float ExpireTime;
        }

        sealed class ActiveBubble
        {
            public ulong Sender;
            public string Text;
            public float ExpireTime;
        }

        static PartyCommsWidget _current;
        static bool _hooked;

        readonly Label _toastText;
        readonly Button _nudgeButton;
        readonly VisualElement _tagLayer;
        readonly CommsMenuWidget _menu = new CommsMenuWidget();

        readonly List<CommsTag> _tags = new List<CommsTag>();
        string _taggedKey = "";
        readonly List<ActivePing> _pings = new List<ActivePing>();
        readonly List<ActiveBubble> _bubbles = new List<ActiveBubble>();
        float _toastExpireTime;
        float _nextNudgeTime;
        Vector2 _lastPointer;
        VisualElement _inputRoot;
        IVisualElementScheduledItem _tick;

        static float Now => Time.realtimeSinceStartup;

        public PartyCommsWidget()
        {
            name = "CommsRoot";
            pickingMode = PickingMode.Ignore;

            _tagLayer = new VisualElement { name = "Tags", pickingMode = PickingMode.Ignore };
            _tagLayer.style.position = Position.Absolute;
            _tagLayer.style.left = 0;
            _tagLayer.style.top = 0;
            _tagLayer.style.right = 0;
            _tagLayer.style.bottom = 0;
            Add(_tagLayer);

            // 재촉 / 알림 (위 가운데)
            _toastText = MakeCommsText(FontSize + 7, UiColor.GoldHover);
            _toastText.style.position = Position.Absolute;
            _toastText.style.left = Length.Percent(50);
            _toastText.style.top = 90f;
            _toastText.style.translate = new Translate(Length.Percent(-50), 0);
            Add(_toastText);
            UiStyle.SetShown(_toastText, false);

            // 재촉 버튼 (오른쪽 아래). 보조 행동 육각 버튼
            _nudgeButton = UiStyle.HexButton("재촉", HexKind.Secondary, HandleNudgeClicked, FontSize + 2);
            _nudgeButton.style.position = Position.Absolute;
            _nudgeButton.style.right = -NudgeButtonPosition.x;
            _nudgeButton.style.bottom = -NudgeButtonPosition.y;
            UiStyle.AddShadow(_nudgeButton, 0.9f);
            UiTooltip.SetText(_nudgeButton, "재촉하기\n아직 턴을 끝내지 않은 동료에게 알림을 보냅니다.");
            Add(_nudgeButton);
            UiStyle.SetShown(_nudgeButton, false);

            RegisterCallback<AttachToPanelEvent>(OnAttach);
            RegisterCallback<DetachFromPanelEvent>(OnDetach);
        }

        static Label MakeCommsText(int size, Color color)
        {
            var text = UiStyle.Text("", size, color, TextAnchor.MiddleCenter);
            text.pickingMode = PickingMode.Ignore;
            UiStyle.AddShadow(text, 0.9f);
            return text;
        }

        // 같은 글자면 다시 안 넣음 + 비었으면 숨김
        static void SetCommsText(Label text, string value) => UiStyle.SetLine(text, value);

        void OnAttach(AttachToPanelEvent e)
        {
            _current = this;
            Hook();

            // 키 / 클릭은 포커스가 어디 있든 (언리얼 입력 전처리기) -> 패널 맨 위에서 먼저 봄
            _inputRoot = panel?.visualTree;
            if (_inputRoot != null)
            {
                _inputRoot.RegisterCallback<KeyDownEvent>(OnKeyDown, TrickleDown.TrickleDown);
                _inputRoot.RegisterCallback<PointerDownEvent>(OnPointerDown, TrickleDown.TrickleDown);
                _inputRoot.RegisterCallback<PointerMoveEvent>(OnPointerMove, TrickleDown.TrickleDown);
            }
            if (_tick == null) _tick = schedule.Execute(Tick).Every(16);
            else _tick.Resume();
        }

        void OnDetach(DetachFromPanelEvent e)
        {
            if (_current == this) _current = null;
            if (_inputRoot != null)
            {
                _inputRoot.UnregisterCallback<KeyDownEvent>(OnKeyDown, TrickleDown.TrickleDown);
                _inputRoot.UnregisterCallback<PointerDownEvent>(OnPointerDown, TrickleDown.TrickleDown);
                _inputRoot.UnregisterCallback<PointerMoveEvent>(OnPointerMove, TrickleDown.TrickleDown);
                _inputRoot = null;
            }
            _tick?.Pause();
            _menu.Close();
        }

        // ------------------------------------------------------------ 서버에서 받은 것 (언리얼 PC 의 Client_Receive*). 처리기는 한 번만 붙임

        // NetBus 가 Play 마다 처리기를 비움 -> 다시 붙도록
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetHook() => _hooked = false;

        static void Hook()
        {
            if (_hooked) return;
            _hooked = true;

            NetBus.On<PingMsg>(m =>
            {
                if (m?.Ping == null) return;
                // 채팅창에도 한 줄 (화면을 놓쳐도 남게)
                ChatView.AddLocal(new ChatMsg { bSystem = true, Text = PartyComms.FormatPingLine(m.SenderName, m.TargetName, m.Ping.Kind) });
                _current?.HandlePing(m);
            });
            NetBus.On<QuickChatMsg>(m =>
            {
                if (m == null) return;
                var text = PartyComms.GetQuickChatText(m.Kind);
                if (string.IsNullOrEmpty(text)) return;
                // 채팅창에 보통 말처럼 한 줄 (소통 화면이 없어도 남김)
                ChatView.AddLocal(new ChatMsg { Sender = m.SenderName, Text = text });
                _current?.HandleQuickChat(m.Sender, text);
            });
            NetBus.On<NudgedMsg>(m =>
            {
                if (m == null || _current == null) return;
                var name = string.IsNullOrEmpty(m.SenderName) ? "?" : m.SenderName;
                _current.ShowToast(PartyComms.NudgeToastText(name));
                ChatView.AddLocal(new ChatMsg { bSystem = true, Text = PartyComms.NudgeChatText(name) });
            });
        }

        void HandlePing(PingMsg m)
        {
            // 한 사람당 핑 하나. 새로 찍으면 이전 것은 지움
            _pings.RemoveAll(p => p.Ping.Sender == m.Ping.Sender);
            _pings.Add(new ActivePing { Ping = m.Ping, AreaIndex = m.AreaIndex, SenderName = m.SenderName, ExpireTime = Now + PingDuration });
        }

        void HandleQuickChat(ulong sender, string text)
        {
            // 보낸 사람이 지금 화면에 있으면 머리 위 말풍선
            _bubbles.RemoveAll(b => b.Sender == sender);
            _bubbles.Add(new ActiveBubble { Sender = sender, Text = text, ExpireTime = Now + BubbleDuration });
        }

        public void ShowToast(string message)
        {
            _toastText.text = message ?? "";
            UiStyle.SetShown(_toastText, true);
            _toastExpireTime = Now + ToastDuration;
        }

        // ------------------------------------------------------------ 입력

        bool CanTakeKeyboard()
        {
            if (panel == null) return false;

            // 채팅 / 비밀번호 같은 입력칸에 글을 쓰는 중이면 그쪽 키
            var focused = panel.focusController?.focusedElement as VisualElement;
            if (focused != null && (focused is TextField || focused.GetFirstAncestorOfType<TextField>() != null)) return false;
            if (ChatView.IsAnyInputOpen) return false;
            if (LoadingScreenWidget.IsShowing) return false;
            return true;
        }

        // 팝업 / 다른 창이 떠 있으면 그쪽 우선 (언리얼 EscapeStack HasOpenEntry)
        static bool HasOpenWindow()
        {
            var root = UIRoot.Instance;
            if (root == null) return false;
            var popup = root.GetLayer(UILayer.Popup);
            var pick = root.GetLayer(UILayer.Pick);
            return (popup != null && popup.childCount > 0) || (pick != null && pick.childCount > 0);
        }

        void OnKeyDown(KeyDownEvent e)
        {
            if (e.keyCode == KeyCode.None || !CanTakeKeyboard()) return;
            var key = e.keyCode;

            // 메뉴가 떠 있으면 숫자 키로 고르기, 같은 키 / ESC 로 닫기
            if (_menu.IsOpen)
            {
                int number = key >= KeyCode.Alpha1 && key <= KeyCode.Alpha9 ? key - KeyCode.Alpha0
                    : key >= KeyCode.Keypad1 && key <= KeyCode.Keypad9 ? key - KeyCode.Keypad0 : 0;
                if (number > 0)
                {
                    if (_menu.PickByNumber(number)) e.StopPropagation();
                    return;
                }
                if (key == QuickChatKey || key == KeyCode.Escape)
                {
                    _menu.Close();
                    e.StopPropagation();
                }
                return;
            }

            if (key != QuickChatKey || HasOpenWindow()) return;
            OpenQuickChatMenu();
            e.StopPropagation();
        }

        void OnPointerMove(PointerMoveEvent e) => _lastPointer = e.position;

        void OnPointerDown(PointerDownEvent e)
        {
            _lastPointer = e.position;
            bool pingGesture = e.button == 2 || (AltClickPing && e.button == 0 && e.altKey);

            // 메뉴 밖을 누르면 메뉴 닫기. 그 클릭은 원래 하려던 일(버튼 등)에 그대로 감
            if (_menu.IsOpen && !_menu.IsOverPanel(e.position)) _menu.Close();

            if (!pingGesture) return;
            if (!FindPingTarget(e.position, out var target)) return;

            OpenPingMenu(target, e.position);
            e.StopPropagation();
        }

        static bool IsFighting(AreaInfo area) => area != null && (area.Phase == "PlayerTurn" || area.Phase == "MonsterTurn");

        static bool InCombat(AreaInfo area) => area != null && area.bInUse && !string.IsNullOrEmpty(area.Phase) && area.Phase != "None";

        // 지금 보는 구역의 배틀러 (몬스터 + 플레이어)
        static List<CommsTarget> GatherBattlers(AreaInfo area)
        {
            var output = new List<CommsTarget>();
            if (area == null || !area.bInUse) return output;
            if (InCombat(area))
            {
                foreach (var m in area.Monsters) output.Add(CommsTarget.Monster(m.Id));
            }
            foreach (var id in area.Occupants) output.Add(CommsTarget.Player(id));
            return output;
        }

        static bool IsAliveBattler(AreaInfo area, CommsTarget t)
        {
            if (t.IsMonster)
            {
                var m = area.Monsters.Find(x => x.Id == t.MonsterId);
                return m != null && !m.bHide && m.State != null && m.State.Health > 0;
            }
            var p = ClientState.Party.Find(t.PlayerId);
            return p != null && p.bHasBattler && p.Combat != null && p.Combat.Health > 0;
        }

        static bool TryGetBounds(CommsTarget t, out Bounds bounds) =>
            t.IsMonster ? AreaView.TryGetMonsterBounds(t.MonsterId, out bounds) : AreaView.TryGetPlayerBounds(t.PlayerId, out bounds);

        // 월드 -> 이 요소 좌표
        bool Project(Vector3 world, out Vector2 local)
        {
            local = default;
            var cam = AreaView.ViewCamera;
            if (cam == null || panel == null) return false;
            var p = RuntimePanelUtils.CameraTransformWorldToPanel(panel, world, cam);
            local = this.WorldToLocal(p);
            return true;
        }

        // Viewer 편에서 대상이 적인가 (몬스터, 배신 전투면 상대 편 플레이어)
        static bool IsEnemyOf(AreaInfo area, ulong viewer, CommsTarget t)
        {
            if (t.IsMonster) return true;
            return area != null && area.bBetrayal && ((viewer == area.BetrayerId) != (t.PlayerId == area.BetrayerId));
        }

        // 핑 메뉴를 열 수 있는 상태에서 마우스 아래 배틀러
        bool FindPingTarget(Vector2 panelPosition, out CommsTarget best)
        {
            best = null;
            var area = AreaView.ShownArea;
            var myArea = ClientState.MyArea;

            // 내가 들어 있는 구역의 싸움 중에만 (관전 중엔 못 찍음)
            if (area == null || myArea == null || area.AreaIndex != myArea.AreaIndex || !IsFighting(area)) return false;

            var local = this.WorldToLocal(panelPosition);
            float bestDistSq = PickRadius * PickRadius;
            foreach (var t in GatherBattlers(area))
            {
                if (!IsAliveBattler(area, t) || !TryGetBounds(t, out var b)) continue;
                if (!Project(b.center, out var body)) continue;
                float d = (body - local).sqrMagnitude;
                if (d <= bestDistSq)
                {
                    bestDistSq = d;
                    best = t;
                }
            }
            return best != null;
        }

        // 핑 / 메뉴에 쓰는 이름 ("적 2", 플레이어 이름). 몬스터 = 지금 보는 구역 몬스터 목록의 순서
        static string DescribeTarget(AreaInfo area, CommsTarget t)
        {
            if (t.bPlayer) return ClientState.NameOf(t.PlayerId);
            if (area != null)
            {
                int i = area.Monsters.FindIndex(m => m.Id == t.MonsterId);
                if (i >= 0) return "적 " + (i + 1);
            }
            return "적";
        }

        void OpenPingMenu(CommsTarget target, Vector2 panelPosition)
        {
            var area = AreaView.ShownArea;

            // 적이면 집중 공격 / 위험, 아군(나 포함)이면 보호 필요 / 회복 필요
            var kinds = IsEnemyOf(area, ClientState.MyId, target)
                ? new[] { PingKind.Focus, PingKind.Danger }
                : new[] { PingKind.NeedGuard, PingKind.NeedHeal };
            var labels = kinds.Select(PartyComms.GetPingText).ToList();

            var sent = target.Clone();
            _menu.Open("핑: " + DescribeTarget(area, target), labels, MenuPosition(panelPosition), index =>
            {
                if (index >= 0 && index < kinds.Length) NetBus.Request(new SendPingReq { Kind = kinds[index], Target = sent });
            });
        }

        public void OpenQuickChatMenu()
        {
            var labels = new List<string>();
            for (int i = 0; i < PartyComms.QuickChatCount; ++i) labels.Add(PartyComms.GetQuickChatText((QuickChat)i));

            _menu.Open("퀵챗 (숫자 키)", labels, MenuPosition(_lastPointer), index =>
            {
                if (index >= 0 && index < PartyComms.QuickChatCount) NetBus.Request(new SendQuickChatReq { Kind = (QuickChat)index });
            });
        }

        // 메뉴 레이어(화면 전체) 좌표. 이 요소도 화면 전체라 패널 좌표 그대로
        Vector2 MenuPosition(Vector2 panelPosition)
        {
            var layer = UIRoot.Instance != null ? UIRoot.Instance.GetLayer(UILayer.CommsMenu) : null;
            return layer != null ? layer.WorldToLocal(panelPosition) : panelPosition;
        }

        void HandleNudgeClicked()
        {
            if (Now < _nextNudgeTime) return;
            _nextNudgeTime = Now + NudgeCooldown;
            NetBus.Request(new NudgeReq());
        }

        // ------------------------------------------------------------ 매 틱: 머리 위 표시 / 재촉 버튼 / 알림

        void Tick()
        {
            float now = Now;

            // 시간이 다 된 것 정리
            _pings.RemoveAll(p => p.ExpireTime <= now);
            _bubbles.RemoveAll(b => b.ExpireTime <= now);

            if (_toastExpireTime > 0f && now >= _toastExpireTime)
            {
                _toastExpireTime = 0f;
                UiStyle.SetShown(_toastText, false);
            }

            var area = AreaView.ShownArea;

            // 배틀러 구성이 바뀌면(새 방, 스냅샷 도착) 표시를 다시 만듦
            var battlers = GatherBattlers(area);
            string key = (area != null ? area.AreaIndex : -1) + "|" + string.Join(",", battlers.Select(b => b.ToString()));
            if (key != _taggedKey)
            {
                _taggedKey = key;
                RebuildTags(battlers);
            }

            UpdateTags(area);
            UpdateNudgeButton();
        }

        void RebuildTags(List<CommsTarget> battlers)
        {
            _tagLayer.Clear();
            _tags.Clear();

            foreach (var battler in battlers)
            {
                var box = new VisualElement { pickingMode = PickingMode.Ignore };
                box.style.position = Position.Absolute;
                box.style.alignItems = Align.Center;
                box.style.translate = new Translate(Length.Percent(-50), Length.Percent(-100));

                // 말풍선 (판 + 테두리, 본문 글자)
                var bubble = new VisualElement { pickingMode = PickingMode.Ignore };
                bubble.style.backgroundColor = UiColor.Hex("1B1E25", 0.95f);
                UiStyle.Border(bubble, 1f, UiColor.Line);
                UiStyle.Padding(bubble, 10f, 4f);
                bubble.style.marginTop = 1f;
                bubble.style.marginBottom = 1f;
                var bubbleText = UiStyle.Text("", FontSize + 1, UiColor.TextBody, TextAnchor.MiddleCenter);
                bubbleText.pickingMode = PickingMode.Ignore;
                bubble.Add(bubbleText);
                box.Add(bubble);
                UiStyle.SetShown(bubble, false);

                var pingText = MakeCommsText(FontSize + 1, UiColor.TextBody);
                pingText.style.marginTop = 1f;
                pingText.style.marginBottom = 1f;
                box.Add(pingText);
                UiStyle.SetShown(pingText, false);

                _tagLayer.Add(box);
                _tags.Add(new CommsTag { Battler = battler, Box = box, Bubble = bubble, BubbleText = bubbleText, PingText = pingText });
            }
        }

        void UpdateTags(AreaInfo area)
        {
            bool fighting = IsFighting(area);

            foreach (var tag in _tags)
            {
                var t = tag.Battler;
                bool hidden = t.IsMonster && (area == null || area.Monsters.Find(m => m.Id == t.MonsterId)?.bHide != false);
                if (area == null || hidden || !TryGetBounds(t, out var bounds))
                {
                    UiStyle.SetShown(tag.Box, false);
                    continue;
                }

                bool alive = IsAliveBattler(area, t);

                // ---- 말풍선 (플레이어만, 쓰러져도 보임)
                var bubble = t.bPlayer ? _bubbles.Find(b => b.Sender == t.PlayerId) : null;
                UiStyle.SetShown(tag.Bubble, bubble != null);
                if (bubble != null) UiStyle.SetTextIfChanged(tag.BubbleText, bubble.Text);

                // ---- 핑 (싸우는 중, 살아 있는 대상)
                string pingLabel = "";
                var pingColor = UiColor.TextBody;
                if (fighting && alive)
                {
                    foreach (var active in _pings)
                    {
                        if (active.AreaIndex != area.AreaIndex || !active.Ping.Target.Same(t)) continue;
                        if (pingLabel.Length > 0) pingLabel += "\n";
                        pingLabel += $"[{PartyComms.GetPingText(active.Ping.Kind)}] {active.SenderName}";
                        pingColor = UiColor.Hex(PartyComms.GetPingColorHex(active.Ping.Kind));   // 여러 개면 마지막 핑 색
                    }
                }
                tag.PingText.style.color = pingColor;
                SetCommsText(tag.PingText, pingLabel);

                // ---- 보일지 / 자리
                bool anything = bubble != null || pingLabel.Length > 0;
                UiStyle.SetShown(tag.Box, anything);
                if (!anything) continue;

                var head = new Vector3(bounds.center.x, bounds.max.y, bounds.center.z);
                if (Project(head, out var headPos))
                {
                    tag.Box.style.left = headPos.x;
                    tag.Box.style.top = headPos.y - HeadScreenOffset;
                }
            }
        }

        static bool IsTurnOf(AreaInfo area, ulong playerId) =>
            area != null && area.Phase == "PlayerTurn" && (!area.bBetrayal || ((playerId == area.BetrayerId) == area.bBetrayerTurn));

        static bool IsAlivePlayer(ulong id)
        {
            var p = ClientState.Party.Find(id);
            return p != null && p.bConnected && p.bHasBattler && p.Combat != null && p.Combat.Health > 0;
        }

        void UpdateNudgeButton()
        {
            // 내 구역의 플레이어 턴이고, 나는 할 게 없는데(끝냈거나 상대 편 차례이거나 쓰러짐) 아직 안 끝낸 사람이 있을 때
            bool show = false;
            var area = AreaView.ShownArea;
            var myArea = ClientState.MyArea;
            ulong me = ClientState.MyId;
            if (area != null && myArea != null && area.AreaIndex == myArea.AreaIndex && area.Phase == "PlayerTurn" && area.Occupants.Contains(me))
            {
                bool meWaiting = area.EndedTurn.Contains(me) || !IsTurnOf(area, me) || !IsAlivePlayer(me);
                if (meWaiting)
                {
                    foreach (var other in area.Occupants)
                    {
                        if (other == me) continue;
                        if (IsTurnOf(area, other) && !area.EndedTurn.Contains(other) && IsAlivePlayer(other))
                        {
                            show = true;
                            break;
                        }
                    }
                }
            }

            UiStyle.SetShown(_nudgeButton, show);
            if (!show) return;

            float remaining = _nextNudgeTime - Now;
            _nudgeButton.SetEnabled(remaining <= 0f);
            var label = remaining > 0f ? $"재촉 ({Mathf.CeilToInt(remaining)})" : "재촉";
            if (_nudgeButton.text != label) _nudgeButton.text = label;
        }
    }
}
