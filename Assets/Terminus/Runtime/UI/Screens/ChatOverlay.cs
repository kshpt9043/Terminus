// 채팅 (언리얼 UChatWidget 의 최소판): 왼쪽 아래 로그 + 입력. 시스템 안내도 여기
using Terminus.Game.Client;
using Terminus.Game.Net;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Screens
{
    public sealed class ChatOverlay : VisualElement
    {
        readonly ScrollView _log;
        readonly TextField _input;

        public ChatOverlay()
        {
            pickingMode = PickingMode.Ignore;
            var box = UiKit.Column();
            box.style.position = Position.Absolute;
            // 아래쪽은 전투 HUD 자리라 왼쪽 위 (상단바 밑)
            box.style.left = 16;
            box.style.top = 72;
            box.style.width = 460;
            box.style.backgroundColor = new Color(0, 0, 0, 0.45f);
            box.style.paddingLeft = 8;
            box.style.paddingRight = 8;
            box.style.paddingTop = 6;
            box.style.paddingBottom = 6;
            Add(box);

            _log = new ScrollView(ScrollViewMode.Vertical);
            _log.style.height = 130;
            box.Add(_log);

            _input = new TextField();
            _input.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode != KeyCode.Return && e.keyCode != KeyCode.KeypadEnter) return;
                var text = (_input.value ?? "").Trim();
                if (text.Length > 0) NetBus.Request(new SendChatReq { Text = text });
                _input.value = "";
            }, TrickleDown.TrickleDown);
            box.Add(_input);

            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.ChatReceived += Append;
                _log.Clear();
                foreach (var m in ClientState.ChatLog) Append(m);
            });
            RegisterCallback<DetachFromPanelEvent>(_ => ClientState.ChatReceived -= Append);
        }

        void Append(ChatMsg m)
        {
            var line = UiKit.Text(m.bSystem ? m.Text : $"{m.Sender}: {m.Text}", m.bSystem ? "t-aux t-wrap" : "t-body t-wrap");
            if (m.bSystem) line.style.color = UiKit.Hex("C9A45C");
            line.style.fontSize = 14;
            _log.Add(line);
            while (_log.contentContainer.childCount > 60) _log.contentContainer.RemoveAt(0);
            _log.schedule.Execute(() => _log.scrollOffset = new Vector2(0, float.MaxValue));
        }
    }
}
