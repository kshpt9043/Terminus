// 메인 메뉴 (언리얼 UMainMenuWidget + WBP_MainMenu, 방 목록은 RoomListView = WBP_RoomList)
// UIApp 이 MainMenu 씬에서 UILayer.Map 에 전체 화면으로 띄움
//  - 던전 입장(싱글) / 이어하기(세이브가 있을 때만) / 주점 입장(방 목록) / 거점 / 창고 / 게임 종료
//  - 우상단 보유 골드, 끊겨서 돌아왔으면 이유 팝업 (WBP DisconnectPopup), 정산 대기가 있으면 정산부터
//  - 유니티 추가 (왼쪽 아래): 내 이름 칸, Steam / IP 모드 표시와 전환 (언리얼은 Steam 하나라 없던 것)
using Terminus.Game;
using Terminus.Game.Net;
using Terminus.Game.Net.Steam;
using Terminus.Game.Save;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class MainMenuView : VisualElement
    {
        enum PendingAction { None, Solo, Host, Join, Continue }

        readonly Label _goldText;
        readonly Button _dungeon;
        readonly Button _continue;
        readonly Button _tavern;
        readonly RoomListView _roomList;

        // 끊김 팝업 (WBP DisconnectPopup)
        readonly VisualElement _popup;
        readonly Label _popupTitle;
        readonly Label _popupBody;

        // 유니티 추가: 이름 / 네트워크 모드
        readonly TextField _name;
        readonly Label _modeText;
        readonly Button _modeToggle;

        BaseView _base;
        StorageView _storage;
        RunSaveListView _saveList;
        SettlementView _settlement;
        PendingAction _pending;

        public MainMenuView()
        {
            MenuStyle.Use(this, "Menu", "MenuWindows");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;

            // [2] 배경 / [3] 위 그림자 (T_UI_Fade 를 위아래 뒤집어 위 78px)
            var bg = MenuStyle.FullScreen(null, PickingMode.Ignore);
            MenuStyle.Background(bg, "/Game/KSH/Asset/T_UI_BG_Main");
            Add(bg);
            var topFade = new VisualElement { pickingMode = PickingMode.Ignore };
            topFade.style.position = Position.Absolute;
            topFade.style.left = 0;
            topFade.style.right = 0;
            topFade.style.top = 0;
            topFade.style.height = 78;
            topFade.style.scale = new Scale(new Vector2(1f, -1f));
            MenuStyle.Background(topFade, "/Game/KSH/Asset/T_UI_Fade");
            Add(topFade);

            // [4] 우상단 골드 (오른쪽 36, 위 39)
            var goldBox = UiStyle.Row();
            goldBox.pickingMode = PickingMode.Ignore;
            goldBox.style.position = Position.Absolute;
            goldBox.style.right = 36;
            goldBox.style.top = 39;
            var coin = new Image { image = UiTextures.Load(UiTextures.Coin), pickingMode = PickingMode.Ignore };
            coin.style.width = 33;
            coin.style.height = 33;
            goldBox.Add(coin);
            _goldText = MenuStyle.Text("", 19, UiColor.GoldHover, true);
            _goldText.style.marginLeft = 12;
            UiStyle.AddShadow(_goldText, 0.5f);
            goldBox.Add(_goldText);
            Add(goldBox);

            // [7] 제목 (가운데, 위 138) + 금 줄
            var titleBox = UiStyle.Column();
            titleBox.pickingMode = PickingMode.Ignore;
            titleBox.style.position = Position.Absolute;
            titleBox.style.left = 0;
            titleBox.style.right = 0;
            titleBox.style.top = 138;
            titleBox.style.alignItems = Align.Center;
            var title = MenuStyle.Text("TERMINUS", 74, UiColor.TextBody, true, TextAnchor.MiddleCenter);
            title.style.letterSpacing = 4.5f;   // LetterSpacing 60 (1/1000 em)
            UiStyle.AddShadow(title, 0.7f);
            titleBox.Add(title);
            var goldLine = new Image { image = UiTextures.Load(UiTextures.GoldLine), pickingMode = PickingMode.Ignore };
            goldLine.style.width = 256;
            goldLine.style.height = 4;
            titleBox.Add(goldLine);
            Add(titleBox);

            // [10] 메뉴 (가운데, 위 417, 폭 510)
            var menuBox = UiStyle.Column();
            menuBox.style.position = Position.Absolute;
            menuBox.style.top = 417;
            menuBox.style.left = new Length(50, LengthUnit.Percent);
            menuBox.style.width = 510;
            menuBox.style.marginLeft = -255;
            menuBox.style.alignItems = Align.Stretch;
            Add(menuBox);

            _dungeon = AddPlate(menuBox, "던전 입장", PlateKind.Red, OnDungeonClicked);
            _continue = AddPlate(menuBox, "이어하기", PlateKind.Amber, OnContinueClicked);
            _tavern = AddPlate(menuBox, "주점 입장", PlateKind.Teal, OnTavernClicked);
            AddTextButton(menuBox, "거점", OnBaseClicked, UiColor.TextBody);
            AddTextButton(menuBox, "창고", OnStorageClicked, UiColor.TextBody);
            AddTextButton(menuBox, "게임 종료", OnQuitClicked, null);

            // 유니티 추가: 왼쪽 아래 이름 / 네트워크 모드
            var netBox = UiStyle.Column();
            netBox.style.position = Position.Absolute;
            netBox.style.left = 36;
            netBox.style.bottom = 36;
            netBox.style.width = 320;
            Add(netBox);
            netBox.Add(MenuStyle.Text("이름", 14, UiColor.GoldHover, true));
            _name = MenuStyle.Input(MenuFlow.InitialName(), MenuFlow.DefaultName, MenuFlow.MaxNameLength);
            _name.style.marginTop = 6;
            netBox.Add(_name);
            _modeText = MenuStyle.Wrap("", 13, UiColor.TextAux, true);
            _modeText.style.marginTop = 10;
            netBox.Add(_modeText);
            _modeToggle = MenuStyle.TextButton("", OnModeToggle, 14, UiColor.TextSub);
            _modeToggle.style.alignSelf = Align.FlexStart;
            _modeToggle.style.marginTop = 4;
            _modeToggle.style.paddingLeft = 0;
            netBox.Add(_modeToggle);
            if (!string.IsNullOrEmpty(LocalProfile.ProfileKey))
            {
                var profile = MenuStyle.Text("프로필 " + LocalProfile.ProfileKey, 12, UiColor.TextFaint);
                profile.style.marginTop = 4;
                netBox.Add(profile);
            }

            // [1] 방 목록 (ZOrder 5: 메뉴 위, 팝업 아래)
            _roomList = new RoomListView();
            _roomList.BeforeConnect = hosting =>
            {
                ApplyName();
                _pending = hosting ? PendingAction.Host : PendingAction.Join;
            };
            Add(_roomList);

            // [29] 끊김 팝업 (ZOrder 10)
            _popup = MenuStyle.FullScreen(new Color(0f, 0f, 0f, 0.6f));
            _popup.style.alignItems = Align.Center;
            _popup.style.justifyContent = Justify.Center;
            _popup.style.display = DisplayStyle.None;
            var popupPanel = MenuStyle.RoundPanel(2f, 6f);
            popupPanel.style.width = 660;
            UiStyle.Padding(popupPanel, 42, 36);
            _popup.Add(popupPanel);
            _popupTitle = MenuStyle.Wrap("연결이 끊어졌습니다.", 22, UiColor.TextBody, true);
            UiStyle.AddShadow(_popupTitle, 0.6f);
            popupPanel.Add(_popupTitle);
            _popupBody = MenuStyle.Wrap("메인 화면으로 돌아왔습니다.", 13, UiColor.TextBody, true);
            _popupBody.style.marginTop = 15;
            popupPanel.Add(_popupBody);
            var popupOk = MenuStyle.PlateButton("확인", PlateKind.Red, HidePopup, 19);
            popupOk.style.width = 180;
            popupOk.style.height = 64;
            popupOk.style.marginTop = 27;
            popupOk.style.alignSelf = Align.FlexEnd;
            popupPanel.Add(popupOk);
            // ESC / Enter = 확인
            MenuStyle.OnEscape(_popup, HidePopup);
            _popup.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) HidePopup();
            });
            Add(_popup);

            RegisterCallback<AttachToPanelEvent>(_ => OnAttached());
            RegisterCallback<DetachFromPanelEvent>(_ => OnDetached());
        }

        static Button AddPlate(VisualElement box, string label, PlateKind kind, System.Action onClick)
        {
            var b = MenuStyle.PlateButton(label, kind, onClick, 27);
            b.style.height = 87;
            b.style.marginBottom = 12;
            box.Add(b);
            return b;
        }

        static Button AddTextButton(VisualElement box, string label, System.Action onClick, Color? color)
        {
            var b = MenuStyle.TextButton(label, onClick, 22, color);
            b.style.height = 66;
            box.Add(b);
            return b;
        }

        // ------------------------------------------------------------ 붙음 / 떨어짐 (언리얼 NativeConstruct / NativeDestruct)

        void OnAttached()
        {
            // 메뉴로 돌아왔으면 이어하기 대기는 버림 (안 그러면 다음 주점이 이어하기로 열림)
            ServerSession.ClearPendingContinue();
            _pending = PendingAction.None;
            SetMenuEnabled(true);

            // 메인 화면 준비 완료 -> 로딩 화면 걷기
            MenuFlow.HideLoading();

            var saves = MenuFlow.Saves;
            saves.RunSavesChanged -= RefreshContinueButton;
            saves.RunSavesChanged += RefreshContinueButton;
            RefreshContinueButton();

            var profile = LocalProfile.Service;
            profile.GoldChanged -= OnGoldChanged;
            profile.GoldChanged += OnGoldChanged;
            RefreshGold();
            RefreshMode();

            MenuFlow.FailureRaised -= OnFailure;
            MenuFlow.FailureRaised += OnFailure;

            // 세이브가 조작 / 손상돼서 복구했거나 새로 시작했으면 알림 (한 번만)
            var notice = profile.ConsumeLoadNotice();
            if (!string.IsNullOrEmpty(notice)) ShowPopup("세이브 파일 문제", notice);

            // 던전을 탈출하고 돌아왔으면 정산부터 (방장이 먼저 나가 끊긴 것도 여기로 -> 끊김 안내는 안 띄움)
            if (profile.HasPendingSettlement)
            {
                MenuFlow.ConsumeFailure();
                ShowSettlement();
                return;
            }

            // 끊겨서 돌아온 거면 이유를 보여준다. 꺼내면 비워지므로 한 번만 뜬다
            var reason = MenuFlow.ConsumeFailure();
            if (!string.IsNullOrEmpty(reason))
            {
                ShowPopup(reason, "메인 화면으로 돌아왔습니다. 진행 중이던 던전은 주점 목록의 '재합류 대기' 방으로 돌아갈 수 있습니다.");
            }
        }

        void OnDetached()
        {
            MenuFlow.FailureRaised -= OnFailure;
            MenuFlow.Saves.RunSavesChanged -= RefreshContinueButton;
            LocalProfile.Service.GoldChanged -= OnGoldChanged;

            // 메뉴 창들도 같이 내림 (주점으로 넘어감)
            _base?.Close();
            _storage?.Close();
            _saveList?.Close();
            _settlement?.RemoveFromHierarchy();
        }

        // ------------------------------------------------------------ 버튼

        void OnDungeonClicked()
        {
            // 싱글: 혼자 방장으로 주점(캐릭터 선택)을 엶. 던전 입장은 늘 새 게임
            // 싱글 세이브는 하나뿐이라, 진행 중인 게 있으면 지워도 되는지 먼저 물음 (이어하기는 '이어하기' 목록에서)
            ServerSession.ClearPendingContinue();
            var single = MenuFlow.Saves.FindSingleRunSave();
            if (single != null)
            {
                ConfirmPopupWidget.Show("새 던전",
                    $"진행 중인 싱글 던전이 있습니다. ({RunSaveService.DescribeTitle(single)})\n새로 시작하면 기존 진행 상황이 삭제됩니다.\n이어서 하려면 '이어하기'에서 고르세요.",
                    () =>
                    {
                        MenuFlow.Saves.DeleteSingleRunSaves();
                        StartNewSolo();
                    }, "삭제하고 시작", "취소");
                return;
            }
            StartNewSolo();
        }

        void StartNewSolo()
        {
            ApplyName();
            _pending = PendingAction.Solo;
            SetMenuEnabled(false);
            MenuFlow.ShowLoading("캐릭터 선택으로 가는 중...");
            if (!NetworkRoot.StartSingle())
            {
                SetMenuEnabled(true);
                MenuFlow.HideLoading();
            }
        }

        void OnTavernClicked()
        {
            ServerSession.ClearPendingContinue();
            _roomList.Open();
        }

        void OnContinueClicked()
        {
            if (_saveList == null)
            {
                _saveList = new RunSaveListView();
                _saveList.BeforeContinue = () =>
                {
                    ApplyName();
                    _pending = PendingAction.Continue;
                    SetMenuEnabled(false);
                };
                _saveList.ContinueFailed = () =>
                {
                    _pending = PendingAction.None;
                    SetMenuEnabled(true);
                };
            }
            _saveList.Open();
        }

        void OnBaseClicked()
        {
            if (_base == null) _base = new BaseView();
            _base.Open();
        }

        void OnStorageClicked()
        {
            if (_storage == null) _storage = new StorageView();
            _storage.Open();
        }

        static void OnQuitClicked()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }

        void ApplyName()
        {
            MenuFlow.ApplyName(_name.value);
            _name.SetValueWithoutNotify(NetworkRoot.LocalPlayerName);
        }

        void SetMenuEnabled(bool enabled)
        {
            _dungeon.SetEnabled(enabled);
            _tavern.SetEnabled(enabled);
            _continue.SetEnabled(enabled);
        }

        // ------------------------------------------------------------ 이어하기 / 골드

        void RefreshContinueButton()
        {
            // 이어하기 버튼: 세이브가 있을 때만. 마지막 세이브를 지웠으면 열려 있던 목록도 닫음
            bool hasSaves = MenuFlow.Saves.HasRunSaves();
            UiStyle.SetShown(_continue, hasSaves);
            if (!hasSaves) _saveList?.Close();
        }

        void OnGoldChanged(int gold, int delta) => RefreshGold();

        void RefreshGold()
        {
            UiStyle.SetTextIfChanged(_goldText, UiText.GoldText(LocalProfile.Service.Gold));
        }

        // ------------------------------------------------------------ 정산

        void ShowSettlement()
        {
            if (_settlement == null || _settlement.IsFinished) _settlement = new SettlementView();
            UIRoot.Instance?.Show(UILayer.Settlement, _settlement);
        }

        // ------------------------------------------------------------ 실패 / 팝업

        // 메인 메뉴가 떠 있는 동안 온 실패 (방 열기 / 참가 / 혼자 시작 / 이어하기)
        void OnFailure(string reason)
        {
            MenuFlow.ConsumeFailure();
            var action = _pending;
            _pending = PendingAction.None;
            SetMenuEnabled(true);
            _saveList?.ResetLoading();

            // 방 목록이 열려 있고 참가 중이었으면 목록이 상태 줄에 사유를 띄운다
            if (_roomList.IsOpen && _roomList.IsJoining)
            {
                _roomList.JoinFailed(reason);
                return;
            }
            _roomList.HostFailed();

            switch (action)
            {
                case PendingAction.Join:
                    ShowPopup("주점에 들어가지 못했습니다.", string.IsNullOrEmpty(reason) ? "잠시 후 다시 시도하세요." : reason);
                    break;
                case PendingAction.Solo:
                case PendingAction.Host:
                case PendingAction.Continue:
                    ShowPopup("주점을 열지 못했습니다.", string.IsNullOrEmpty(reason) ? "Steam이 켜져 있는지 확인한 뒤 다시 시도하세요." : reason);
                    break;
                default:
                    ShowPopup(string.IsNullOrEmpty(reason) ? "연결이 끊어졌습니다." : reason, "메인 화면으로 돌아왔습니다.");
                    break;
            }
        }

        void ShowPopup(string title, string body)
        {
            UiStyle.SetLine(_popupTitle, title);
            _popupBody.text = body ?? "";
            _popup.style.display = DisplayStyle.Flex;
            _popup.BringToFront();
            _popup.schedule.Execute(() => _popup.Focus());
        }

        void HidePopup()
        {
            _popup.style.display = DisplayStyle.None;
        }

        // ------------------------------------------------------------ 유니티 추가: Steam / IP 모드

        void RefreshMode()
        {
            bool steam = NetworkRoot.Mode == TransportMode.Steam && SteamBootstrap.Available;
            string text = steam ? "Steam 모드 · 방 목록 / 친구 초대" : "IP 모드 · 방장 IP 로 참가";
            if (!steam && !string.IsNullOrEmpty(SteamBootstrap.LastError)) text += "\n" + SteamBootstrap.LastError;
            _modeText.text = text;
            _modeText.style.color = steam ? UiColor.Ready : UiColor.TextAux;
            _modeToggle.text = steam ? "IP 모드로 전환" : "Steam 모드로 전환";
        }

        void OnModeToggle()
        {
            if (NetworkRoot.IsBusy) return;
            bool steam = NetworkRoot.Mode == TransportMode.Steam && SteamBootstrap.Available;
            if (steam)
            {
                SteamBootstrap.UseIpMode();
#if UNITY_EDITOR
                SteamBootstrap.PlayerPrefsWantSteamInEditor = false;
#endif
            }
            else
            {
                SteamBootstrap.UseSteamMode();
#if UNITY_EDITOR
                SteamBootstrap.PlayerPrefsWantSteamInEditor = SteamBootstrap.Available;
#endif
                // Steam 이 켜졌고 이름 칸이 비었거나 기본값이면 Steam 이름으로 (Steam 이 비어 있던 LocalPlayerName 을 채움)
                var typed = (_name.value ?? "").Trim();
                if (SteamBootstrap.Available && !string.IsNullOrWhiteSpace(NetworkRoot.LocalPlayerName)
                    && (typed.Length == 0 || typed == MenuFlow.DefaultName))
                {
                    _name.value = NetworkRoot.LocalPlayerName;
                }
            }
            RefreshMode();
            if (_roomList.IsOpen) _roomList.Refresh();
        }
    }
}
