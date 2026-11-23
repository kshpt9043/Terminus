// 방(주점) 목록 (언리얼 URoomListWidget + WBP_RoomList, 한 줄은 URoomEntryWidget + WBP_RoomEntry)
// 메인 메뉴 안에 겹쳐 둔 창 (WBP_MainMenu 의 RoomBrowser, ZOrder 5). 열면 바로 한 번 찾음
//  - 목록: RoomDirectory.Refresh / Join. 잠긴 방은 비밀번호부터
//  - 주점 열기: 이름(기본 랜덤) / 비밀번호(선택, 영문·숫자 1~16) / 목록에 안 띄우기(초대 전용)
//  - 유니티 추가: 방 목록이 없는 모드(IP, RoomDirectory.SupportsBrowsing == false)면 목록 대신 방장 IP 로 참가하는 칸
// ESC: 안쪽 창(비밀번호 / 주점 열기)부터 닫고, 없으면 목록을 닫음
using System;
using System.Collections.Generic;
using Terminus.Game.Net;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class RoomListView : VisualElement
    {
        // WBP 색 (선형 -> sRGB)
        static readonly Color DimColor = new Color(0f, 0f, 0f, 0.36f);   // Brush A 0.6 x ColorAndOpacity A 0.6
        static readonly Color InnerDimColor = new Color(0f, 0f, 0f, 0.6f);

        readonly Label _status;
        readonly ScrollView _scroll;
        readonly VisualElement _list;
        readonly Button _refresh;
        readonly Button _openCreate;
        readonly Button _close;

        // IP 모드 칸 (유니티 추가)
        readonly VisualElement _ipBox;
        readonly TextField _address;
        readonly TextField _ipPassword;
        readonly Button _ipJoin;

        // 주점 열기 창
        readonly VisualElement _createPanel;
        readonly TextField _roomName;
        readonly TextField _password;
        readonly Toggle _private;
        readonly Label _createError;
        readonly Button _createConfirm;

        // 비밀번호 창
        readonly VisualElement _passwordPanel;
        readonly TextField _joinPassword;
        readonly Label _joinError;

        RoomListing _pendingJoin;
        bool _busy;

        /// <summary>방을 열거나(true) 들어가기(false) 직전 (메인 메뉴가 이름을 정함)</summary>
        public Action<bool> BeforeConnect;

        /// <summary>참가를 기다리는 중 (실패하면 상태 줄에 이유)</summary>
        public bool IsJoining { get; private set; }

        public bool IsOpen => style.display != DisplayStyle.None;

        public RoomListView()
        {
            MenuStyle.Use(this, "Menu");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;
            style.display = DisplayStyle.None;

            // [1] 어두운 바탕 + [2] 960 x 720 창
            var dim = MenuStyle.FullScreen(DimColor);
            dim.style.alignItems = Align.Center;
            dim.style.justifyContent = Justify.Center;
            Add(dim);

            var window = MenuStyle.RoundPanel(2f, 6f);
            window.style.width = 960;
            window.style.height = 720;
            UiStyle.Padding(window, 36, 36);
            dim.Add(window);

            // [5] 위: 제목 / + 주점 열기 / 새로고침 / 닫기
            var header = UiStyle.Row();
            window.Add(header);
            var title = MenuStyle.Text("주점 찾기", 22, UiColor.TextBody, true);
            title.style.flexGrow = 1;
            header.Add(title);

            _openCreate = MenuStyle.PlateButton("+ 주점 열기", PlateKind.Teal, OpenCreate, 20);
            _openCreate.style.width = 200;
            _openCreate.style.height = 60;
            _openCreate.style.marginLeft = 18;
            header.Add(_openCreate);

            _refresh = MenuStyle.TextButton("새로고침", Refresh, 18, UiColor.TextBody);
            _refresh.style.marginLeft = 18;
            _refresh.style.height = 60;
            header.Add(_refresh);

            _close = MenuStyle.TextButton("닫기", Close, 18, UiColor.TextBody);
            _close.style.height = 60;
            header.Add(_close);

            // [14] 상태 줄
            _status = MenuStyle.Wrap("", 15, UiColor.TextAux, true);
            _status.style.marginTop = 24;
            window.Add(_status);

            // 유니티 추가: IP 로 참가
            _ipBox = UiStyle.Row();
            _ipBox.style.marginTop = 18;
            window.Add(_ipBox);
            var ipLabel = MenuStyle.Text("방장 IP", 15, UiColor.GoldHover, true);
            ipLabel.style.marginRight = 12;
            _ipBox.Add(ipLabel);
            _address = MenuStyle.Input(PlayerPrefs.GetString(MenuFlow.AddressPrefKey, "127.0.0.1"), "127.0.0.1", 64);
            _address.style.width = 300;
            _ipBox.Add(_address);
            _ipPassword = MenuStyle.Input("", "비밀번호(선택)", 16, true);
            _ipPassword.style.width = 220;
            _ipPassword.style.marginLeft = 12;
            _ipBox.Add(_ipPassword);
            _ipJoin = MenuStyle.PlateButton("참가", PlateKind.Red, JoinByAddress, 19);
            _ipJoin.style.width = 160;
            _ipJoin.style.height = 56;
            _ipJoin.style.marginLeft = 18;
            _ipBox.Add(_ipJoin);

            // [15] 목록
            _scroll = new ScrollView(ScrollViewMode.Vertical);
            _scroll.AddToClassList("tm-scroll");
            _scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _scroll.style.flexGrow = 1;
            _scroll.style.marginTop = 18;
            window.Add(_scroll);
            _list = _scroll.contentContainer;

            // [17] 주점 열기 창
            _createPanel = MakeInnerPanel(640, out var create);
            Add(_createPanel);
            create.Add(MenuStyle.Text("주점 열기", 22, UiColor.TextBody, true));
            create.Add(FieldLabel("주점 이름", 14, 24));
            _roomName = MenuStyle.Input("", "비우면 무작위 이름", RunSession.MaxRoomNameLength);
            _roomName.style.marginTop = 6;
            create.Add(_roomName);
            create.Add(FieldLabel("비밀번호(선택)", 15, 18));
            _password = MenuStyle.Input("", "영문 / 숫자 1~16자", 16, true);
            _password.style.marginTop = 6;
            create.Add(_password);
            _private = MenuStyle.Check("목록에 표시하지 않기 (초대로만 입장)", false, 14, UiColor.TextAux);
            _private.style.marginTop = 18;
            create.Add(_private);
            _createError = MenuStyle.Wrap("", 14, UiColor.Alert);
            _createError.style.marginTop = 10;
            create.Add(_createError);
            var createButtons = UiStyle.Row();
            createButtons.style.marginTop = 24;
            createButtons.style.alignSelf = Align.FlexEnd;
            create.Add(createButtons);
            var createCancel = MenuStyle.TextButton("취소", CloseCreate, 15, UiColor.TextAux);
            createCancel.style.marginRight = 18;
            createButtons.Add(createCancel);
            _createConfirm = MenuStyle.PlateButton("열기", PlateKind.Red, ConfirmCreate, 19);
            _createConfirm.style.width = 180;
            _createConfirm.style.height = 60;
            createButtons.Add(_createConfirm);

            // [35] 비밀번호 창
            _passwordPanel = MakeInnerPanel(560, out var pw);
            Add(_passwordPanel);
            pw.Add(MenuStyle.Text("비밀번호", 22, UiColor.TextBody, true));
            _joinPassword = MenuStyle.Input("", "영문 / 숫자 1~16자", 16, true);
            _joinPassword.style.marginTop = 6;
            pw.Add(_joinPassword);
            _joinError = MenuStyle.Wrap("", 14, UiColor.Alert);
            _joinError.style.marginTop = 8;
            pw.Add(_joinError);
            var pwButtons = UiStyle.Row();
            pwButtons.style.marginTop = 24;
            pwButtons.style.alignSelf = Align.FlexEnd;
            pw.Add(pwButtons);
            var pwCancel = MenuStyle.TextButton("취소", ClosePassword, 15, UiColor.TextAux);
            pwCancel.style.marginRight = 18;
            pwButtons.Add(pwCancel);
            var pwConfirm = MenuStyle.PlateButton("입장", PlateKind.Red, ConfirmPassword, 19);
            pwConfirm.style.width = 180;
            pwConfirm.style.height = 60;
            pwButtons.Add(pwConfirm);

            // Enter = 확인
            _joinPassword.RegisterCallback<KeyDownEvent>(e => { if (IsSubmit(e)) ConfirmPassword(); });
            _password.RegisterCallback<KeyDownEvent>(e => { if (IsSubmit(e)) ConfirmCreate(); });
            _roomName.RegisterCallback<KeyDownEvent>(e => { if (IsSubmit(e)) ConfirmCreate(); });
            _address.RegisterCallback<KeyDownEvent>(e => { if (IsSubmit(e)) JoinByAddress(); });

            MenuStyle.OnEscape(this, HandleEscape);
        }

        static bool IsSubmit(KeyDownEvent e) => e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter;

        static Label FieldLabel(string text, int size, float top)
        {
            var l = MenuStyle.Text(text, size, UiColor.GoldHover, true);
            l.style.marginTop = top;
            return l;
        }

        // 겹쳐 뜨는 작은 창 (WBP CreatePanel / PasswordPanel: 어두운 바탕 0.6 + 가운데 판)
        static VisualElement MakeInnerPanel(float width, out VisualElement column)
        {
            var overlay = MenuStyle.FullScreen(InnerDimColor);
            overlay.style.alignItems = Align.Center;
            overlay.style.justifyContent = Justify.Center;
            overlay.style.display = DisplayStyle.None;
            var panel = MenuStyle.RoundPanel(2f, 6f);
            panel.style.width = width;
            UiStyle.Padding(panel, 36, 36);
            overlay.Add(panel);
            column = panel;
            return overlay;
        }

        // ------------------------------------------------------------ 열고 닫기

        public void Open()
        {
            style.display = DisplayStyle.Flex;
            BringToFront();
            SetBusy(false);
            IsJoining = false;
            UiStyle.SetShown(_createPanel, false);
            UiStyle.SetShown(_passwordPanel, false);
            Focus();
            Refresh();   // 열자마자 한 번 찾는다
        }

        public void Close()
        {
            UiStyle.SetShown(_createPanel, false);
            UiStyle.SetShown(_passwordPanel, false);
            _pendingJoin = null;
            IsJoining = false;
            style.display = DisplayStyle.None;
        }

        void HandleEscape()
        {
            // 참가 / 주점 열기를 기다리는 중이면 닫지 않음
            if (_busy) return;
            if (UiStyle.IsShown(_passwordPanel)) ClosePassword();
            else if (UiStyle.IsShown(_createPanel)) CloseCreate();
            else Close();
        }

        // ------------------------------------------------------------ 목록

        public void Refresh()
        {
            _list.Clear();
            bool browsing = RoomDirectory.SupportsBrowsing;
            UiStyle.SetShown(_ipBox, !browsing);
            UiStyle.SetShown(_refresh, browsing);

            if (!browsing)
            {
                SetStatus("방 목록이 없는 모드(IP)입니다. 방장 IP 로 참가하거나 직접 주점을 여세요.");
                return;
            }

            SetStatus("주점을 찾는 중…");
            var throbber = new Throbber(22f);
            throbber.style.alignSelf = Align.Center;
            throbber.style.marginTop = 40;
            throbber.style.marginBottom = 40;
            _list.Add(throbber);
            _refresh.SetEnabled(false);

            RoomDirectory.Refresh(OnRefreshed);
        }

        void OnRefreshed(List<RoomListing> rooms)
        {
            if (panel == null) return;
            _refresh.SetEnabled(!_busy);
            _list.Clear();

            if (rooms == null)
            {
                SetStatus("주점 목록을 불러오지 못했습니다.");
                return;
            }
            if (rooms.Count == 0)
            {
                SetStatus("열린 주점이 없습니다. 직접 열어 보세요.");
                return;
            }

            SetStatus("");
            foreach (var room in rooms)
            {
                var entry = new RoomEntryView(room);
                entry.Chosen += OnEntryChosen;
                entry.SetEnabled(!_busy && room.Players < room.MaxPlayers);
                _list.Add(entry);
            }
        }

        void OnEntryChosen(RoomListing room)
        {
            if (_busy) return;
            // 이공간(재합류 대기)은 나갔던 사람만 들어감 -> 비밀번호는 묻지 않음
            if (!room.bHasPassword || room.bRejoinOpen)
            {
                Join(room, "");
                return;
            }

            _pendingJoin = room;
            _joinPassword.value = "";
            _joinError.text = "";
            UiStyle.SetShown(_passwordPanel, true);
            _joinPassword.Focus();
        }

        // ------------------------------------------------------------ 비밀번호

        void ConfirmPassword()
        {
            var password = _joinPassword.value ?? "";
            // 형식이 틀린 건 방장까지 보낼 것도 없이 여기서 거른다. 맞고 틀림은 방장이 판정
            if (!NetworkRoot.IsValidRoomPassword(password))
            {
                _joinError.text = "영문과 숫자 1~16자로 입력하세요.";
                return;
            }
            UiStyle.SetShown(_passwordPanel, false);
            var room = _pendingJoin;
            _pendingJoin = null;
            if (room != null) Join(room, password);
        }

        void ClosePassword()
        {
            UiStyle.SetShown(_passwordPanel, false);
            _pendingJoin = null;
            Focus();
        }

        void Join(RoomListing room, string password)
        {
            BeforeConnect?.Invoke(false);
            SetBusy(true);
            IsJoining = true;
            SetStatus("주점에 들어가는 중…");
            MenuFlow.ShowLoading("주점에 들어가는 중...", LoadingStyle.Light, true);
            RoomDirectory.Join(room, password, (ok, error) =>
            {
                if (ok) return;   // 곧 주점으로. 방장이 거절하면 연결 실패로 옴 (JoinFailed)
                JoinFailed(error);
            });
        }

        void JoinByAddress()
        {
            if (_busy) return;
            var address = (_address.value ?? "").Trim();
            if (address.Length == 0) address = "127.0.0.1";
            var password = _ipPassword.value ?? "";
            if (password.Length > 0 && !NetworkRoot.IsValidRoomPassword(password))
            {
                SetStatus("비밀번호는 영문과 숫자 1~16자입니다.");
                return;
            }
            PlayerPrefs.SetString(MenuFlow.AddressPrefKey, address);
            PlayerPrefs.Save();

            BeforeConnect?.Invoke(false);
            SetBusy(true);
            IsJoining = true;
            SetStatus("주점에 들어가는 중…");
            MenuFlow.ShowLoading("주점에 들어가는 중...", LoadingStyle.Light, true);
            if (!NetworkRoot.Join(address, password)) JoinFailed("");
        }

        /// <summary>참가 실패 (서브시스템이 남긴 사유. 가득 참 / 비밀번호 / 응답 없음)</summary>
        public void JoinFailed(string reason)
        {
            MenuFlow.HideLoading();
            IsJoining = false;
            SetBusy(false);
            SetStatus(string.IsNullOrEmpty(reason) ? "주점에 들어가지 못했습니다. 목록을 새로고침해 보세요." : reason);
        }

        /// <summary>주점 열기 실패 (팝업은 메인 메뉴가). 버튼만 되살림</summary>
        public void HostFailed()
        {
            SetBusy(false);
            _createConfirm.SetEnabled(true);
        }

        // ------------------------------------------------------------ 주점 열기

        void OpenCreate()
        {
            // 기본은 랜덤 이름 (지우면 열 때 다시 랜덤)
            _roomName.value = RunSession.MakeRandomRoomName(new System.Random());
            _password.value = "";
            _private.value = false;
            _createError.text = "";
            _createConfirm.SetEnabled(true);
            UiStyle.SetShown(_createPanel, true);
            _roomName.Focus();
        }

        void CloseCreate()
        {
            UiStyle.SetShown(_createPanel, false);
            Focus();
        }

        void ConfirmCreate()
        {
            if (_busy) return;
            var name = (_roomName.value ?? "").Trim();
            var password = _password.value ?? "";
            bool listed = !_private.value;

            // 비밀번호는 선택. 넣었다면 규칙에 맞아야 한다
            if (password.Length > 0 && !NetworkRoot.IsValidRoomPassword(password))
            {
                _createError.text = "비밀번호는 영문과 숫자 1~16자입니다.";
                return;
            }
            if (name.Length == 0) name = RunSession.MakeRandomRoomName(new System.Random());

            BeforeConnect?.Invoke(true);
            Game.ServerSession.ClearPendingContinue();
            SetBusy(true);
            _createConfirm.SetEnabled(false);
            MenuFlow.ShowLoading("주점을 여는 중...", LoadingStyle.Light, true);
            if (!NetworkRoot.StartHost(name, password, listed))
            {
                MenuFlow.HideLoading();
                HostFailed();
            }
        }

        // ------------------------------------------------------------ 도우미

        void SetStatus(string text)
        {
            UiStyle.SetLine(_status, text);
        }

        void SetBusy(bool busy)
        {
            // 참가나 주점 열기를 기다리는 동안 다른 걸 누르지 못하게
            _busy = busy;
            _list.SetEnabled(!busy);
            _refresh.SetEnabled(!busy);
            _openCreate.SetEnabled(!busy);
            _close.SetEnabled(!busy);
            _ipJoin.SetEnabled(!busy);
        }
    }

    /// <summary>방 목록 한 줄 (언리얼 URoomEntryWidget + WBP_RoomEntry): 잠김 / 이름 / 인원 / 핑</summary>
    public sealed class RoomEntryView : Button
    {
        public RoomListing Room { get; }

        public event Action<RoomListing> Chosen;

        public RoomEntryView(RoomListing room)
        {
            Room = room;
            MenuStyle.Use(this, "Menu");
            AddToClassList("tm-room-entry");
            clicked += () => Chosen?.Invoke(Room);

            bool locked = room.bHasPassword && !room.bRejoinOpen;
            if (locked)
            {
                var lockMark = new Label("잠김") { pickingMode = PickingMode.Ignore };
                lockMark.AddToClassList("tm-lock");
                Add(lockMark);
            }

            // 이름이 없으면 "<방장>의 주점", 이공간이면 "(재합류 대기)" (언리얼 USessionSubsystem 찾기 결과)
            var name = string.IsNullOrEmpty(room.RoomName) ? (room.HostName ?? "") + "의 주점" : room.RoomName;
            if (room.bRejoinOpen) name += " (재합류 대기)";
            var nameText = MenuStyle.Text(name, 15, UiColor.TextBody, true);
            nameText.style.flexGrow = 1;
            nameText.style.flexShrink = 1;
            nameText.style.overflow = Overflow.Hidden;
            nameText.style.textOverflow = TextOverflow.Ellipsis;
            Add(nameText);

            var players = MenuStyle.Text($"{room.Players} / {room.MaxPlayers}", 13, UiColor.TextAux, true);
            players.style.marginLeft = 24;
            players.style.marginRight = 24;
            Add(players);

            var ping = MenuStyle.Text(room.Ping >= 0 ? room.Ping + "ms" : "", 11, UiColor.TextFaint, true);
            ping.style.minWidth = 48;
            Add(ping);

            UiStyle.IgnorePicking(nameText);
            players.pickingMode = PickingMode.Ignore;
            ping.pickingMode = PickingMode.Ignore;
        }
    }
}
