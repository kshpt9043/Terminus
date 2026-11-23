// 선택지 카드 / 버튼 하나 (언리얼 UChoiceCardWidget + WBP_ChoiceCard). 휴식터 선택지, 이벤트 카드, 층 투표 카드, 관전 바 버튼이 같이 씀
//   제목 (20 금 굵게)
//   [칸] (유물 / 스킬 후보일 때, Style.ShowItemSlot)
//   설명 (14 본문)
//   아래 한 줄 (13 흐림)
// 판 #1B1E25 + 1px #3A3F4A, 안쪽 20. 호버 #2E333D + 어두운 금, 눌림 #23272E + 금. 비어 있는 줄은 숨김
// 상태(SetState): Selected 금 2px + 제목 밝은 금 + 아래 줄 초록 / Highlight 밝은 금 2px + #2E333D / Danger 경고 2px
// SetItem 으로 유물 / 스킬을 주면 카드에 올렸을 때 그 설명 툴팁이 뜸
//
//   var c = new ChoiceCardWidget(); c.SetTexts("휴식", "체력을 30% 회복합니다"); c.Chosen += _ => NetBus.Request(...);
using System;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public enum ChoiceCardState
    {
        Normal,
        Selected,   // 내가 고른 것 / 지금 보는 것
        Highlight,  // 결과로 뽑힌 것
        Danger,     // 배신 같은 위험한 선택
    }

    /// <summary>카드 꾸밈. 기본값 = WBP_ChoiceCard (왼쪽 정렬, 안쪽 20, 크기는 내용에 맞춤)</summary>
    public sealed class ChoiceCardStyle
    {
        public Vector2 Size = Vector2.zero;   // 0 이면 내용에 맞춤
        public bool FixedHeight = true;       // false 면 Size.y 는 최소 높이
        public int TitleSize = 20;
        public Color TitleColor = UiColor.Gold;
        public int DescSize = 14;
        public bool CenterText = false;       // WBP 는 왼쪽 정렬, C++ 기본 모양은 가운데
        public float PaddingH = 20f;
        public float PaddingV = 20f;
        public Color? NormalColor = null;     // null = #1B1E25 (호버 / 눌림 색도 테마대로)
        public bool ShowItemSlot = false;     // SetItem 때 칸 아이콘도 보이기 (C++ 기본 모양). WBP 는 툴팁만
    }

    public sealed class ChoiceCardWidget : VisualElement
    {
        readonly Label _titleText;
        readonly ItemSlotWidget _itemSlot;
        readonly Label _descText;
        readonly Label _footerText;
        ChoiceCardStyle _style = new ChoiceCardStyle();
        bool _enabled = true;
        bool _hasItem;

        public string ChoiceId { get; private set; } = "";
        public ChoiceCardState State { get; private set; } = ChoiceCardState.Normal;
        public bool IsChoiceEnabled => _enabled;

        /// <summary>눌림 (비활성이면 안 불림)</summary>
        public event Action<ChoiceCardWidget> Chosen;

        /// <summary>내용 / 상태가 바뀐 뒤 (OnCardUpdated. 아이콘 / 색을 더 꾸미고 싶을 때)</summary>
        public event Action<ChoiceCardWidget> Updated;

        public ChoiceCardWidget(ChoiceCardStyle style = null)
        {
            UiStyle.Use(this, "Common");
            AddToClassList("tc-choice");

            _titleText = UiStyle.WrapText("", 20, UiColor.Gold, TextAnchor.UpperLeft, true);
            _titleText.AddToClassList("tc-choice__title");
            Add(_titleText);

            _itemSlot = new ItemSlotWidget { pickingMode = PickingMode.Ignore, TooltipEnabled = false };
            _itemSlot.AddToClassList("tc-choice__slot");
            _itemSlot.SetShowName(false);
            Add(_itemSlot);
            UiStyle.SetShown(_itemSlot, false);

            _descText = UiStyle.WrapText("", 14, UiColor.TextBody, TextAnchor.UpperLeft);
            _descText.AddToClassList("tc-choice__desc");
            Add(_descText);

            _footerText = UiStyle.WrapText("", UiStyle.Aux, UiColor.TextFaint, TextAnchor.UpperLeft);
            _footerText.AddToClassList("tc-choice__footer");
            Add(_footerText);

            SetTexts("", "", "");

            this.AddManipulator(new Clickable(() =>
            {
                if (_enabled) Chosen?.Invoke(this);
            }));

            ApplyStyle(style ?? new ChoiceCardStyle());
        }

        public void SetTexts(string title, string description, string footer = "")
        {
            UiStyle.SetLine(_titleText, title);
            UiStyle.SetLine(_descText, description);
            UiStyle.SetLine(_footerText, footer);
            Updated?.Invoke(this);
        }

        /// <summary>유물 / 스킬 후보 (row 가 비면 뗌). 카드에 올리면 설명 툴팁, Style.ShowItemSlot 이면 칸도 보임</summary>
        public void SetItem(bool relic, string row)
        {
            _hasItem = !string.IsNullOrEmpty(row);
            if (!_hasItem)
            {
                UiStyle.SetShown(_itemSlot, false);
                UiTooltip.Detach(this);
                Updated?.Invoke(this);
                return;
            }

            _itemSlot.SetItem(relic, row);
            UiStyle.SetShown(_itemSlot, _style.ShowItemSlot);
            if (relic) UiTooltip.Attach(this, () => RelicTooltipWidget.For(row));
            else UiTooltip.Attach(this, () => SkillTooltipWidget.For(row));
            Updated?.Invoke(this);
        }

        public void SetChoiceId(string id)
        {
            ChoiceId = id ?? "";
            Updated?.Invoke(this);
        }

        public void SetState(ChoiceCardState state)
        {
            State = state;
            EnableInClassList("tc-choice--selected", state == ChoiceCardState.Selected);
            EnableInClassList("tc-choice--highlight", state == ChoiceCardState.Highlight);
            EnableInClassList("tc-choice--danger", state == ChoiceCardState.Danger);
            ApplyStateColors();
            Updated?.Invoke(this);
        }

        public void SetChoiceEnabled(bool enabled)
        {
            _enabled = enabled;
            EnableInClassList("tc-choice--disabled", !enabled);
        }

        public void ApplyStyle(ChoiceCardStyle style)
        {
            _style = style ?? new ChoiceCardStyle();

            style = _style;
            this.style.width = style.Size.x > 0f ? new StyleLength(style.Size.x) : new StyleLength(StyleKeyword.Null);
            this.style.height = style.Size.y > 0f && style.FixedHeight ? new StyleLength(style.Size.y) : new StyleLength(StyleKeyword.Null);
            this.style.minHeight = style.Size.y > 0f && !style.FixedHeight ? new StyleLength(style.Size.y) : new StyleLength(StyleKeyword.Null);
            UiStyle.Padding(this, style.PaddingH, style.PaddingV);
            this.style.backgroundColor = style.NormalColor.HasValue ? new StyleColor(style.NormalColor.Value) : new StyleColor(StyleKeyword.Null);

            _titleText.style.fontSize = style.TitleSize;
            _descText.style.fontSize = style.DescSize;
            var align = style.CenterText ? TextAnchor.UpperCenter : TextAnchor.UpperLeft;
            _titleText.style.unityTextAlign = align;
            _descText.style.unityTextAlign = align;
            _footerText.style.unityTextAlign = align;
            _itemSlot.style.alignSelf = style.CenterText ? Align.Center : Align.FlexStart;
            if (_hasItem) UiStyle.SetShown(_itemSlot, style.ShowItemSlot);

            ApplyStateColors();
        }

        // 상태는 테두리(USS) / 글자색(여기)으로
        void ApplyStateColors()
        {
            var title = _style.TitleColor;
            var footer = UiColor.TextFaint;
            switch (State)
            {
                case ChoiceCardState.Selected: title = UiColor.GoldHover; footer = UiColor.Ready; break;
                case ChoiceCardState.Highlight: title = UiColor.GoldHover; break;
            }
            _titleText.style.color = title;
            _footerText.style.color = footer;
        }
    }
}
