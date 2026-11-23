// 주점 (언리얼 UTavernWidget + WBP_TavernWidget, 직업 버튼 WBP_ClassButton, 플레이어 칸 WBP_PlayerSlot)
// UIApp 이 Tavern 씬에서 UILayer.Map 에 띄움
//  - 직업 고르기 (SelectCharacterReq) + 고른 직업 정보 (DT_CharacterClass: 이름 / 세력 / 패시브 / 기본 스킬 / 설명 / 일러스트)
//  - 준비 (SetReadyReq) / 출발 (방장, StartGameReq) / 친구 초대 (RoomDirectory.InviteFriends) / 나가기
//  - 하드 모드 (방장, SetHardModeReq): 언리얼 C++ 에는 있지만 WBP 에 체크 칸이 없어서 출발 버튼 위에 둠 (유니티 추가 배치)
//  - 싱글이면 '캐릭터 선택' (칸 / 초대 / 준비 숨김, 출발만)
//  - 내 플레이어가 생기면 이름과 거점 강화를 서버에 한 번 알림 (SetNameReq / ReportUpgradesReq)
// 폴링 0.5초 + 파티 스냅샷 변화 (언리얼 RefreshSlots 타이머)
using System.Collections.Generic;
using Terminus.Art;
using Terminus.Data;
using Terminus.Game;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.UI.Common;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class TavernView : VisualElement
    {
        const int MaxSlots = 4;

        readonly Label _title;
        readonly Button _invite;
        readonly VisualElement _slotPanel;
        readonly List<PlayerSlotView> _slots = new List<PlayerSlotView>();
        readonly Image _illustration;
        readonly Label _classTitle;
        readonly Label _faction;
        readonly Label _passive;
        readonly Label _skills;
        readonly Label _description;
        readonly VisualElement _classButtonBox;
        readonly List<ClassButtonView> _classButtons = new List<ClassButtonView>();
        readonly Button _leave;
        readonly Button _ready;
        readonly Button _start;
        readonly Label _wait;
        readonly Toggle _hardMode;

        CharacterClass _selected = CharacterClass.Fighter;
        bool _syncedSelection;
        float _localChoiceTime = -10f;
        bool _sentHello;
        bool _loadingHidden;
        int _lastReadyShown = -1;
        IVisualElementScheduledItem _illustAnim;
        SpriteFlipbook _illustFlipbook;
        int _illustFrame;

        public TavernView()
        {
            MenuStyle.Use(this, "Menu", "Tavern");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;

            // [1] 배경 / [2] 위 그림자 (뒤집은 T_UI_Fade, 126px)
            var bg = MenuStyle.FullScreen(null, PickingMode.Ignore);
            MenuStyle.Background(bg, "/Game/KSH/Asset/T_UI_BG_Tavern");
            Add(bg);
            var topFade = Abs(new VisualElement { pickingMode = PickingMode.Ignore });
            topFade.style.left = 0;
            topFade.style.right = 0;
            topFade.style.top = 0;
            topFade.style.height = 126;
            topFade.style.scale = new Scale(new Vector2(1f, -1f));
            MenuStyle.Background(topFade, "/Game/KSH/Asset/T_UI_Fade");
            Add(topFade);

            // [3] 제목 (왼쪽 60, 세로 가운데 63)
            _title = MenuStyle.Text("주점", 22, UiColor.TextBody, true);
            Abs(_title);
            _title.style.left = 60;
            _title.style.top = 63;
            _title.style.translate = new Translate(0, Length.Percent(-50));
            _title.style.textShadow = new TextShadow { offset = new Vector2(0f, 3f), color = new Color(0f, 0f, 0f, 0.6f) };
            Add(_title);

            // [4] 오른쪽 위: 친구 초대 + 플레이어 칸 4개 (오른쪽 60, 세로 가운데 63)
            var topRight = Abs(UiStyle.Row());
            topRight.style.right = 60;
            topRight.style.top = 63;
            topRight.style.translate = new Translate(0, Length.Percent(-50));
            Add(topRight);
            _invite = new Button(() => RoomDirectory.InviteFriends()) { text = "+ 친구 초대" };
            _invite.AddToClassList("tm-invite");
            _invite.style.width = 180;
            _invite.style.height = 69;
            _invite.style.marginRight = 15;
            topRight.Add(_invite);
            _slotPanel = UiStyle.Row();
            topRight.Add(_slotPanel);
            for (int i = 0; i < MaxSlots; ++i)
            {
                var slot = new PlayerSlotView();
                _slots.Add(slot);
                _slotPanel.Add(slot);
            }

            // [9] 일러스트 자리 (왼쪽 120, 위 165, 720 x 705): 바닥 그림자 + 일러스트 (아래 가운데)
            var illustArea = Abs(new VisualElement { pickingMode = PickingMode.Ignore });
            illustArea.style.left = 120;
            illustArea.style.top = 165;
            illustArea.style.width = 720;
            illustArea.style.height = 705;
            Add(illustArea);
            var floorShadow = Abs(new Image { image = UiTextures.LoadUnreal("/Game/KSH/Asset/T_UI_FloorShadow"), pickingMode = PickingMode.Ignore });
            floorShadow.style.width = 540;
            floorShadow.style.height = 54;
            floorShadow.style.left = 90;
            floorShadow.style.bottom = 0;
            illustArea.Add(floorShadow);
            _illustration = Abs(new Image { scaleMode = ScaleMode.ScaleToFit, pickingMode = PickingMode.Ignore });
            _illustration.style.left = 0;
            _illustration.style.right = 0;
            _illustration.style.top = 0;
            _illustration.style.bottom = 12;
            illustArea.Add(_illustration);

            // [13] 정보 (왼쪽 960, 위 192, 864 x 700)
            var info = Abs(UiStyle.Column());
            info.style.left = 960;
            info.style.top = 192;
            info.style.width = 864;
            info.style.height = 700;
            Add(info);
            _classTitle = MenuStyle.Text("", 54, UiColor.TextBody, true);
            _classTitle.style.textShadow = new TextShadow { offset = new Vector2(0f, 4f), color = new Color(0f, 0f, 0f, 0.6f) };
            info.Add(_classTitle);
            _faction = MenuStyle.Text("", 15, UiColor.Gold, true);
            UiStyle.Padding(_faction, 6, 6);
            info.Add(_faction);
            var passiveFrame = new VisualElement();
            passiveFrame.AddToClassList("tm-passive");
            passiveFrame.style.marginTop = 27;
            passiveFrame.style.marginBottom = 21;
            info.Add(passiveFrame);
            passiveFrame.Add(MenuStyle.Text("패시브", 15, UiColor.GoldHover, true));
            _passive = MenuStyle.Wrap("", 19, UiColor.TextBody, true);
            _passive.style.marginTop = 6;
            passiveFrame.Add(_passive);
            _skills = MenuStyle.Wrap("", 14, UiColor.TextBody, true);
            _skills.style.marginTop = 24;
            info.Add(_skills);
            _description = MenuStyle.Wrap("", 15, UiColor.TextSub, true);
            _description.style.marginTop = 24;
            info.Add(_description);

            // [22] 아래 그림자 (180px)
            var bottomFade = Abs(new VisualElement { pickingMode = PickingMode.Ignore });
            bottomFade.style.left = 0;
            bottomFade.style.right = 0;
            bottomFade.style.bottom = 0;
            bottomFade.style.height = 180;
            MenuStyle.Background(bottomFade, "/Game/KSH/Asset/T_UI_Fade");
            Add(bottomFade);

            // [23] 나가기 (왼쪽 60, 아래에서 90 가운데)
            _leave = MenuStyle.TextButton("나가기", OnLeave, 19, UiColor.TextAux);
            Abs(_leave);
            _leave.style.left = 60;
            _leave.style.bottom = 90;
            _leave.style.height = 50;
            _leave.style.translate = new Translate(0, Length.Percent(50));
            Add(_leave);

            // [26] 직업 버튼 (아래 가운데, 24 위)
            _classButtonBox = Abs(UiStyle.Row());
            _classButtonBox.style.left = 0;
            _classButtonBox.style.right = 0;
            _classButtonBox.style.bottom = 24;
            _classButtonBox.style.justifyContent = Justify.Center;
            _classButtonBox.style.alignItems = Align.FlexEnd;
            Add(_classButtonBox);
            for (int i = 0; i < CharacterNames.ClassCount; ++i)
            {
                var c = (CharacterClass)i;
                var row = GameData.FindCharacterClassRow(c);
                if (row == null) continue;   // 행이 없으면 버튼을 건너뜀
                var button = new ClassButtonView(c, row.DisplayName);
                button.Chosen += OnClassChosen;
                _classButtons.Add(button);
                _classButtonBox.Add(button);
            }

            // [27] 오른쪽 아래: 준비 / 출발 (오른쪽 60, 아래에서 90 가운데)
            var actionBox = Abs(UiStyle.Row());
            actionBox.style.right = 60;
            actionBox.style.bottom = 90;
            actionBox.style.translate = new Translate(0, Length.Percent(50));
            Add(actionBox);
            _ready = MenuStyle.PlateButton("준비 완료", PlateKind.Red, OnReadyClicked, 19);
            _ready.style.width = 255;
            _ready.style.height = 84;
            _ready.style.marginRight = 21;
            actionBox.Add(_ready);
            _start = MenuStyle.PlateButton("출발", PlateKind.Teal, OnStartClicked, 19);
            _start.style.width = 255;
            _start.style.height = 84;
            actionBox.Add(_start);

            // [34] 기다림 안내 (오른쪽 60, 아래 150 위)
            _wait = MenuStyle.Text("", 16, UiColor.TextAux, true, TextAnchor.MiddleRight);
            Abs(_wait);
            _wait.style.right = 60;
            _wait.style.bottom = 150;
            _wait.style.display = DisplayStyle.None;
            Add(_wait);

            // 하드 모드 (유니티 추가 배치: 출발 버튼 위)
            _hardMode = MenuStyle.Check("하드 모드 (죽으면 들고 간 창고 유물이 창고에서 사라짐)", false, 15, UiColor.TextSub);
            Abs(_hardMode);
            _hardMode.style.right = 60;
            _hardMode.style.bottom = 190;
            _hardMode.RegisterValueChangedCallback(OnHardModeChanged);
            Add(_hardMode);

            ApplySelection(_selected);
            RegisterCallback<AttachToPanelEvent>(_ => OnAttached());
            RegisterCallback<DetachFromPanelEvent>(_ => OnDetached());
        }

        static T Abs<T>(T e) where T : VisualElement
        {
            e.style.position = Position.Absolute;
            return e;
        }

        static bool IsSolo => NetworkRoot.IsSinglePlayer;

        // ------------------------------------------------------------ 붙음 / 떨어짐

        void OnAttached()
        {
            ClientState.PartyChanged -= Refresh;
            ClientState.PartyChanged += Refresh;
            HookSceneEvents(true);

            // 내 플레이어가 생기면 이름과 거점 강화를 서버에 알림 (언리얼 Server_ReportUpgrades)
            schedule.Execute(SayHello).Every(300).Until(() => _sentHello || panel == null);

            if (IsSolo)
            {
                ApplySoloLayout();
                HideLoadingScreen();
            }
            // 폴링 (첫 호출은 바로)
            Refresh();
            schedule.Execute(Refresh).Every(500).Until(() => panel == null);
        }

        void OnDetached()
        {
            ClientState.PartyChanged -= Refresh;
            HookSceneEvents(false);
            _illustAnim?.Pause();
        }

        void SayHello()
        {
            if (_sentHello || TerminusPlayer.Local == null) return;
            _sentHello = true;
            NetBus.Request(new SetNameReq { Name = NetworkRoot.LocalPlayerName });
            NetBus.Request(new ReportUpgradesReq { Upgrades = LocalProfile.Service.GetAllUpgrades() });
        }

        // 던전으로 넘어가기 시작하면 로딩 화면 (언리얼: 방장 TryStartGame / 손님 PreClientTravel)
        void HookSceneEvents(bool hook)
        {
            var nm = NetworkManager.Singleton;
            if (nm == null || nm.SceneManager == null) return;
            nm.SceneManager.OnSceneEvent -= OnSceneEvent;
            if (hook) nm.SceneManager.OnSceneEvent += OnSceneEvent;
        }

        void OnSceneEvent(SceneEvent e)
        {
            if (e.SceneEventType == SceneEventType.Load && e.SceneName == SceneNames.Dungeon) MenuFlow.ShowDungeonTravel();
        }

        // ------------------------------------------------------------ 직업

        void OnClassChosen(CharacterClass c)
        {
            if (c == _selected) return;
            // 준비 완료 상태면 못 바꿈 (서버는 거절하는데 내 정보 패널만 바뀐 채로 남지 않게)
            var me = ClientState.Me;
            if (me != null && me.bReady) return;
            // 내 화면은 바로 바꾸고, 서버를 거쳐서 실제 선택
            ApplySelection(c);
            _localChoiceTime = Time.realtimeSinceStartup;
            NetBus.Request(new SelectCharacterReq { Class = c });
        }

        void ApplySelection(CharacterClass c)
        {
            _selected = c;
            foreach (var b in _classButtons) b.SetSelected(b.Class == c);

            var row = GameData.FindCharacterClassRow(c);
            if (row == null)
            {
                Debug.LogWarning($"[UI] 주점: {c} 직업 행이 없음 (DT_CharacterClass)");
                return;
            }
            _classTitle.text = row.DisplayName;
            _description.text = UiText.FixNewlines(row.Description);
            _passive.text = UiText.FixNewlines(row.PassiveText);
            _faction.text = CharacterNames.GetFactionName(row.Faction);

            // "이름 · 설명" 한 줄씩 (스킬 데이터의 Description_KR 그대로)
            var lines = new List<string>();
            foreach (var skill in GameData.FindBasicSkills(c)) lines.Add($"{skill.DisplayName_KR} · {UiText.FixNewlines(skill.Description_KR)}");
            _skills.text = string.Join("\n", lines);

            SetIllustration(row);
        }

        // 일러스트: DT 의 Illustration 텍스처 (/Game/...). 아직 정식 그림이 없으면(언리얼 AICON 임시값)
        // 직업 대기 플립북(Visuals/Characters/<직업>)으로 대신. 둘 다 없으면 자리만 남김 (언리얼 Hidden)
        void SetIllustration(CharacterClassRow row)
        {
            _illustAnim?.Pause();
            _illustFlipbook = null;

            Texture2D tex = null;
            if (!string.IsNullOrEmpty(row.Illustration) && row.Illustration.Contains("/Game/")) tex = UiTextures.LoadUnreal(row.Illustration);
            if (tex != null)
            {
                _illustration.sprite = null;
                _illustration.image = tex;
                _illustration.style.visibility = Visibility.Visible;
                return;
            }

            var visual = Resources.Load<MonsterVisualSet>("Visuals/Characters/" + row.Class);
            var flipbook = visual != null ? visual.Idle : null;
            if (flipbook == null || flipbook.FrameCount == 0)
            {
                _illustration.style.visibility = Visibility.Hidden;
                return;
            }

            _illustFlipbook = flipbook;
            _illustFrame = 0;
            _illustration.image = null;
            _illustration.sprite = flipbook.GetFrame(0);
            _illustration.style.visibility = Visibility.Visible;
            if (flipbook.FrameCount > 1 && flipbook.FramesPerSecond > 0f)
            {
                long ms = (long)(1000f / flipbook.FramesPerSecond);
                if (_illustAnim == null) _illustAnim = schedule.Execute(NextIllustFrame).Every(ms);
                else
                {
                    _illustAnim.Every(ms);
                    _illustAnim.Resume();
                }
            }
        }

        void NextIllustFrame()
        {
            if (_illustFlipbook == null || _illustFlipbook.FrameCount == 0) return;
            _illustFrame = (_illustFrame + 1) % _illustFlipbook.FrameCount;
            _illustration.sprite = _illustFlipbook.GetFrame(_illustFrame);
        }

        // ------------------------------------------------------------ 버튼

        void OnReadyClicked()
        {
            // 내 준비의 반대값을 서버로
            var me = ClientState.Me;
            NetBus.Request(new SetReadyReq { bReady = me == null || !me.bReady });
        }

        void OnStartClicked()
        {
            // 폴링 간격 동안 준비를 풀었을 수도 있으므로 검사 (싱글은 검사 안 함)
            if (!IsSolo && !AreAllPlayersReady()) return;
            // 싱글은 준비 버튼이 없음 -> 서버(전원 준비 검사)에 준비를 같이 보냄 (언리얼은 Standalone 이면 검사를 건너뜀)
            if (IsSolo) NetBus.Request(new SetReadyReq { bReady = true });
            NetBus.Request(new StartGameReq());
        }

        void OnLeave()
        {
            // 연타 막기
            _leave.SetEnabled(false);
            MenuFlow.LeaveToMenu();
        }

        void OnHardModeChanged(ChangeEvent<bool> e)
        {
            if (!ClientState.IAmHost) return;   // 방장만
            if (e.newValue != ClientState.Party.bHardMode) NetBus.Request(new SetHardModeReq { bHardMode = e.newValue });
        }

        static bool AreAllPlayersReady()
        {
            var players = ClientState.Party.Players;
            if (players.Count == 0) return false;
            foreach (var p in players)
            {
                if (p.bConnected && !p.bReady) return false;
            }
            return true;
        }

        // ------------------------------------------------------------ 갱신 (언리얼 RefreshSlots)

        void Refresh()
        {
            if (panel == null) return;
            var party = ClientState.Party;
            var me = ClientState.Me;

            if (!IsSolo)
            {
                for (int i = 0; i < _slots.Count; ++i)
                {
                    var p = i < party.Players.Count ? party.Players[i] : null;
                    if (p == null)
                    {
                        _slots[i].SetEmpty();
                        continue;
                    }
                    var row = GameData.FindCharacterClassRow(p.Class);
                    _slots[i].Setup(p.Name, row != null ? row.DisplayName : CharacterNames.GetClassName(p.Class), p.bReady, p.Id == ClientState.MyId);
                }
            }

            // 화면 선택은 서버 값을 따름 (처음 들어올 때 / 서버가 거절했을 때 / 이어하기로 직업이 고정됐을 때).
            // 방금 내가 누른 건 서버 답이 올 때까지(1초) 화면 값을 유지
            if (me != null && me.Class != _selected && (!_syncedSelection || Time.realtimeSinceStartup - _localChoiceTime > 1f))
            {
                ApplySelection(me.Class);
            }
            if (me != null) _syncedSelection = true;

            // 내 준비 상태 -> 버튼 글 / 판 색 / 직업 버튼 잠금
            bool myReady = me != null && me.bReady;
            UiStyle.SetTextIfChanged(_ready, myReady ? "준비 취소" : "준비 완료");
            // 판 색은 준비 상태가 바뀐 순간에만 (매번 넣으면 호버 중에 깜빡일 수 있음)
            if (_lastReadyShown != (myReady ? 1 : 0))
            {
                MenuStyle.SetPlateKind(_ready, myReady ? PlateKind.Gray : PlateKind.Red);
                _lastReadyShown = myReady ? 1 : 0;
            }
            foreach (var b in _classButtons) b.SetEnabled(!myReady);

            bool isHost = NetBus.IsServer;
            if (IsSolo)
            {
                UiStyle.SetShown(_start, true);
                _start.SetEnabled(true);
            }
            else
            {
                UiStyle.SetShown(_start, isHost);
                _start.SetEnabled(AreAllPlayersReady());
                UiStyle.SetShown(_wait, !isHost);
                UiStyle.SetTextIfChanged(_wait, myReady ? "방장의 출발을 기다리는 중" : "준비를 누르면 방장이 출발할 수 있습니다");
                UiStyle.SetShown(_invite, RoomDirectory.SupportsInvites);
            }

            RefreshHardMode(isHost);

            // 슬롯이 처음 채워지면 로딩 화면 걷기
            if (me != null) HideLoadingScreen();
        }

        void RefreshHardMode(bool isHost)
        {
            // 방장은 리슨 서버라 누르는 즉시 서버 값이 바뀜 -> 모두 스냅샷 값을 그대로 보여주면 됨
            _hardMode.SetValueWithoutNotify(ClientState.Party.bHardMode);
            // 이어하기는 세이브 당시 설정 그대로라 방장도 못 바꿈
            _hardMode.SetEnabled(isHost && !IsContinuing());
        }

        static bool IsContinuing() => NetBus.IsServer && ServerSession.Instance != null && ServerSession.Instance.IsContinuingRun;

        void HideLoadingScreen()
        {
            if (_loadingHidden) return;
            // 이어하기는 주점을 거쳐 바로 던전으로 감 -> 계속 덮어 둠
            if (IsContinuing()) return;
            _loadingHidden = true;
            MenuFlow.HideLoading();
        }

        void ApplySoloLayout()
        {
            // 기획서: 싱글은 던전 입장 -> 캐릭터 선택 화면
            UiStyle.SetShown(_slotPanel, false);
            UiStyle.SetShown(_invite, false);
            UiStyle.SetShown(_ready, false);
            UiStyle.SetShown(_wait, false);
            _title.text = "캐릭터 선택";
            UiStyle.SetShown(_start, true);
            _start.SetEnabled(true);
        }
    }

    /// <summary>직업 버튼 (언리얼 UClassButton + WBP_ClassButton): 96 칸 + 선택 빛 + 이름</summary>
    public sealed class ClassButtonView : VisualElement
    {
        readonly VisualElement _glow;
        readonly Label _label;
        readonly Button _click;

        public CharacterClass Class { get; }

        public event System.Action<CharacterClass> Chosen;

        public ClassButtonView(CharacterClass c, string label)
        {
            Class = c;
            MenuStyle.Use(this, "Menu", "Tavern");
            style.width = 132;
            style.alignItems = Align.Center;

            var box = new VisualElement();
            box.style.width = 128;
            box.style.height = 128;
            box.style.alignItems = Align.Center;
            box.style.justifyContent = Justify.Center;
            Add(box);
            _click = new Button(() => Chosen?.Invoke(Class));
            _click.AddToClassList("tm-classbtn");
            box.Add(_click);
            _glow = new VisualElement { pickingMode = PickingMode.Ignore };
            _glow.AddToClassList("tm-classglow");
            _glow.style.display = DisplayStyle.None;
            box.Add(_glow);

            _label = MenuStyle.Text(label, 19, UiColor.TextAux, true, TextAnchor.MiddleCenter);
            _label.style.paddingLeft = 4;
            _label.style.paddingRight = 4;
            _label.style.paddingBottom = 2;
            Add(_label);
            SetSelected(false);
        }

        public void SetSelected(bool selected)
        {
            _glow.style.display = selected ? DisplayStyle.Flex : DisplayStyle.None;
            // 1d 팔레트: 선택되면 금색, 아니면 보조 글자색
            _label.style.color = selected ? UiColor.GoldHover : UiColor.TextAux;
        }
    }

    /// <summary>플레이어 칸 (언리얼 UPlayerSlot + WBP_PlayerSlot 317x82): 이름(나) / 직업 / 준비, 빈 자리, 내 자리 금 테두리</summary>
    public sealed class PlayerSlotView : VisualElement
    {
        readonly VisualElement _frame;
        readonly Label _name;
        readonly Label _class;
        readonly Label _readyMark;
        readonly VisualElement _mine;
        readonly VisualElement _empty;

        public PlayerSlotView()
        {
            MenuStyle.Use(this, "Menu", "Tavern");
            style.width = 317;
            style.height = 82;
            pickingMode = PickingMode.Ignore;

            var overlay = new VisualElement { pickingMode = PickingMode.Ignore };
            overlay.style.position = Position.Absolute;
            overlay.style.left = 15;
            overlay.style.right = 0;
            overlay.style.top = 0;
            overlay.style.bottom = 0;
            Add(overlay);

            _frame = new VisualElement { pickingMode = PickingMode.Ignore };
            _frame.AddToClassList("tm-slot-frame");
            overlay.Add(_frame);
            _name = MenuStyle.Text("", 15, UiColor.TextBody, true, TextAnchor.MiddleCenter);
            _name.style.overflow = Overflow.Hidden;
            _name.style.textOverflow = TextOverflow.Ellipsis;
            _name.style.maxWidth = 240;
            _frame.Add(_name);
            _class = MenuStyle.Text("", 11, UiColor.TextAux, true, TextAnchor.MiddleCenter);
            _frame.Add(_class);
            _readyMark = MenuStyle.Text("준비", 14, UiColor.Ready, true, TextAnchor.MiddleCenter);
            _frame.Add(_readyMark);

            _mine = new VisualElement { pickingMode = PickingMode.Ignore };
            _mine.AddToClassList("tm-slot-mine");
            overlay.Add(_mine);

            _empty = new VisualElement { pickingMode = PickingMode.Ignore };
            _empty.AddToClassList("tm-slot-empty");
            _empty.Add(MenuStyle.Text("빈 자리", 14, UiColor.TextFaint, true, TextAnchor.MiddleCenter));
            overlay.Add(_empty);

            SetEmpty();
        }

        public void Setup(string playerName, string className, bool ready, bool mine)
        {
            // 닉네임이 겹칠 수 있으니 본인 자리는 표시해둔다
            UiStyle.SetTextIfChanged(_name, mine ? playerName + " (나)" : playerName);
            UiStyle.SetTextIfChanged(_class, className);
            UiStyle.SetShown(_readyMark, ready);
            UiStyle.SetShown(_empty, false);
            UiStyle.SetShown(_mine, mine);
        }

        public void SetEmpty()
        {
            UiStyle.SetTextIfChanged(_name, "");
            UiStyle.SetTextIfChanged(_class, "");
            UiStyle.SetShown(_readyMark, false);
            UiStyle.SetShown(_empty, true);
            UiStyle.SetShown(_mine, false);
        }
    }
}
