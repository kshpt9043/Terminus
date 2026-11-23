// 스킬 / 유물 칸 + 설명 한 줄 + 버튼 한 개짜리 카드 (언리얼 UItemCardWidget + WBP_ItemCard)
// 상점 진열 칸, 전투 보상의 유물 카드가 같이 씀
//      [ 칸 ]
//   설명 / 단계 (13, 비면 숨김)
//   [ 구매 30 ]  <- 육각 금 버튼 (라벨이 비면 숨김)
//
//   var card = new ItemCardWidget();
//   card.SetItem(true, "RLC_Common_001"); card.SetCaption("표층 유물"); card.SetAction("구매 30", gold >= 30);
//   card.ActionClicked += () => NetBus.Request(new ShopBuyRelicReq { Index = i });
using System;
using Terminus.Data;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    /// <summary>카드 버튼 모양. WBP_ItemCard 는 금 육각(Currency)</summary>
    public enum CardActionStyle
    {
        Currency,   // 구매 (T_UI_Hex_Gold)
        Primary,
        Secondary,
        Flat,       // 평평한 보조 버튼 (판매 / 강화 같은 작은 버튼)
    }

    public sealed class ItemCardWidget : VisualElement
    {
        readonly VisualElement _slotHolder;
        readonly Label _captionText;
        readonly Button _actionButton;
        ItemSlotWidget _slot;
        Color? _actionLabelColor;

        public bool IsRelic { get; private set; }
        public string ItemRow { get; private set; } = "";

        /// <summary>카드 안 칸 (첫 SetItem 때 만들어짐. 그 전엔 null)</summary>
        public ItemSlotWidget Slot => _slot;

        /// <summary>버튼을 누름</summary>
        public event Action ActionClicked;

        /// <summary>내용이 바뀐 뒤 (OnCardUpdated)</summary>
        public event Action<ItemCardWidget> Updated;

        public ItemCardWidget()
        {
            UiStyle.Use(this, "Common");
            AddToClassList("tc-itemcard");

            _slotHolder = new VisualElement { name = "SlotHolder" };
            _slotHolder.AddToClassList("tc-itemcard__slot");
            Add(_slotHolder);

            _captionText = UiStyle.WrapText("", UiStyle.Aux, UiColor.TextAux, TextAnchor.UpperCenter, true);
            _captionText.AddToClassList("tc-itemcard__caption");
            Add(_captionText);
            UiStyle.SetShown(_captionText, false);

            _actionButton = UiStyle.HexButton("", HexKind.Currency, () => ActionClicked?.Invoke(), UiStyle.Aux);
            _actionButton.AddToClassList("tc-itemcard__action");
            Add(_actionButton);
            UiStyle.SetShown(_actionButton, false);
        }

        /// <summary>
        /// 칸 내용. slotSize 가 0 이면 칸 기본 크기 (84x104, 이름이 보이면 100x128).
        /// upgradedSkill = 상점 강화 카드처럼 강화 수치로 툴팁을 보여줄 때
        /// </summary>
        public void SetItem(bool relic, string row, Vector2 slotSize = default, bool showName = true, SkillRow upgradedSkill = null)
        {
            IsRelic = relic;
            ItemRow = row ?? "";
            if (_slot == null)
            {
                _slot = new ItemSlotWidget();
                _slotHolder.Add(_slot);
            }
            if (slotSize != Vector2.zero) _slot.SetSlotSize(slotSize);
            _slot.SetShowName(showName);
            _slot.SetItem(relic, ItemRow, upgradedSkill);
            Updated?.Invoke(this);
        }

        /// <summary>설명 / 단계 ("+1", 유물 설명). 비면 숨김</summary>
        public void SetCaption(string caption)
        {
            UiStyle.SetLine(_captionText, caption);
            Updated?.Invoke(this);
        }

        /// <summary>버튼 라벨 / 누를 수 있는지 / 모양. 라벨이 비면 버튼 숨김</summary>
        public void SetAction(string label, bool enabled, CardActionStyle actionStyle = CardActionStyle.Currency)
        {
            _actionButton.text = label ?? "";
            UiStyle.SetShown(_actionButton, !string.IsNullOrEmpty(label));
            _actionButton.SetEnabled(enabled);
            switch (actionStyle)
            {
                case CardActionStyle.Primary: UiStyle.StyleHex(_actionButton, HexKind.Primary); break;
                case CardActionStyle.Secondary: UiStyle.StyleHex(_actionButton, HexKind.Secondary); break;
                case CardActionStyle.Flat: UiStyle.StyleFlat(_actionButton, FlatKind.Board); break;
                default: UiStyle.StyleHex(_actionButton, HexKind.Currency); break;
            }
            _actionButton.style.color = _actionLabelColor.HasValue ? new StyleColor(_actionLabelColor.Value) : new StyleColor(StyleKeyword.Null);
            Updated?.Invoke(this);
        }

        /// <summary>버튼 글자색만 (상점: 재화 부족이면 경고색). null = 기본</summary>
        public void SetActionLabelColor(Color? color)
        {
            _actionLabelColor = color;
            _actionButton.style.color = color.HasValue ? new StyleColor(color.Value) : new StyleColor(StyleKeyword.Null);
        }

        /// <summary>설명 줄 폭 / 글자 크기 / 색 (width 0 이면 그대로)</summary>
        public void SetCaptionStyle(float width, int fontSize, Color color)
        {
            if (width > 0f) _captionText.style.width = width;
            _captionText.style.fontSize = fontSize;
            _captionText.style.color = color;
        }
    }
}
