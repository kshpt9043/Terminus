// 던전 화면 바탕 = 지도 화면 (언리얼 UMapScreenWidget / WBP_MapScreen) + 던전 PC 가 띄우던 화면들 (UIApp 이 UILayer.Map 에 하나)
//
// 지도 화면 (WBP_MapScreen 그대로):
//   Border_46 (#0E0E10, 화면 전체)
//   MapCanvas (가로 가운데, 폭 max(780, MapCanvasWidth 1240), 세로 전체)
//   Vignette (T_Map_Vignette, 화면 전체, 클릭 통과)
//   MapTopBar (위, 높이 64)
//   HintBox (왼쪽 아래 28 / 28): HintText 15 #E8DFCB "다음 방을 고르세요" (+ 퀘스트 한 줄, 툴팁) / HintSubText 13 #A89F8C (위 6)
//   RelicBar (오른쪽 아래 28 / 28. WBP 에 없어서 C++ 이 만들던 것)
// 던전 구역에 들어가면(그 구역을 보면) 화면째 숨기고, 지도로 돌아오면 다시 보임 (위젯은 그대로라 스크롤 위치도 유지)
//
// 언리얼 ATerminusPlayerController 가 띄우던 것도 여기서:
//   전투 HUD (UILayer.CombatHUD, 스스로 보임 / 숨김), 방 패널 (RoomPanels.Attach / Detach), 시작 스킬 / 유물 고르기,
//   방 상단바 (휴식 / 이벤트 / 상점 화면 위), 층 도착 타이틀, 층 투표, 관전 바, 이공간, 방 선택 실패 / 투표 거절 팝업, 소통 도구(멀티)
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Map;
using Terminus.Run;
using Terminus.UI.Combat;
using Terminus.UI.Comms;
using Terminus.UI.Common;
using Terminus.UI.Rooms;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class DungeonView : VisualElement
    {
        // 지도 칸 폭 (WBP 슬롯 값 780 보다 작으면 이 값으로). 열 간격 280 x 3 + 여백 기준
        public float MapCanvasWidth = 1240f;

        // ------------------------------------------------------------ 지도 화면

        readonly VisualElement _mapScreen;
        readonly MapCanvasWidget _mapCanvas;
        readonly MapTopBarWidget _mapTopBar;
        readonly Label _hintText;
        readonly Label _hintSubText;
        readonly RelicBarWidget _relicBar;

        // ------------------------------------------------------------ 던전 PC 가 띄우던 것

        readonly CombatHud _combatHud;
        readonly SpectateWidget _spectate;
        readonly RoomTopBarWidget _roomTopBar;
        readonly PartyCommsWidget _comms;
        FloorVoteWidget _floorVote;
        RiftWidget _rift;
        FloorTitleWidget _floorTitle;
        VisualElement _skillPick;
        VisualElement _relicPick;

        bool _skillPickOpened;
        bool _relicPickOpened;
        bool _restOpen, _eventOpen, _shopOpen;
        bool _mapShown = true;
        int _lastMyArea = int.MinValue;
        string _shownVoteKey = "";
        string _shownQuestTip;
        bool _attached;

        // NetBus 처리기는 한 번만 붙이고 지금 화면으로 넘김 (씬마다 화면이 새로 생겨도 처리기가 쌓이지 않게)
        static DungeonView _current;
        static bool _hooked;
        static FloorTitleMsg _pendingTitle;

        public DungeonView()
        {
            DungeonStyle.Use(this);
            name = "DungeonView";
            pickingMode = PickingMode.Ignore;

            // ---- 지도 화면
            _mapScreen = new VisualElement { name = "MapScreen", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(_mapScreen);
            Add(_mapScreen);

            var background = new VisualElement { name = "Border_46", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(background);
            background.style.backgroundColor = UiColor.FromLinear(0.004391f, 0.004391f, 0.005182f);
            _mapScreen.Add(background);

            // 지도 칸 폭: WBP 는 780 고정이라 열 간격을 넓히면 오른쪽 방이 잘림 -> 넓혀 줌 (11-04 피드백: 양 옆 빈 공간)
            float width = Mathf.Max(780f, MapCanvasWidth);
            _mapCanvas = new MapCanvasWidget();
            _mapCanvas.style.position = Position.Absolute;
            _mapCanvas.style.top = 0;
            _mapCanvas.style.bottom = 0;
            _mapCanvas.style.left = Length.Percent(50);
            _mapCanvas.style.width = width;
            _mapCanvas.style.marginLeft = -width * 0.5f;
            _mapScreen.Add(_mapCanvas);

            var vignette = new VisualElement { name = "Vignette", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(vignette);
            DungeonStyle.SetImage(vignette, MapTextures.Vignette);
            vignette.style.backgroundSize = new BackgroundSize(Length.Percent(100), Length.Percent(100));
            _mapScreen.Add(vignette);

            _mapTopBar = new MapTopBarWidget();
            _mapTopBar.style.position = Position.Absolute;
            _mapTopBar.style.left = 0;
            _mapTopBar.style.right = 0;
            _mapTopBar.style.top = 0;
            _mapTopBar.style.height = RoomTopBarWidget.BarHeight;
            _mapScreen.Add(_mapTopBar);

            var hintBox = new VisualElement { name = "HintBox", pickingMode = PickingMode.Ignore };
            hintBox.style.position = Position.Absolute;
            hintBox.style.left = 28f;
            hintBox.style.bottom = 28f;
            _mapScreen.Add(hintBox);

            _hintText = UiStyle.Text("다음 방을 고르세요", 15, UiColor.FromLinear(0.806952f, 0.737910f, 0.597202f), TextAnchor.UpperLeft);
            _hintText.pickingMode = PickingMode.Ignore;
            hintBox.Add(_hintText);

            _hintSubText = UiStyle.Text("마우스 휠로 지도 이동", 13, UiColor.FromLinear(0.391572f, 0.346704f, 0.262251f), TextAnchor.UpperLeft);
            _hintSubText.pickingMode = PickingMode.Ignore;
            _hintSubText.style.marginTop = 6f;
            hintBox.Add(_hintSubText);

            // 보유 유물 줄. 전투 HUD 와 같은 자리(오른쪽 아래)
            _relicBar = new RelicBarWidget();
            _relicBar.style.position = Position.Absolute;
            _relicBar.style.right = 28f;
            _relicBar.style.bottom = 28f;
            _mapScreen.Add(_relicBar);

            // ---- 다른 레이어에 붙는 화면들 (붙을 때 / 떨어질 때 같이)
            _combatHud = new CombatHud();
            _spectate = new SpectateWidget();
            _spectate.ViewChanged += Refresh;
            _roomTopBar = new RoomTopBarWidget();
            // 혼자면 나눌 사람이 없음 (채팅창과 같은 기준)
            _comms = NetworkRoot.IsSinglePlayer ? null : new PartyCommsWidget();

            RegisterCallback<AttachToPanelEvent>(_ => OnAttach());
            RegisterCallback<DetachFromPanelEvent>(_ => OnDetach());
        }

        // ------------------------------------------------------------ 붙기 / 떨어지기

        void OnAttach()
        {
            if (_attached) return;
            _attached = true;
            _current = this;
            Hook();

            var root = UIRoot.Instance;
            if (root != null)
            {
                root.Show(UILayer.CombatHUD, _combatHud);
                root.Show(UILayer.Overlay, _spectate);
                if (_comms != null) root.Show(UILayer.Comms, _comms);
            }
            RoomPanels.Attach();

            ClientState.PartyChanged -= Refresh;
            ClientState.PartyChanged += Refresh;
            ClientState.MapChanged -= Refresh;
            ClientState.MapChanged += Refresh;
            ClientState.AreasChanged -= Refresh;
            ClientState.AreasChanged += Refresh;

            Refresh();

            if (_pendingTitle != null)
            {
                var title = _pendingTitle;
                _pendingTitle = null;
                ShowFloorTitle(title);
            }
        }

        void OnDetach()
        {
            if (!_attached) return;
            _attached = false;
            if (_current == this) _current = null;

            ClientState.PartyChanged -= Refresh;
            ClientState.MapChanged -= Refresh;
            ClientState.AreasChanged -= Refresh;

            RoomPanels.Detach();
            foreach (var e in new VisualElement[] { _combatHud, _spectate, _comms, _roomTopBar, _floorVote, _rift, _floorTitle, _skillPick, _relicPick })
            {
                e?.RemoveFromHierarchy();
            }
            AreaView.SpectateArea = -1;
        }

        // NetBus 가 Play 마다 처리기를 비움 -> 다시 붙도록
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetHook() => _hooked = false;

        static void Hook()
        {
            if (_hooked) return;
            _hooked = true;

            // 층 도착 타이틀은 씬을 다 불러온 직후에 올 수 있음 -> 화면이 아직 없으면 붙을 때 띄움
            NetBus.On<FloorTitleMsg>(m =>
            {
                if (_current != null && _current._attached) _current.ShowFloorTitle(m);
                else _pendingTitle = m;
            });
            NetBus.On<RoomSelectFailedMsg>(m => _current?.OnRoomSelectFailed(m));
            NetBus.On<FloorVoteRejectedMsg>(m => _current?.OnFloorVoteRejected(m));
            NetBus.On<EnterRiftMsg>(m => _current?.OnEnterRift(m));
            NetBus.On<LeaveRiftMsg>(_ => _current?.OnLeaveRift());

            // 방 상단바: 휴식 / 이벤트 / 상점 화면이 열려 있는 동안 (언리얼 PC::UpdateRoomTopBar)
            NetBus.On<ShowRestMsg>(_ => _current?.SetRoomPanel(RestPanel, true));
            NetBus.On<CloseRestMsg>(_ => _current?.SetRoomPanel(RestPanel, false));
            NetBus.On<ShowEventMsg>(_ => _current?.SetRoomPanel(EventPanel, true));
            NetBus.On<CloseEventMsg>(_ => _current?.SetRoomPanel(EventPanel, false));
            NetBus.On<ShowShopMsg>(_ => _current?.SetRoomPanel(ShopPanel, true));
            NetBus.On<CloseShopMsg>(_ => _current?.SetRoomPanel(ShopPanel, false));
        }

        // ------------------------------------------------------------ 갱신 (스냅샷이 바뀔 때)

        void Refresh()
        {
            if (!_attached) return;
            var me = ClientState.Me;

            // 내 구역이 바뀌면 그 구역을 봄 (언리얼 PC::ViewDungeonArea. 관전은 풀림)
            int myArea = me != null ? me.AreaIndex : -1;
            if (myArea != _lastMyArea)
            {
                _lastMyArea = myArea;
                AreaView.SpectateArea = -1;
            }

            // 구역 진입 / 복귀에 맞춰 지도 화면째 숨기기
            var shown = AreaView.ShownArea;
            bool viewingArea = shown != null && shown.bInUse;
            if (viewingArea == _mapShown)
            {
                _mapShown = !viewingArea;
                UiStyle.SetShown(_mapScreen, _mapShown);
                // 방을 끝내고 지도로 돌아오면 이제 고를 줄이 가운데 오게
                if (_mapShown) _mapCanvas.CenterOnSelectableRooms();
            }

            _mapCanvas.Refresh();
            RefreshHint();
            RefreshStartPicks(me);
            RefreshFloorVote();
            RefreshRoomTopBar();
            _spectate.Refresh();
        }

        // 왼쪽 아래 안내 (언리얼 RefreshHint)
        void RefreshHint()
        {
            var me = ClientState.Me;
            var players = ClientState.Party.Players.Where(p => p.bConnected).ToList();
            int total = players.Count;
            int chosen = players.Count(p => p.Run != null && p.Run.SelectedRoomId >= 0);

            bool mine = me != null && me.Run != null && me.Run.SelectedRoomId >= 0;
            string hint = mine && total > 1 ? "동료의 선택을 기다리는 중" : "다음 방을 고르세요";

            // 진행 중 퀘스트 한 줄
            string quest = me != null ? Quests.DescribeActive(me.Run) : "";
            if (!string.IsNullOrEmpty(quest)) hint += "\n" + quest;
            UiStyle.SetTextIfChanged(_hintText, hint);

            // 진행 중 퀘스트가 있으면 안내 줄에 마우스를 올려 의뢰 설명을 볼 수 있게 (11-04 피드백)
            string questRow = me?.Run?.ActiveQuest?.Quest ?? "";
            string questTip = Ids.IsNone(questRow) ? "" : Quests.DescribeOffer(questRow);
            if (_shownQuestTip != questTip)
            {
                _shownQuestTip = questTip;
                // 첫 줄 = 의뢰 이름 (금색 제목)
                var questData = Ids.IsNone(questRow) ? null : GameData.FindQuestRow(questRow);
                UiTooltip.SetText(_hintText, string.IsNullOrEmpty(questTip) ? "" : $"{(questData != null ? questData.QuestName : "의뢰")}\n{questTip}");
                // 안내 줄(퀘스트) 툴팁이 뜨게: 퀘스트가 있을 때만 마우스를 받음
                _hintText.pickingMode = string.IsNullOrEmpty(questTip) ? PickingMode.Ignore : PickingMode.Position;
            }

            // CurrentMapLevel = 지금까지 지난 방 수 -> 다음에 들어갈 방은 그 다음
            var rooms = ClientState.Map.Rooms;
            int levels = rooms != null && rooms.Count > 0 ? rooms.Max(r => r.Row) + 1 : 12;
            int next = Mathf.Min((me?.Run?.CurrentMapLevel ?? 0) + 1, levels);
            string sub = $"다음 방 {next} / {levels}";
            if (total > 1) sub += $" · {chosen} / {total}명 선택";
            sub += " · 마우스 휠 / 스크롤바 / 오른쪽 끌기로 지도 이동";
            UiStyle.SetTextIfChanged(_hintSubText, sub);
        }

        // 게임 시작 -> (아직 안 골랐으면) 시작 스킬 고르기 -> 서버 확정 -> 창고 유물 고르기 (한 번씩)
        void RefreshStartPicks(PlayerInfo me)
        {
            if (me == null || me.Run == null || !me.bHasBattler) return;

            if (!me.Run.bStartSkillChosen)
            {
                if (_skillPickOpened) return;
                _skillPickOpened = true;
                _skillPick = StartSkillPickWidget.OpenForLocalPlayer();
                return;
            }

            if (!me.Run.bStartRelicsChosen && !_relicPickOpened)
            {
                _relicPickOpened = true;
                _relicPick = StartRelicPickWidget.OpenForLocalPlayer();
            }
        }

        // ------------------------------------------------------------ 층 투표 (언리얼 PC::UpdateFloorVote)

        void RefreshFloorVote()
        {
            var vote = ClientState.Map.Vote;
            if (vote == null || vote.Phase == FloorVotePhase.None)
            {
                _floorVote?.RemoveFromHierarchy();
                _shownVoteKey = "";
                return;
            }

            if (_floorVote == null) _floorVote = new FloorVoteWidget();
            if (_floorVote.parent == null) UIRoot.Instance?.Show(UILayer.Overlay, _floorVote);

            // 바뀐 것만 다시 그림 (지도 스냅샷은 투표 말고도 자주 바뀜)
            string key = JsonUtility.ToJson(vote);
            if (key == _shownVoteKey) return;
            _shownVoteKey = key;
            _floorVote.Refresh(vote);
        }

        void OnFloorVoteRejected(FloorVoteRejectedMsg m)
        {
            _floorVote?.ClearMyChoice();
            ConfirmPopupWidget.Alert("투표", m.Message);
        }

        void OnRoomSelectFailed(RoomSelectFailedMsg m)
        {
            Debug.LogWarning("[Map] 선택 실패: " + m.Message);
            ConfirmPopupWidget.Alert("방을 고를 수 없습니다", m.Message);
        }

        // ------------------------------------------------------------ 층 도착 타이틀

        void ShowFloorTitle(FloorTitleMsg m)
        {
            // 같은 타이틀이 아직 떠 있으면 (씬 로드 직후 두 번 오는 경우) 다시 안 띄움
            if (_floorTitle != null && _floorTitle.panel != null && _floorTitleKey == m.Title + "|" + m.Subtitle) return;
            _floorTitleKey = m.Title + "|" + m.Subtitle;
            _floorTitle = FloorTitleWidget.Show(m.Title, m.Subtitle);
        }

        string _floorTitleKey = "";

        // ------------------------------------------------------------ 이공간

        void OnEnterRift(EnterRiftMsg m)
        {
            // 이공간 화면이 곧 안내 -> 로딩 화면은 걷음
            LoadingScreenWidget.Hide();
            if (_rift == null) _rift = new RiftWidget();
            UIRoot.Instance?.Show(UILayer.Overlay, _rift);
            _rift.SetFromSave(m.bFromSave);
            _rift.SetWaiting(m.WaitingFor);
        }

        void OnLeaveRift()
        {
            _rift?.RemoveFromHierarchy();
        }

        // ------------------------------------------------------------ 방 상단바

        const int RestPanel = 0, EventPanel = 1, ShopPanel = 2;

        bool _panelSeenInArea;

        void SetRoomPanel(int kind, bool open)
        {
            if (open) _panelSeenInArea = ClientState.MyArea != null;
            if (kind == RestPanel) _restOpen = open;
            else if (kind == EventPanel) _eventOpen = open;
            else _shopOpen = open;
            RefreshRoomTopBar();
        }

        void RefreshRoomTopBar()
        {
            // 방에서 나왔으면 닫힘 (서버가 닫으라고 안 보낸 경로 대비)
            // 단, 패널 알림(Show*Msg)은 구역 스냅샷(0.1초 주기)보다 먼저 올 수 있음 -> 방 안에 있는 걸 한 번 본 뒤에 나간 경우만
            if (ClientState.MyArea != null) _panelSeenInArea = true;
            else if (_panelSeenInArea)
            {
                _restOpen = _eventOpen = _shopOpen = false;
                _panelSeenInArea = false;
            }

            bool wanted = _restOpen || _eventOpen || _shopOpen;
            if (wanted && _roomTopBar.parent == null) UIRoot.Instance?.Show(UILayer.RoomTopBar, _roomTopBar);
            else if (!wanted && _roomTopBar.parent != null) _roomTopBar.RemoveFromHierarchy();
        }
    }
}
