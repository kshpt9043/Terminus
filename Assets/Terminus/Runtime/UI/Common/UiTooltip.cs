// 마우스를 올리면 뜨는 툴팁 (언리얼 SetToolTip / TerminusUI::SetGameTooltip 자리)
// 화면(패널)마다 툴팁 칸 하나를 맨 위에 두고, 지금 마우스 아래 요소의 툴팁만 그 칸에 넣어 마우스를 따라다니게 함
//
//   UiTooltip.Attach(slot, () => mySkillTooltip);          // 올릴 때마다 build 를 불러 내용을 받음 (null 이면 안 뜸)
//   UiTooltip.SetText(button, "제목\n본문 여러 줄");        // 게임 모양 글자 툴팁. 첫 줄 = 금색 제목, 나머지 = 본문
//   UiTooltip.Detach(slot);                                // 떼기 (SetText(x, "") 도 같음)
using System;
using System.Runtime.CompilerServices;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public static class UiTooltip
    {
        /// <summary>올리고 나서 뜨기까지 (초)</summary>
        public static float ShowDelay = 0.12f;

        /// <summary>마우스에서 툴팁까지 거리</summary>
        public static Vector2 Offset = new Vector2(16f, 20f);

        sealed class Binding
        {
            public Func<VisualElement> Build;
            public bool Hooked;
            public IVisualElementScheduledItem Pending;
        }

        static readonly ConditionalWeakTable<VisualElement, Binding> Bindings = new ConditionalWeakTable<VisualElement, Binding>();

        static VisualElement _host;
        static VisualElement _currentTarget;
        static Vector2 _mouse;   // 패널 좌표

        /// <summary>지금 툴팁이 떠 있는 요소 (없으면 null)</summary>
        public static VisualElement CurrentTarget => _currentTarget;

        // ------------------------------------------------------------ 붙이기

        public static void Attach(VisualElement target, Func<VisualElement> build)
        {
            if (target == null) return;
            var b = Bindings.GetValue(target, _ => new Binding());
            b.Build = build;
            if (!b.Hooked)
            {
                b.Hooked = true;
                target.RegisterCallback<PointerEnterEvent>(OnEnter);
                target.RegisterCallback<PointerMoveEvent>(OnMove);
                target.RegisterCallback<PointerLeaveEvent>(OnLeave);
                target.RegisterCallback<PointerDownEvent>(OnDown, TrickleDown.TrickleDown);
                target.RegisterCallback<DetachFromPanelEvent>(OnDetach);
            }
            // 떠 있는 중에 내용이 바뀌면 바로 다시 그림
            if (_currentTarget == target) Rebuild();
        }

        /// <summary>게임 모양 글자 툴팁 (어두운 판 + 금색 테두리). 첫 줄 = 제목, 한 줄뿐이면 본문만. 빈 글자면 뗌</summary>
        public static void SetText(VisualElement target, string text)
        {
            if (string.IsNullOrEmpty(text))
            {
                Detach(target);
                return;
            }
            Attach(target, () => MakeTextTooltip(text));
        }

        public static void Detach(VisualElement target)
        {
            if (target == null) return;
            if (Bindings.TryGetValue(target, out var b))
            {
                b.Build = null;
                b.Pending?.Pause();
            }
            if (_currentTarget == target) Hide();
        }

        /// <summary>떠 있는 툴팁을 닫음 (화면이 바뀔 때 등)</summary>
        public static void Hide()
        {
            _currentTarget = null;
            if (_host == null) return;
            _host.Clear();
            _host.style.display = DisplayStyle.None;
        }

        /// <summary>SetGameTooltip 모양 (판 #17141C 96%, 테두리 #C9A55A 60%, 제목 15 금 / 본문 13, 300px 에서 줄바꿈)</summary>
        public static VisualElement MakeTextTooltip(string text)
        {
            string title = "", body = text ?? "";
            int nl = body.IndexOf('\n');
            if (nl >= 0)
            {
                title = body.Substring(0, nl);
                body = body.Substring(nl + 1);
            }
            body = body.Trim();

            var card = new VisualElement();
            UiStyle.Use(card, "Common");
            card.AddToClassList("tc-gametip");
            if (title.Length > 0)
            {
                var t = UiStyle.Text(title, 15, UiColor.Gold, TextAnchor.UpperLeft, true);
                t.AddToClassList("tc-gametip__title");
                card.Add(t);
            }
            if (body.Length > 0)
            {
                var b = UiStyle.WrapText(body, 13, UiColor.TextBody, TextAnchor.UpperLeft);
                b.AddToClassList("tc-gametip__body");
                if (title.Length > 0) b.style.marginTop = 6;
                card.Add(b);
            }
            return card;
        }

        // ------------------------------------------------------------ 처리

        static void OnEnter(PointerEnterEvent evt)
        {
            var target = evt.currentTarget as VisualElement;
            if (target == null || !Bindings.TryGetValue(target, out var b) || b.Build == null) return;
            _mouse = evt.position;
            b.Pending?.Pause();
            if (ShowDelay <= 0f) Show(target);
            else b.Pending = target.schedule.Execute(() => Show(target)).StartingIn((long)(ShowDelay * 1000f));
        }

        static void OnMove(PointerMoveEvent evt)
        {
            _mouse = evt.position;
            if (_currentTarget != null && _currentTarget == evt.currentTarget) Place();
        }

        static void OnLeave(PointerLeaveEvent evt)
        {
            var target = evt.currentTarget as VisualElement;
            if (target != null && Bindings.TryGetValue(target, out var b)) b.Pending?.Pause();
            if (_currentTarget == target) Hide();
        }

        // 누르면 닫음 (언리얼 툴팁도 클릭하면 사라짐). 다시 움직이면 안 뜨고, 나갔다 들어오면 다시 뜸
        static void OnDown(PointerDownEvent evt)
        {
            var target = evt.currentTarget as VisualElement;
            if (target != null && Bindings.TryGetValue(target, out var b)) b.Pending?.Pause();
            if (_currentTarget == target) Hide();
        }

        static void OnDetach(DetachFromPanelEvent evt)
        {
            var target = evt.currentTarget as VisualElement;
            if (target != null && Bindings.TryGetValue(target, out var b)) b.Pending?.Pause();
            if (_currentTarget == target) Hide();
        }

        static void Show(VisualElement target)
        {
            if (target?.panel == null || !Bindings.TryGetValue(target, out var b) || b.Build == null) return;
            _currentTarget = target;
            Rebuild();
        }

        static void Rebuild()
        {
            var target = _currentTarget;
            if (target?.panel == null || !Bindings.TryGetValue(target, out var b) || b.Build == null)
            {
                Hide();
                return;
            }

            VisualElement content;
            try { content = b.Build(); }
            catch (Exception e)
            {
                Debug.LogException(e);
                content = null;
            }
            if (content == null)
            {
                Hide();
                _currentTarget = target;   // 나갈 때까지는 이 요소 차례
                return;
            }

            EnsureHost(target.panel);
            _host.Clear();
            _host.Add(content);
            UiStyle.IgnorePicking(content);
            _host.style.display = DisplayStyle.Flex;
            _host.BringToFront();
            Place();
        }

        static void EnsureHost(IPanel panel)
        {
            if (_host == null)
            {
                _host = new VisualElement { name = "TooltipHost", pickingMode = PickingMode.Ignore };
                UiStyle.Use(_host, "Common");
                _host.AddToClassList("tc-tooltip-host");
                _host.RegisterCallback<GeometryChangedEvent>(_ => Place());
            }
            var root = panel.visualTree;
            if (_host.parent != root)
            {
                _host.RemoveFromHierarchy();
                root.Add(_host);
            }
        }

        // 마우스 오른쪽 아래. 화면 밖으로 나가면 반대쪽으로
        static void Place()
        {
            if (_host == null || _host.parent == null || _currentTarget == null) return;
            var root = _host.parent;
            var local = root.WorldToLocal(_mouse);
            float w = _host.resolvedStyle.width, h = _host.resolvedStyle.height;
            float rw = root.resolvedStyle.width, rh = root.resolvedStyle.height;
            if (float.IsNaN(w)) w = 0f;
            if (float.IsNaN(h)) h = 0f;

            float x = local.x + Offset.x;
            float y = local.y + Offset.y;
            if (rw > 0f && x + w > rw) x = local.x - w - 8f;
            if (rh > 0f && y + h > rh) y = rh - h - 4f;
            _host.style.left = Mathf.Max(0f, x);
            _host.style.top = Mathf.Max(0f, y);
        }
    }
}
