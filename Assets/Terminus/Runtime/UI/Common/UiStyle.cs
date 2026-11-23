// UI 를 코드로 만들 때 쓰는 공통 도우미 (언리얼 Widgets/Common/TerminusUIBuilder 이식)
// 기존 Terminus.UI.UiKit 은 그대로 두고, 진짜 육각 판 텍스처 / 금 구분선 / 컴포넌트 USS 로드는 여기
//
// 컴포넌트 USS: Assets/Terminus/UI/Common/Resources/UI/Common/<이름>.uss  ->  UiStyle.Use(element, "Common")
// 클래스 이름은 tc- 로 시작 (Theme/Terminus.uss 의 t- 클래스와 안 겹치게)
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    /// <summary>육각 플레이트 버튼 종류 (스타일 가이드 03)</summary>
    public enum HexKind
    {
        Primary,    // 주 행동 (붉은)  T_UI_Hex_Primary_*
        Secondary,  // 보조 행동 (청록) T_UI_Hex_Secondary_*
        Currency,   // 재화 / 구매 (금) T_UI_Hex_Gold_*
    }

    /// <summary>평평한 버튼 바탕</summary>
    public enum FlatKind
    {
        Board,  // WBP 의 취소 / 보조 버튼 (#1B1E25 판, 호버 #2E333D + 금 테두리)
        Card,   // C++ MakeFlatButtonStyle 기본 (#262A32 판)
    }

    public static class UiStyle
    {
        // 글자 크기 단계 (스타일 가이드 02)
        public const int ScreenTitle = 40;
        public const int Section = 22;
        public const int Subtitle = 17;
        public const int Body = 16;
        public const int Aux = 13;

        public const string BoldClass = "t-bold";

        // ------------------------------------------------------------ 스타일 시트

        const string SheetFolder = "UI/Common/";
        static readonly Dictionary<string, StyleSheet> Sheets = new Dictionary<string, StyleSheet>();

        /// <summary>컴포넌트 USS 를 요소에 붙임 ("Common", "Skill", "Relic"). 같은 시트는 한 번만</summary>
        public static void Use(VisualElement element, params string[] sheetNames)
        {
            if (element == null || sheetNames == null) return;
            foreach (var name in sheetNames)
            {
                var sheet = LoadSheet(name);
                if (sheet != null && !element.styleSheets.Contains(sheet)) element.styleSheets.Add(sheet);
            }
        }

        public static StyleSheet LoadSheet(string name)
        {
            if (Sheets.TryGetValue(name, out var cached)) return cached;
            var sheet = Resources.Load<StyleSheet>(SheetFolder + name);
            if (sheet == null) Debug.LogWarning("[UI] USS 없음: Resources/" + SheetFolder + name + " (Assets/Terminus/UI/Common/Resources/UI/Common)");
            Sheets[name] = sheet;
            return sheet;
        }

        // ------------------------------------------------------------ 글자

        /// <summary>글자 하나 (MakeText). color 가 없으면 본문색. 가운데 정렬이 기본 (언리얼과 같음)</summary>
        public static Label Text(string text, int size = Body, Color? color = null, TextAnchor align = TextAnchor.UpperCenter, bool bold = false)
        {
            var l = new Label(text ?? "");
            l.AddToClassList("tc-text");
            l.style.fontSize = size;
            l.style.color = color ?? UiColor.TextBody;
            l.style.unityTextAlign = align;
            l.style.whiteSpace = WhiteSpace.NoWrap;   // 언리얼 MakeText 는 줄바꿈 없음 (WrapText 가 켬)
            if (bold) l.AddToClassList(BoldClass);
            return l;
        }

        /// <summary>자동 줄바꿈 글자 (MakeWrapText)</summary>
        public static Label WrapText(string text, int size = Body, Color? color = null, TextAnchor align = TextAnchor.UpperCenter, bool bold = false)
        {
            var l = Text(text, size, color, align, bold);
            l.style.whiteSpace = WhiteSpace.Normal;
            return l;
        }

        /// <summary>화면 제목 (40 / 굵게 / 금 / 자간 0.08em)</summary>
        public static Label Title(string text, int size = ScreenTitle)
        {
            var l = Text(text, size, UiColor.Gold, TextAnchor.MiddleCenter, true);
            l.style.letterSpacing = Mathf.Round(size * 0.08f);
            return l;
        }

        public static void SetBold(VisualElement text, bool bold = true) => text?.EnableInClassList(BoldClass, bold);

        /// <summary>월드 위에 뜨는 글자처럼 배경 없이 읽혀야 할 때 그림자</summary>
        public static void AddShadow(VisualElement text, float opacity = 0.85f)
        {
            if (text == null) return;
            text.style.textShadow = new TextShadow { offset = new Vector2(1f, 1f), blurRadius = 0f, color = new Color(0f, 0f, 0f, opacity) };
        }

        /// <summary>같은 글자면 다시 넣지 않음 (자주 부르는 곳에서 레이아웃이 다시 잡히지 않게)</summary>
        public static void SetTextIfChanged(TextElement text, string value)
        {
            value ??= "";
            if (text != null && text.text != value) text.text = value;
        }

        /// <summary>글자 넣고, 비었으면 숨김</summary>
        public static void SetLine(TextElement text, string value)
        {
            if (text == null) return;
            SetTextIfChanged(text, value);
            SetShown(text, !string.IsNullOrEmpty(value));
        }

        // ------------------------------------------------------------ 버튼

        /// <summary>육각 플레이트 버튼 (280x56 이상, 양 끝 18px 는 늘이지 않음). 진짜 T_UI_Hex_* 텍스처</summary>
        public static Button HexButton(string label, HexKind kind, Action onClick = null, int fontSize = Subtitle)
        {
            var b = onClick != null ? new Button(onClick) : new Button();
            b.text = label ?? "";
            StyleHex(b, kind);
            if (fontSize != Subtitle) b.style.fontSize = fontSize;
            return b;
        }

        /// <summary>이미 있는 버튼에 육각 판 입히기 (종류를 바꿀 때도)</summary>
        public static void StyleHex(Button button, HexKind kind)
        {
            if (button == null) return;
            UseOn(button);
            button.RemoveFromClassList("tc-flat");
            button.RemoveFromClassList("tc-flat--card");
            button.AddToClassList("tc-hex");
            button.EnableInClassList("tc-hex--primary", kind == HexKind.Primary);
            button.EnableInClassList("tc-hex--secondary", kind == HexKind.Secondary);
            button.EnableInClassList("tc-hex--currency", kind == HexKind.Currency);
        }

        /// <summary>평평한 보조 버튼 (6px 둥근 판, 호버 금 테두리). fill 을 주면 그 바탕색 (호버 때도 유지)</summary>
        public static Button FlatButton(string label, Action onClick = null, int fontSize = Body, FlatKind kind = FlatKind.Board, Color? fill = null)
        {
            var b = onClick != null ? new Button(onClick) : new Button();
            b.text = label ?? "";
            StyleFlat(b, kind, fill);
            if (fontSize != Body) b.style.fontSize = fontSize;
            return b;
        }

        public static void StyleFlat(Button button, FlatKind kind = FlatKind.Board, Color? fill = null)
        {
            if (button == null) return;
            UseOn(button);
            button.RemoveFromClassList("tc-hex");
            button.RemoveFromClassList("tc-hex--primary");
            button.RemoveFromClassList("tc-hex--secondary");
            button.RemoveFromClassList("tc-hex--currency");
            button.AddToClassList("tc-flat");
            button.EnableInClassList("tc-flat--card", kind == FlatKind.Card);
            button.style.backgroundColor = fill.HasValue ? new StyleColor(fill.Value) : new StyleColor(StyleKeyword.Null);
        }

        // 버튼만 따로 쓰일 때도 스타일이 먹게 (이미 Common 시트가 조상에 있으면 겹쳐도 무해)
        static void UseOn(VisualElement e) => Use(e, "Common");

        // ------------------------------------------------------------ 판

        /// <summary>화면 전체를 덮는 어두운 바탕 (내용은 가운데). 아래 화면을 못 누르게 클릭을 먹음</summary>
        public static VisualElement DimRoot(Color? color = null)
        {
            var dim = new VisualElement { pickingMode = PickingMode.Position };
            Use(dim, "Common");
            dim.AddToClassList("tc-dim");
            if (color.HasValue) dim.style.backgroundColor = color.Value;
            return dim;
        }

        /// <summary>가운데 판 (6px 둥근 모서리 + 1px #3A3F4A 테두리). fill 이 없으면 #1B1E25</summary>
        public static VisualElement Panel(float padH = 24f, float padV = 24f, Color? fill = null)
        {
            var p = new VisualElement();
            Use(p, "Common");
            p.AddToClassList("tc-panel");
            p.style.paddingLeft = padH;
            p.style.paddingRight = padH;
            p.style.paddingTop = padV;
            p.style.paddingBottom = padV;
            if (fill.HasValue) p.style.backgroundColor = fill.Value;
            return p;
        }

        /// <summary>금 구분선 (스타일 가이드 07, T_UI_GoldDivider: 양 끝이 흐려짐). title = 제목 아래 2px 금, 아니면 1px 어두운 금</summary>
        public static VisualElement GoldDivider(bool title = false)
        {
            var line = new VisualElement { pickingMode = PickingMode.Ignore };
            Use(line, "Common");
            line.AddToClassList("tc-divider");
            if (title) line.AddToClassList("tc-divider--title");
            return line;
        }

        /// <summary>세로 줄에 한 칸 추가 (위 여백 + 가로 정렬). AddRow</summary>
        public static T AddRow<T>(VisualElement column, T child, float top, Align align = Align.Center) where T : VisualElement
        {
            if (column == null || child == null) return child;
            child.style.marginTop = top;
            child.style.alignSelf = align;
            column.Add(child);
            return child;
        }

        public static VisualElement Row()
        {
            var v = new VisualElement();
            v.style.flexDirection = FlexDirection.Row;
            v.style.alignItems = Align.Center;
            return v;
        }

        public static VisualElement Column()
        {
            var v = new VisualElement();
            v.style.flexDirection = FlexDirection.Column;
            return v;
        }

        /// <summary>가로로 남는 자리를 채우는 빈칸 (Spacer Fill)</summary>
        public static VisualElement FlexSpacer()
        {
            var v = new VisualElement { pickingMode = PickingMode.Ignore };
            v.style.flexGrow = 1;
            return v;
        }

        // ------------------------------------------------------------ 기타

        /// <summary>보이기 / 숨기기 (Collapsed = display:none)</summary>
        public static void SetShown(VisualElement e, bool shown)
        {
            if (e != null) e.style.display = shown ? DisplayStyle.Flex : DisplayStyle.None;
        }

        public static bool IsShown(VisualElement e) => e != null && e.resolvedStyle.display != DisplayStyle.None;

        /// <summary>요소와 그 안 전부를 마우스가 통과하게 (툴팁 / 보기만 하는 칸)</summary>
        public static void IgnorePicking(VisualElement root)
        {
            root?.Query<VisualElement>().ForEach(e => e.pickingMode = PickingMode.Ignore);
        }

        public static void Padding(VisualElement e, float h, float v)
        {
            if (e == null) return;
            e.style.paddingLeft = h;
            e.style.paddingRight = h;
            e.style.paddingTop = v;
            e.style.paddingBottom = v;
        }

        public static void Border(VisualElement e, float width, Color color)
        {
            if (e == null) return;
            e.style.borderLeftWidth = width;
            e.style.borderRightWidth = width;
            e.style.borderTopWidth = width;
            e.style.borderBottomWidth = width;
            e.style.borderLeftColor = color;
            e.style.borderRightColor = color;
            e.style.borderTopColor = color;
            e.style.borderBottomColor = color;
        }

        public static void Radius(VisualElement e, float r)
        {
            if (e == null) return;
            e.style.borderTopLeftRadius = r;
            e.style.borderTopRightRadius = r;
            e.style.borderBottomLeftRadius = r;
            e.style.borderBottomRightRadius = r;
        }
    }
}
