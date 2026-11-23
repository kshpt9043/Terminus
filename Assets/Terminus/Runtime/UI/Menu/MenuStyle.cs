// 메뉴 화면들(메인 메뉴 / 방 목록 / 주점 / 거점 / 창고 / 이어하기 / 정산) 공용 도우미
//  - USS: Assets/Terminus/UI/Menu/Resources/UI/Menu/<이름>.uss  ->  MenuStyle.Use(element, "Menu")
//  - KSH WBP(메인 메뉴 / 주점 / 방 목록)의 버튼은 T_UI_Btn_Hex 한 장을 색만 바꿔(TintColor) 씀 -> PlateButton
//  - JSY WBP(거점 / 창고 / 이어하기 / 정산)의 버튼은 C++ MakeFlatButtonStyle 그대로 -> UiStyle.FlatButton(Card)
//  - 덤프의 글자 크기는 공용 부품과 같은 규칙(언리얼 숫자 = px, 소수는 반올림)
using System;
using System.Collections.Generic;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    /// <summary>T_UI_Btn_Hex 판 색 (WBP TintColor. 선형 -> sRGB 로 바꾼 값은 Menu.uss)</summary>
    public enum PlateKind
    {
        Red,    // 던전 입장 / 준비 / 확인 / 열기 (#A8322C)
        Amber,  // 이어하기 (#A88100)
        Teal,   // 주점 / 출발 / 주점 열기 (#3F7F86)
        Gray,   // 준비 취소 (언리얼 CancelStyle #3A3F4A)
    }

    public static class MenuStyle
    {
        const string SheetFolder = "UI/Menu/";
        static readonly Dictionary<string, StyleSheet> Sheets = new Dictionary<string, StyleSheet>();

        // WBP 덤프 공통 색 (선형 -> sRGB)
        public static readonly Color WindowBackground = UiColor.FromLinear(0.006995f, 0.005605f, 0.004391f, 0.98f);   // 거점 / 창고 배경

        /// <summary>메뉴 USS 붙이기 ("Menu", "Tavern", "MenuWindows"). 공용 부품 시트(Common)도 같이</summary>
        public static void Use(VisualElement element, params string[] sheetNames)
        {
            if (element == null) return;
            UiStyle.Use(element, "Common");
            foreach (var name in sheetNames)
            {
                if (!Sheets.TryGetValue(name, out var sheet))
                {
                    sheet = Resources.Load<StyleSheet>(SheetFolder + name);
                    if (sheet == null) Debug.LogWarning("[UI] USS 없음: Resources/" + SheetFolder + name + " (Assets/Terminus/UI/Menu/Resources/UI/Menu)");
                    Sheets[name] = sheet;
                }
                if (sheet != null && !element.styleSheets.Contains(sheet)) element.styleSheets.Add(sheet);
            }
        }

        // ------------------------------------------------------------ 버튼

        /// <summary>T_UI_Btn_Hex 판 버튼 (양 끝 20% 는 늘이지 않음, 눌리면 글자 3px 내려감). 크기는 부른 쪽이</summary>
        public static Button PlateButton(string label, PlateKind kind, Action onClick = null, int fontSize = 27)
        {
            var b = onClick != null ? new Button(onClick) : new Button();
            b.text = label ?? "";
            b.AddToClassList("tm-plate");
            SetPlateKind(b, kind);
            b.style.fontSize = fontSize;
            return b;
        }

        public static void SetPlateKind(Button b, PlateKind kind)
        {
            if (b == null) return;
            b.EnableInClassList("tm-plate--red", kind == PlateKind.Red);
            b.EnableInClassList("tm-plate--amber", kind == PlateKind.Amber);
            b.EnableInClassList("tm-plate--teal", kind == PlateKind.Teal);
            b.EnableInClassList("tm-plate--gray", kind == PlateKind.Gray);
        }

        /// <summary>바탕 없는 글자 버튼 (WBP DrawAs=NoDrawType). color 가 없으면 흐린 글자 + 호버 금 (게임 종료)</summary>
        public static Button TextButton(string label, Action onClick, int fontSize, Color? color = null)
        {
            var b = onClick != null ? new Button(onClick) : new Button();
            b.text = label ?? "";
            b.AddToClassList("tm-textbtn");
            if (color.HasValue) b.style.color = color.Value;
            else b.AddToClassList("tm-textbtn--hover-gold");   // 인라인 색은 :hover 를 이기므로 클래스로
            b.style.fontSize = fontSize;
            return b;
        }

        /// <summary>JSY WBP 의 평평한 버튼 (#262A32 판, 6px 모서리, 호버 금 테두리, 안쪽 24 / 10)</summary>
        public static Button FlatButton(string label, Action onClick = null, int fontSize = UiStyle.Body)
        {
            return UiStyle.FlatButton(label, onClick, fontSize, FlatKind.Card);
        }

        /// <summary>둥근 판 (WBP Border DrawAs=RoundedBox: #1B1E25 + 테두리 #3A3F4A)</summary>
        public static VisualElement RoundPanel(float borderWidth = 2f, float radius = 6f, Color? fill = null)
        {
            var p = new VisualElement();
            p.style.backgroundColor = fill ?? UiColor.Board;
            UiStyle.Border(p, borderWidth, UiColor.Line);
            UiStyle.Radius(p, radius);
            return p;
        }

        /// <summary>화면 전체 덮개 (position absolute 0 0 0 0)</summary>
        public static VisualElement FullScreen(Color? fill = null, PickingMode picking = PickingMode.Position)
        {
            var v = new VisualElement { pickingMode = picking };
            v.AddToClassList("tm-fill");
            if (fill.HasValue) v.style.backgroundColor = fill.Value;
            return v;
        }

        /// <summary>글자 (UiStyle.Text 와 같지만 왼쪽 정렬이 기본)</summary>
        public static Label Text(string text, int size, Color color, bool bold = false, TextAnchor align = TextAnchor.MiddleLeft)
        {
            return UiStyle.Text(text, size, color, align, bold);
        }

        public static Label Wrap(string text, int size, Color color, bool bold = false, TextAnchor align = TextAnchor.UpperLeft)
        {
            return UiStyle.WrapText(text, size, color, align, bold);
        }

        // ------------------------------------------------------------ 입력

        /// <summary>한 줄 입력칸 (어두운 판 + 본문색 글자. 언리얼 PaintInputText). hint = 비었을 때 흐린 안내</summary>
        public static TextField Input(string value, string hint, int maxLength, bool password = false)
        {
            var f = new TextField { value = value ?? "", maxLength = maxLength, isPasswordField = password };
            f.AddToClassList("tm-input");
            if (password) f.maskChar = '*';
            if (!string.IsNullOrEmpty(hint)) f.textEdition.placeholder = hint;
            return f;
        }

        /// <summary>체크 칸 + 글자 (WBP CheckBox 안에 TextBlock)</summary>
        public static Toggle Check(string label, bool value, int fontSize, Color color)
        {
            var t = new Toggle { value = value, text = label ?? "" };
            t.AddToClassList("tm-check");
            var text = t.Q<Label>(className: "unity-toggle__text");
            if (text != null)
            {
                text.style.fontSize = fontSize;
                text.style.color = color;
            }
            return t;
        }

        // ------------------------------------------------------------ 기타

        /// <summary>그 요소에 Esc 키를 붙임 (언리얼 EscapeStack 자리. 포커스가 이 안에 있을 때)</summary>
        public static void OnEscape(VisualElement e, Action action)
        {
            e.focusable = true;
            e.RegisterCallback<KeyDownEvent>(ev =>
            {
                if (ev.keyCode != KeyCode.Escape) return;
                ev.StopPropagation();
                action?.Invoke();
            }, TrickleDown.TrickleDown);
        }

        /// <summary>텍스처 바탕 (Resources 경로, 없으면 빈 바탕)</summary>
        public static void Background(VisualElement e, string unrealPath)
        {
            UiTextures.SetBackground(e, UiTextures.FromUnrealPath(unrealPath));
        }
    }
}
