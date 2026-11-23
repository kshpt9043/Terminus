// UI Toolkit 공통 도우미 (언리얼 Widgets/Common/TerminusUIBuilder 자리). 스타일은 Theme/Terminus.uss 클래스
using System;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI
{
    public static class UiKit
    {
        public static Label Text(string text, string cls = "t-body")
        {
            var l = new Label(text);
            if (!string.IsNullOrEmpty(cls)) foreach (var c in cls.Split(' ')) l.AddToClassList(c);
            return l;
        }

        public static Label Title(string text) => Text(text, "t-title");

        public static Button Button(string label, Action onClick, string cls = "t-button")
        {
            var b = new Button(onClick) { text = label };
            foreach (var c in cls.Split(' ')) b.AddToClassList(c);
            return b;
        }

        public static Button Hex(string label, Action onClick, string kind = "primary") =>
            Button(label, onClick, "t-hex t-hex--" + kind);

        public static VisualElement Panel(string cls = "t-panel")
        {
            var v = new VisualElement();
            foreach (var c in cls.Split(' ')) v.AddToClassList(c);
            return v;
        }

        public static VisualElement Row(float gap = 8)
        {
            var v = new VisualElement();
            v.style.flexDirection = FlexDirection.Row;
            v.style.alignItems = Align.Center;
            v.userData = gap;
            return v;
        }

        public static VisualElement Column()
        {
            var v = new VisualElement();
            v.style.flexDirection = FlexDirection.Column;
            return v;
        }

        public static VisualElement Spacer(float size)
        {
            var v = new VisualElement();
            v.style.width = size;
            v.style.height = size;
            v.style.flexShrink = 0;
            return v;
        }

        /// <summary>화면 전체를 덮는 어두운 배경 + 가운데 판</summary>
        public static VisualElement Modal(out VisualElement panel, float width = 720)
        {
            var dim = new VisualElement();
            dim.AddToClassList("t-dim");
            dim.style.alignItems = Align.Center;
            dim.style.justifyContent = Justify.Center;
            panel = Panel();
            panel.style.width = width;
            panel.style.maxHeight = Length.Percent(90);
            dim.Add(panel);
            return dim;
        }

        public static VisualElement FullScreen()
        {
            var v = new VisualElement();
            v.style.position = Position.Absolute;
            v.style.left = 0;
            v.style.top = 0;
            v.style.right = 0;
            v.style.bottom = 0;
            return v;
        }

        public static void Margin(VisualElement e, float all)
        {
            e.style.marginLeft = all;
            e.style.marginRight = all;
            e.style.marginTop = all;
            e.style.marginBottom = all;
        }

        public static Color Hex(string hex)
        {
            ColorUtility.TryParseHtmlString(hex.StartsWith("#") ? hex : "#" + hex, out var c);
            return c;
        }
    }
}
