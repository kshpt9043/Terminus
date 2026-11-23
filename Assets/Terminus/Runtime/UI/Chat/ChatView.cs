// 멀티 채팅창 (언리얼 UChatWidget / WBP_Chat + UChatSubsystem 의 Enter / 기록). 주점 / 지도 / 전투 어디서나 떠 있음 (UIApp 이 UILayer.Chat 에)
// 혼자 하는 오프라인이면 채팅할 사람이 없어서 안 보임 (언리얼: NM_Standalone 이면 안 만듦)
//
// - Enter: 입력창 열기 (다른 입력칸에 글을 쓰는 중이거나 팝업이 떠 있으면 가로채지 않음)
// - 입력 중 Enter: 전송하고 입력창 닫기. 빈 칸이면 그냥 닫기
// - 입력 중 ESC: 입력창 닫기
// - 제목줄(DragHandle)을 끌어서 위치 옮기기. 위치는 씬이 바뀌어도 유지
//
// 채팅 시안 (Terminus Chat):
//  - 평소: 바탕 검정 30%, 제목줄 흐림(18%), 오래된 줄일수록 흐림 (한 줄 위로 갈 때마다 -12%, 최소 25%), 글자 그림자
//  - 제목줄에 마우스: 제목줄 진해짐(85%) + 아래 구분선
//  - 입력 중: 바탕 검정 70% + 1px 테두리, 모든 줄 또렷, 입력칸 + 글자 수
//  - 새 메시지: 그 줄만 금빛 바탕이 0.25초 켜졌다 1.6초 동안 사라짐
//  - 한 줄: [시간] (핑 태그 / 퀵챗 말풍선) 이름: 내용. 내 말 금색, 안내 하늘색, 남의 말 본문색
//
// 기록은 ClientState.ChatLog (서버 채팅) + AddLocal (핑 / 퀵챗 / 재촉: 각자 받은 알림으로 이 컴퓨터에서 만든 줄)
// 시간은 서버가 안 보내서 받은 시각 (이 컴퓨터 시계)
using System;
using System.Runtime.CompilerServices;
using Terminus.Comms;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Chat
{
    public sealed class ChatView : VisualElement
    {
        // 채팅창 크기 (화면 단위)
        public Vector2 ChatSize = new Vector2(420f, 260f);

        // 처음 뜰 때 위치 (화면 왼쪽 위 기준 비율). 한 번 옮기면 그 자리를 기억
        public Vector2 DefaultPositionRatio = new Vector2(0.015f, 0.4f);

        public int FontSize = 14;
        public int TimeFontSize = 11;
        public Color MyMessageColor = UiColor.Hex("FFD978");
        public Color OtherMessageColor = UiColor.Hex("ECE3CF");
        public Color SystemMessageColor = UiColor.Hex("99D9FF");
        public Color TimeColor = UiColor.Hex("8A8478");

        // 바탕 / 테두리 (평소 / 입력 중)
        public Color IdleBackgroundColor = new Color(0f, 0f, 0f, 0.3f);
        public Color IdleOutlineColor = new Color(1f, 1f, 1f, 0.03f);
        public Color ActiveBackgroundColor = new Color(0f, 0f, 0f, 0.7f);
        public Color ActiveOutlineColor = UiColor.Hex("ECE3CF", 0.2f);

        // 제목줄 불투명도 (평소 / 마우스 올림 / 입력 중)
        public Vector3 HeaderOpacity = new Vector3(0.18f, 0.85f, 1f);

        // 새 메시지 금빛 (최대 진하기 / 켜진 채 있는 시간 / 사라지는 시간)
        public Color FlashColor = UiColor.Hex("FFD978", 0.28f);
        public float FlashHold = 0.25f;
        public float FlashFade = 1.6f;

        // 한 번에 보낼 수 있는 글자 수
        public const int MaxMessageLength = 200;

        // 기록 최대 줄 수 (언리얼 UChatSubsystem::MaxHistory)
        public const int MaxHistory = 100;

        // ------------------------------------------------------------ 정적: 기록 / 시간 / 위치 (씬이 바뀌어도 유지 = 언리얼 게임 인스턴스 서브시스템)

        static readonly ConditionalWeakTable<ChatMsg, string> Times = new ConditionalWeakTable<ChatMsg, string>();
        static Vector2? _savedPosition;
        static ChatView _active;
        static event Action<ChatMsg> LocalAdded;

        /// <summary>입력창이 열려 있나 (다른 화면이 키를 가로채지 않게)</summary>
        public static bool IsAnyInputOpen => _active != null && _active._inputOpen && _active.panel != null;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            _active = null;
            ClientState.ChatReceived -= Stamp;
            ClientState.ChatReceived += Stamp;
        }

        static void Stamp(ChatMsg m)
        {
            if (m == null) return;
            Times.Remove(m);
            Times.Add(m, DateTime.Now.ToString("HH:mm"));
        }

        static string TimeOf(ChatMsg m) => m != null && Times.TryGetValue(m, out var t) ? t : "";

        /// <summary>
        /// 서버를 거치지 않고 이 컴퓨터에서 바로 넣는 줄 (핑 / 퀵챗 / 재촉: 각자 받은 알림으로 만듦. 언리얼 UChatSubsystem::AddLocalMessage)
        /// 기록(ClientState.ChatLog)에도 남아서 씬이 바뀌어도 보임
        /// </summary>
        public static void AddLocal(ChatMsg message)
        {
            if (message == null) return;
            Stamp(message);
            ClientState.ChatLog.Add(message);
            while (ClientState.ChatLog.Count > MaxHistory) ClientState.ChatLog.RemoveAt(0);
            LocalAdded?.Invoke(message);
        }

        // ------------------------------------------------------------ 위젯

        readonly VisualElement _window;
        readonly VisualElement _background;
        readonly VisualElement _dragHandle;
        readonly VisualElement _headerLine;
        readonly ScrollView _messageList;
        readonly VisualElement _inputFrame;
        readonly TextField _inputBox;
        readonly Label _counterText;

        bool _inputOpen;
        bool _headerHovered;
        bool _dragging;
        Vector2 _dragOffset;
        bool _positioned;

        VisualElement _flashLine;
        float _flashStartTime;
        IVisualElementScheduledItem _flashTick;
        VisualElement _keyRoot;

        public ChatView()
        {
            var sheet = Resources.Load<StyleSheet>("UI/Chat/Chat");
            if (sheet != null) styleSheets.Add(sheet);
            else Debug.LogWarning("[UI] USS 없음: Resources/UI/Chat/Chat");

            name = "Chat";
            pickingMode = PickingMode.Ignore;

            // 화면에 붙일 크기 (뷰포트에 직접 붙은 위젯 = 레이어 안의 절대 위치 창)
            _window = new VisualElement { name = "SizeBox_0", pickingMode = PickingMode.Ignore };
            _window.style.position = Position.Absolute;
            _window.style.width = ChatSize.x;
            _window.style.height = ChatSize.y;
            Add(_window);

            // 채팅창 바탕 (둥근 판). 상태마다 바탕 / 테두리 색을 바꿈
            _background = new VisualElement { name = "Background" };
            _background.style.flexGrow = 1;
            UiStyle.Radius(_background, 3f);
            _window.Add(_background);

            // 제목줄 22px: [잡는 점] 채팅  (여기를 잡고 끔)
            _dragHandle = new VisualElement { name = "DragHandle" };
            _dragHandle.style.height = 22f;
            _dragHandle.style.flexShrink = 0;
            _dragHandle.style.flexDirection = FlexDirection.Row;
            _dragHandle.style.alignItems = Align.Center;
            _dragHandle.style.paddingLeft = 10f;
            _dragHandle.style.paddingRight = 10f;
            _background.Add(_dragHandle);

            var grip = new VisualElement { name = "Grip", pickingMode = PickingMode.Ignore };
            var gripTex = UiTextures.LoadUnreal("/Game/UI/Textures/Common/T_Chat_Grip");
            if (gripTex != null) grip.style.backgroundImage = new StyleBackground(gripTex);
            grip.style.width = 8f;
            grip.style.height = 13f;
            grip.style.unityBackgroundImageTintColor = OtherMessageColor;
            grip.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Contain);
            _dragHandle.Add(grip);

            var title = UiStyle.Text("채팅", 12, OtherMessageColor, TextAnchor.MiddleLeft);
            title.style.letterSpacing = 1f;   // 언리얼 LetterSpacing 80 (= 0.08em)
            title.style.marginLeft = 8f;
            title.pickingMode = PickingMode.Ignore;
            _dragHandle.Add(title);

            _headerLine = new VisualElement { name = "HeaderLine", pickingMode = PickingMode.Ignore };
            _headerLine.style.height = 1f;
            _headerLine.style.flexShrink = 0;
            _headerLine.style.backgroundColor = UiColor.Hex("ECE3CF", 0.12f);
            _background.Add(_headerLine);

            _messageList = new ScrollView(ScrollViewMode.Vertical) { name = "MessageList" };
            _messageList.AddToClassList("ct-list");
            _messageList.verticalScrollerVisibility = ScrollerVisibility.Hidden;
            _messageList.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _messageList.style.flexGrow = 1;
            _messageList.style.marginLeft = 10f;
            _messageList.style.marginTop = 4f;
            _messageList.style.marginRight = 10f;
            _messageList.style.marginBottom = 8f;
            // 줄이 적으면 아래부터 쌓이게 (시안: 최근 줄이 맨 아래)
            _messageList.contentViewport.RegisterCallback<GeometryChangedEvent>(_ =>
                _messageList.contentContainer.style.minHeight = _messageList.contentViewport.layout.height);
            _background.Add(_messageList);

            // 입력칸 30px: [입력 ............ 9/200 · Enter 보내기 · Esc 닫기]
            _inputFrame = new VisualElement { name = "InputFrame" };
            _inputFrame.style.height = 30f;
            _inputFrame.style.flexShrink = 0;
            _inputFrame.style.marginLeft = 8f;
            _inputFrame.style.marginRight = 8f;
            _inputFrame.style.marginBottom = 8f;
            _inputFrame.style.flexDirection = FlexDirection.Row;
            _inputFrame.style.alignItems = Align.Center;
            _inputFrame.style.paddingLeft = 10f;
            _inputFrame.style.paddingRight = 10f;
            _inputFrame.style.backgroundColor = new Color(0f, 0f, 0f, 0.45f);
            UiStyle.Border(_inputFrame, 1f, UiColor.Hex("ECE3CF", 0.22f));
            UiStyle.Radius(_inputFrame, 2f);
            _background.Add(_inputFrame);

            _inputBox = new TextField { name = "InputBox", maxLength = MaxMessageLength, isDelayed = false };
            _inputBox.AddToClassList("ct-input");
            _inputBox.RegisterValueChangedCallback(_ => UpdateCounter());
            _inputBox.RegisterCallback<KeyDownEvent>(OnInputKeyDown, TrickleDown.TrickleDown);
            _inputFrame.Add(_inputBox);

            _counterText = UiStyle.Text("", 11, TimeColor, TextAnchor.MiddleRight);
            _counterText.style.marginLeft = 8f;
            _counterText.pickingMode = PickingMode.Ignore;
            _inputFrame.Add(_counterText);
            UiStyle.SetShown(_inputFrame, false);

            // 제목줄: 마우스 올림 / 끌기
            _dragHandle.RegisterCallback<PointerEnterEvent>(_ => SetHeaderHovered(true));
            _dragHandle.RegisterCallback<PointerLeaveEvent>(_ => SetHeaderHovered(false));
            _dragHandle.RegisterCallback<PointerDownEvent>(OnHeaderPointerDown);
            _dragHandle.RegisterCallback<PointerMoveEvent>(OnHeaderPointerMove);
            _dragHandle.RegisterCallback<PointerUpEvent>(OnHeaderPointerUp);
            _dragHandle.RegisterCallback<PointerCaptureOutEvent>(_ => _dragging = false);

            // 채팅창 안 클릭은 아래 화면(지도 등)으로 새지 않게
            _background.RegisterCallback<PointerDownEvent>(e => e.StopPropagation());

            RegisterCallback<GeometryChangedEvent>(_ => PlaceWindow());
            RegisterCallback<AttachToPanelEvent>(OnAttach);
            RegisterCallback<DetachFromPanelEvent>(OnDetach);

            ApplyBackground();
        }

        void OnAttach(AttachToPanelEvent e)
        {
            _active = this;
            ClientState.ChatReceived -= HandleMessageAdded;
            ClientState.ChatReceived += HandleMessageAdded;
            LocalAdded -= HandleMessageAdded;
            LocalAdded += HandleMessageAdded;
            ClientState.PartyChanged -= RefreshVisible;
            ClientState.PartyChanged += RefreshVisible;

            // Enter 는 포커스가 어디 있든 (언리얼 입력 전처리기) -> 패널 맨 위에서 먼저 봄
            _keyRoot = panel?.visualTree;
            _keyRoot?.RegisterCallback<KeyDownEvent>(OnGlobalKeyDown, TrickleDown.TrickleDown);

            RebuildFromHistory();
            RefreshVisible();
            _positioned = false;
            PlaceWindow();
        }

        void OnDetach(DetachFromPanelEvent e)
        {
            if (_active == this) _active = null;
            ClientState.ChatReceived -= HandleMessageAdded;
            LocalAdded -= HandleMessageAdded;
            ClientState.PartyChanged -= RefreshVisible;
            _keyRoot?.UnregisterCallback<KeyDownEvent>(OnGlobalKeyDown, TrickleDown.TrickleDown);
            _keyRoot = null;
            CloseInput();
        }

        // 혼자(오프라인)면 채팅할 사람이 없음
        void RefreshVisible()
        {
            bool show = !NetworkRoot.IsSinglePlayer;
            UiStyle.SetShown(_window, show);
            if (!show) CloseInput();
        }

        // ------------------------------------------------------------ 위치

        Vector2 ViewSize => new Vector2(layout.width, layout.height);

        void PlaceWindow()
        {
            var view = ViewSize;
            if (float.IsNaN(view.x) || view.x <= 0f || view.y <= 0f) return;
            if (!_positioned)
            {
                _positioned = true;
                MoveTo(_savedPosition ?? Vector2.Scale(view, DefaultPositionRatio), _savedPosition.HasValue);
            }
            else
            {
                MoveTo(new Vector2(_window.resolvedStyle.left, _window.resolvedStyle.top), _savedPosition.HasValue);
            }
        }

        // 제목줄이 화면 밖으로 나가 다시 못 잡는 일이 없게 화면 안으로
        void MoveTo(Vector2 position, bool remember)
        {
            var view = ViewSize;
            if (view.x > 0f && view.y > 0f)
            {
                position.x = Mathf.Clamp(position.x, 0f, Mathf.Max(0f, view.x - ChatSize.x));
                position.y = Mathf.Clamp(position.y, 0f, Mathf.Max(0f, view.y - ChatSize.y));
            }
            _window.style.left = position.x;
            _window.style.top = position.y;
            if (remember) _savedPosition = position;
        }

        void OnHeaderPointerDown(PointerDownEvent e)
        {
            if (e.button != 0) return;
            _dragging = true;
            _dragOffset = (Vector2)e.position - new Vector2(_window.resolvedStyle.left, _window.resolvedStyle.top);
            _dragHandle.CapturePointer(e.pointerId);
            e.StopPropagation();
        }

        void OnHeaderPointerMove(PointerMoveEvent e)
        {
            if (!_dragging || !_dragHandle.HasPointerCapture(e.pointerId)) return;
            MoveTo((Vector2)e.position - _dragOffset, true);
        }

        void OnHeaderPointerUp(PointerUpEvent e)
        {
            if (!_dragging || e.button != 0) return;
            _dragging = false;
            if (_dragHandle.HasPointerCapture(e.pointerId)) _dragHandle.ReleasePointer(e.pointerId);
        }

        void SetHeaderHovered(bool hovered)
        {
            if (_headerHovered == hovered) return;
            _headerHovered = hovered;
            ApplyBackground();
        }

        // ------------------------------------------------------------ 메시지

        // 채팅 기록 전체를 다시 그림 (처음 / 씬이 바뀌어 새로 붙었을 때)
        void RebuildFromHistory()
        {
            _messageList.Clear();
            _flashLine = null;
            foreach (var m in ClientState.ChatLog) AddLine(m);
            ApplyLineStyles();
            ScrollToEnd();
        }

        void HandleMessageAdded(ChatMsg message)
        {
            // 내가 보낸 건 항상 맨 아래로. 남이 보낸 건 맨 아래를 보고 있을 때만 따라 내려감
            // (위로 올려 지난 대화를 읽는 중이면 화면이 튀지 않게 그대로)
            bool wasAtEnd = _messageList.scrollOffset.y >= _messageList.verticalScroller.highValue - 1f;

            // 이전에 깜빡이던 줄은 바로 끔
            if (_flashLine != null) _flashLine.style.backgroundColor = Color.clear;

            _flashLine = AddLine(message);
            _flashStartTime = Time.realtimeSinceStartup;
            if (_flashTick == null) _flashTick = schedule.Execute(TickFlash).Every(16);
            else _flashTick.Resume();
            TickFlash();

            ApplyLineStyles();
            if (wasAtEnd || IsMyMessage(message)) ScrollToEnd();
        }

        // 새 메시지 줄 금빛: 잠깐 켜졌다가 서서히 사라짐
        void TickFlash()
        {
            if (_flashLine == null)
            {
                _flashTick?.Pause();
                return;
            }
            float elapsed = Time.realtimeSinceStartup - _flashStartTime;
            float fade = elapsed <= FlashHold ? 1f : 1f - Mathf.Clamp01((elapsed - FlashHold) / Mathf.Max(FlashFade, 0.01f));
            var c = FlashColor;
            c.a *= fade;
            _flashLine.style.backgroundColor = c;
            if (fade <= 0f)
            {
                _flashLine = null;
                _flashTick?.Pause();
            }
        }

        void ScrollToEnd()
        {
            _messageList.schedule.Execute(() => _messageList.scrollOffset = new Vector2(0f, _messageList.verticalScroller.highValue));
        }

        bool IsMyMessage(ChatMsg m)
        {
            var me = ClientState.Me;
            return m != null && !m.bSystem && me != null && m.Sender == me.Name;
        }

        VisualElement AddLine(ChatMsg message)
        {
            if (message == null) return null;

            // 너무 많이 쌓이면 오래된 줄부터 지움 (기록 보관 수와 맞춤)
            while (_messageList.contentContainer.childCount >= MaxHistory) _messageList.contentContainer.RemoveAt(0);

            bool system = message.bSystem;
            PingKind pingKind = PingKind.Focus;
            bool ping = system && PartyComms.FindPingKind(message.Text, out pingKind);
            bool quick = !system && PartyComms.IsQuickChatText(message.Text);

            // 줄 색: 내 말 금 / 안내 하늘 / 남의 말과 핑 본문색
            var lineColor = ping ? OtherMessageColor : system ? SystemMessageColor : IsMyMessage(message) ? MyMessageColor : OtherMessageColor;

            // 줄 하나 = 금빛 깜빡임용 바탕 + [시간][태그][이름: ][내용]
            var line = new VisualElement { pickingMode = PickingMode.Ignore };
            line.style.flexDirection = FlexDirection.Row;
            line.style.alignItems = Align.Center;
            line.style.paddingLeft = 4f;
            line.style.paddingRight = 4f;
            line.style.marginLeft = -4f;
            line.style.marginRight = -4f;
            line.style.marginTop = 1f;
            line.style.marginBottom = 1f;
            line.style.flexShrink = 0;
            UiStyle.Radius(line, 2f);

            Label AddText(string text, int size, Color color, bool bold, float rightPad, bool fill = false)
            {
                var block = UiStyle.Text(text, size, color, TextAnchor.MiddleLeft, bold);
                block.AddToClassList("ct-line-text");
                block.pickingMode = PickingMode.Ignore;
                block.style.marginRight = rightPad;
                block.style.flexShrink = 0;
                if (fill) block.AddToClassList("ct-line-fill");
                line.Add(block);
                return block;
            }

            var time = TimeOf(message);
            if (!string.IsNullOrEmpty(time)) AddText($"[{time}]", TimeFontSize, TimeColor, false, 6f);

            if (ping)
            {
                AddText("[핑]", FontSize, UiColor.Hex(PartyComms.GetPingColorHex(pingKind)), true, 5f);
                AddText(message.Text.Substring(PartyComms.PingPrefix.Length), FontSize, lineColor, false, 0f, true);
            }
            else if (system)
            {
                AddText("· " + message.Text, FontSize, lineColor, false, 0f, true);
            }
            else
            {
                // 퀵챗은 이름 앞에 작은 말풍선
                if (quick)
                {
                    var bubble = new VisualElement { pickingMode = PickingMode.Ignore };
                    bubble.style.width = 11f;
                    bubble.style.height = 8f;
                    bubble.style.marginTop = 1f;
                    bubble.style.marginRight = 5f;
                    bubble.style.opacity = 0.8f;
                    bubble.style.flexShrink = 0;
                    UiStyle.Border(bubble, 1.5f, lineColor);
                    UiStyle.Radius(bubble, 3f);
                    line.Add(bubble);
                }
                AddText(message.Sender, FontSize, lineColor, true, 0f);
                AddText(": " + message.Text, FontSize, lineColor, false, 0f, true);
            }

            _messageList.Add(line);
            return line;
        }

        void ApplyLineStyles()
        {
            // 평소엔 아래 줄일수록 또렷 (한 줄 위로 갈 때마다 12% 흐리게, 최소 25%) + 그림자. 입력 중엔 모두 또렷, 그림자 없음
            var lines = _messageList.contentContainer;
            int count = lines.childCount;
            var shadow = new TextShadow { offset = new Vector2(0f, 1f), blurRadius = 0f, color = new Color(0f, 0f, 0f, _inputOpen ? 0f : 0.95f) };
            for (int i = 0; i < count; ++i)
            {
                var line = lines[i];
                int age = count - 1 - i;
                line.style.opacity = _inputOpen ? 1f : Mathf.Max(0.25f, 1f - age * 0.12f);
                foreach (var child in line.Children())
                {
                    if (child is Label label) label.style.textShadow = shadow;
                }
            }
        }

        // ------------------------------------------------------------ 입력

        bool CanOpenInput()
        {
            if (panel == null || _window.resolvedStyle.display == DisplayStyle.None) return false;

            // 다른 입력칸에 글을 쓰는 중이면 그쪽 키
            var focused = panel.focusController?.focusedElement as VisualElement;
            if (focused != null && focused != _inputBox && (focused is TextField || focused.GetFirstAncestorOfType<TextField>() != null)) return false;

            // 팝업이 떠 있으면 팝업의 Enter = 확인 이 그대로 동작
            var popupLayer = UIRoot.Instance != null ? UIRoot.Instance.GetLayer(UILayer.Popup) : null;
            if (popupLayer != null && popupLayer.childCount > 0) return false;
            if (LoadingScreenWidget.IsShowing) return false;
            return true;
        }

        void OnGlobalKeyDown(KeyDownEvent e)
        {
            if (_inputOpen) return;
            if (e.keyCode != KeyCode.Return && e.keyCode != KeyCode.KeypadEnter) return;
            if (!CanOpenInput()) return;
            OpenInput();
            e.StopPropagation();
        }

        void OnInputKeyDown(KeyDownEvent e)
        {
            if (!_inputOpen) return;
            if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter)
            {
                Commit();
                e.StopImmediatePropagation();
            }
            else if (e.keyCode == KeyCode.Escape)
            {
                CloseInput();
                e.StopImmediatePropagation();
            }
            else if (e.character == '\n' || e.character == '\r')
            {
                e.StopImmediatePropagation();
            }
        }

        public void OpenInput()
        {
            if (_inputOpen) return;
            _inputOpen = true;

            _inputBox.SetValueWithoutNotify("");
            UiStyle.SetShown(_inputFrame, true);
            UpdateCounter();

            // 최신 대화를 보면서 입력하게
            ScrollToEnd();
            _inputBox.schedule.Execute(() => { if (_inputOpen) _inputBox.Focus(); });

            ApplyBackground();
        }

        public void CloseInput()
        {
            if (!_inputOpen) return;
            _inputOpen = false;

            _inputBox.SetValueWithoutNotify("");
            UiStyle.SetShown(_inputFrame, false);

            // 키보드 포커스를 게임 화면으로 돌려줌 (입력창이 숨었는데 포커스를 쥐고 있지 않게)
            _inputBox.Blur();

            ApplyBackground();
        }

        void Commit()
        {
            var message = (_inputBox.value ?? "").Trim();
            if (message.Length > 0)
            {
                if (message.Length > MaxMessageLength) message = message.Substring(0, MaxMessageLength);
                NetBus.Request(new SendChatReq { Text = message });
            }
            CloseInput();
        }

        void UpdateCounter()
        {
            int length = (_inputBox.value ?? "").Length;
            UiStyle.SetTextIfChanged(_counterText, $"{length}/{MaxMessageLength} · Enter 보내기 · Esc 닫기");
        }

        void ApplyBackground()
        {
            // 둥근 판 (3px). 상태마다 바탕 / 테두리
            _background.style.backgroundColor = _inputOpen ? ActiveBackgroundColor : IdleBackgroundColor;
            UiStyle.Border(_background, 1f, _inputOpen ? ActiveOutlineColor : IdleOutlineColor);

            // 제목줄(드래그)만 클릭을 받고, 대화 목록은 입력 중일 때만 (평소엔 아래 전투 대상 버튼이 눌리게)
            var picking = _inputOpen ? PickingMode.Position : PickingMode.Ignore;
            _background.pickingMode = picking;
            _messageList.pickingMode = picking;
            _messageList.contentViewport.pickingMode = picking;
            _messageList.contentContainer.pickingMode = picking;

            // 제목줄: 평소 흐림, 마우스 / 입력 중 진함 + 구분선
            _dragHandle.style.opacity = _inputOpen ? HeaderOpacity.z : _headerHovered ? HeaderOpacity.y : HeaderOpacity.x;
            _headerLine.style.visibility = _inputOpen || _headerHovered ? Visibility.Visible : Visibility.Hidden;

            ApplyLineStyles();
        }
    }
}
