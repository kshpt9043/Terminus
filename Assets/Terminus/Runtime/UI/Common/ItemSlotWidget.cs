// 스킬 / 유물 칸 공용 (언리얼 UItemSlotWidget + WBP_SkillSlot. 설정의 ItemSlot 클래스가 WBP_SkillSlot)
// 전투 HUD 스킬 칸, 창고 격자, 보유 유물 줄, 고르기 패널이 모두 이걸 씀
//
//  - 아이콘 (없으면 이름 앞 두 글자) + 이름 (SetShowName 으로 숨김)
//  - 유물 / 스킬: 아이콘을 칸에 맞는 정사각형으로 크게, 이름은 10px. 이름이 보이면 칸을 100x128 이상으로 키움
//  - 판 #262A32(95%) + 1px #3A3F4A 테두리, 위쪽 4px 띠 색: 유물은 등급, 스킬은 공격 / 방어 / 특수
//    호버면 #A8894A 테두리 + #2E333D 바탕, 선택(SetSelected)이면 금 2px 테두리
//  - 마우스를 올리면 툴팁 (스킬 = SkillTooltipWidget, 유물 = RelicTooltipWidget)
//  - 아이콘만 모드(SetIconOnly): 이름 / 테두리 / 바탕을 숨기고 아이콘만 (선택 강조는 보임). 정보는 툴팁으로 (상점)
//  - SetUsable(false) 면 눌리지 않고 흐려짐 (전투 비용 부족, 다른 직업 유물). 빈 칸은 항상 안 눌림
//  - SlotIndex 는 쓰는 쪽이 정하는 번호. 눌리면 Clicked(this)
// 같은 내용을 다시 넣으면 다시 그리지 않음 (전투 HUD 가 자주 부름)
using System;
using Terminus.Art;
using Terminus.Data;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public enum ItemSlotKind
    {
        Empty,
        Skill,
        Relic,
    }

    public sealed class ItemSlotWidget : VisualElement
    {
        /// <summary>WBP_SkillSlot SlotSizeBox 84x104</summary>
        public static readonly Vector2 DefaultSize = new Vector2(84f, 104f);

        /// <summary>이름이 보이는 유물 / 스킬 칸의 최소 크기 (아이콘이 이름에 밀려 작아지지 않게)</summary>
        public static readonly Vector2 NamedMinSize = new Vector2(100f, 128f);

        const float NameReserve = 28f;    // 이름 자리 (작은 글씨 두 줄)
        const float IconPadding = 1f;     // 유물 / 스킬 칸 안쪽 여백
        const int ItemNameFontSize = 10;
        const int EmptyNameFontSize = 12; // WBP NameText 12

        /// <summary>빈 칸 띠 / 바탕 색</summary>
        public static readonly Color EmptyColor = UiColor.Inset;
        /// <summary>선택 테두리 색</summary>
        public static readonly Color SelectedColor = UiColor.Gold;
        /// <summary>WBP Background BrushColor (0.019382, 0.023153, 0.031896, 0.95) = #262A32 95%</summary>
        public static readonly Color BackgroundColor = UiColor.FromLinear(0.019382f, 0.023153f, 0.031896f, 0.95f);

        readonly VisualElement _frame;
        readonly VisualElement _background;
        readonly VisualElement _iconArea;
        readonly Image _icon;
        readonly Label _iconText;
        readonly Label _name;

        Vector2 _requestedSize;
        bool _shownOnce;
        bool _selected;
        bool _usable = true;
        bool _hovered;
        bool _pressed;
        bool _iconOnly;
        bool _showName = true;
        bool _tooltipEnabled = true;
        Color _itemColor = UiColor.Line;
        int _shownSkillBase, _shownSkillStatus;

        SkillTooltipWidget _skillTooltip;
        RelicTooltipWidget _relicTooltip;

        // ------------------------------------------------------------ 공개

        public ItemSlotKind Kind { get; private set; } = ItemSlotKind.Empty;

        /// <summary>DT_Skill / DT_Relic 행 이름 (빈 칸이면 "")</summary>
        public string ItemRow { get; private set; } = "";

        /// <summary>쓰는 쪽이 정하는 번호 (전투 스킬 칸 번호, 강화 칸 번호 등). 기본 -1</summary>
        public int SlotIndex { get; set; } = -1;

        /// <summary>강화 복사본을 넣었으면 그것, 아니면 표의 행 (스킬일 때만)</summary>
        public SkillRow ShownSkill { get; private set; }

        public bool IsSelected => _selected;
        public bool IsUsable => _usable;
        public bool IsIconOnly => _iconOnly;

        /// <summary>칸을 누르고 있는 중인지 (전투 HUD 의 스킬 드래그용)</summary>
        public bool IsPressed => _pressed && _usable && Kind != ItemSlotKind.Empty;

        /// <summary>눌림 (빈 칸 / 못 쓰는 칸은 안 불림)</summary>
        public event Action<ItemSlotWidget> Clicked;

        /// <summary>마우스가 들어옴(true) / 나감(false)</summary>
        public event Action<ItemSlotWidget, bool> HoverChanged;

        /// <summary>툴팁 켜기 (기본 켬). 끄면 쓰는 쪽이 따로 보여줄 때</summary>
        public bool TooltipEnabled
        {
            get => _tooltipEnabled;
            set => _tooltipEnabled = value;
        }

        public ItemSlotWidget() : this(DefaultSize) { }

        public ItemSlotWidget(Vector2 size)
        {
            UiStyle.Use(this, "Common");
            AddToClassList("tc-slot");
            _requestedSize = size;

            _frame = new VisualElement { name = "Frame", pickingMode = PickingMode.Ignore };
            _frame.AddToClassList("tc-slot__frame");
            Add(_frame);

            _background = new VisualElement { name = "Background", pickingMode = PickingMode.Ignore };
            _background.AddToClassList("tc-slot__bg");
            _frame.Add(_background);

            _iconArea = new VisualElement { name = "IconArea", pickingMode = PickingMode.Ignore };
            _iconArea.AddToClassList("tc-slot__iconarea");
            _background.Add(_iconArea);

            _icon = new Image { name = "Icon", pickingMode = PickingMode.Ignore, scaleMode = ScaleMode.ScaleToFit };
            _icon.AddToClassList("tc-slot__icon");
            _iconArea.Add(_icon);

            _iconText = new Label { name = "IconText", pickingMode = PickingMode.Ignore };
            _iconText.AddToClassList("tc-slot__icontext");
            _iconArea.Add(_iconText);

            _name = new Label { name = "NameText", pickingMode = PickingMode.Ignore };
            _name.AddToClassList("tc-slot__name");
            _background.Add(_name);

            _iconArea.RegisterCallback<GeometryChangedEvent>(_ => FitIcon());

            this.AddManipulator(new Clickable(HandleClicked));
            RegisterCallback<PointerEnterEvent>(_ => SetHovered(true));
            RegisterCallback<PointerLeaveEvent>(_ => { _pressed = false; SetHovered(false); });
            // TrickleDown: Clickable 이 전파를 막아도 눌림 상태는 받게
            RegisterCallback<PointerDownEvent>(e => { if (e.button == 0) _pressed = true; }, TrickleDown.TrickleDown);
            RegisterCallback<PointerUpEvent>(e => { if (e.button == 0) _pressed = false; }, TrickleDown.TrickleDown);
            RegisterCallback<PointerCaptureOutEvent>(_ => _pressed = false);

            UiTooltip.Attach(this, BuildTooltip);

            SetEmpty();
        }

        // ------------------------------------------------------------ 내용

        public void SetEmpty()
        {
            if (_shownOnce && Kind == ItemSlotKind.Empty) return;
            _shownOnce = true;

            Kind = ItemSlotKind.Empty;
            ItemRow = "";
            ShownSkill = null;
            _itemColor = EmptyColor;
            ShowIcon(null, "");
            ApplyKindLayout();
            ApplyFrameColor();
            ApplyUsable();
            if (UiTooltip.CurrentTarget == this) UiTooltip.Hide();
        }

        /// <summary>스킬. upgraded = 강화 단계를 더한 복사본 (전투 칸 / 상점 강화 카드). 툴팁 수치와 설명이 그 값으로</summary>
        public void SetSkill(string skillRow, SkillRow upgraded = null)
        {
            var skill = string.IsNullOrEmpty(skillRow) ? null : GameData.FindSkillRow(skillRow);
            var shown = upgraded ?? skill;
            if (_shownOnce && Kind == ItemSlotKind.Skill && ItemRow == skillRow && shown != null
                && shown.BaseValue == _shownSkillBase && shown.StatusValue == _shownSkillStatus) return;
            if (skill == null)
            {
                SetEmpty();
                return;
            }
            _shownOnce = true;

            Kind = ItemSlotKind.Skill;
            ItemRow = skillRow;
            ShownSkill = shown;
            _itemColor = UiText.SkillTypeColor(skill.SkillType);
            ShowIcon(GameIcons.FindSkillIcon(skill.IconID, skill.SkillType.ToString()), skill.DisplayName_KR);
            ApplyKindLayout();

            _skillTooltip ??= new SkillTooltipWidget();
            _skillTooltip.SetSkill(shown, skill);
            _shownSkillBase = shown.BaseValue;
            _shownSkillStatus = shown.StatusValue;

            ApplyFrameColor();
            ApplyUsable();
        }

        public void SetRelic(string relicRow)
        {
            if (_shownOnce && Kind == ItemSlotKind.Relic && ItemRow == relicRow) return;

            var relic = string.IsNullOrEmpty(relicRow) ? null : GameData.FindRelicRow(relicRow);
            if (relic == null)
            {
                SetEmpty();
                return;
            }
            _shownOnce = true;

            Kind = ItemSlotKind.Relic;
            ItemRow = relicRow;
            ShownSkill = null;
            _itemColor = UiText.RelicTierColor(relic.RelicTier);
            ShowIcon(GameIcons.FindRelicIcon(relic.Icon), relic.RelicName);
            ApplyKindLayout();

            _relicTooltip ??= new RelicTooltipWidget();
            _relicTooltip.SetRelic(relic);

            ApplyFrameColor();
            ApplyUsable();
        }

        /// <summary>유물이면 SetRelic, 아니면 SetSkill</summary>
        public void SetItem(bool relic, string row, SkillRow upgradedSkill = null)
        {
            if (relic) SetRelic(row);
            else SetSkill(row, upgradedSkill);
        }

        // ------------------------------------------------------------ 상태 / 모양

        /// <summary>선택 / 대상 고르는 중 강조 (금 2px 테두리)</summary>
        public void SetSelected(bool selected)
        {
            if (_selected == selected) return;
            _selected = selected;
            ApplyFrameColor();
        }

        /// <summary>누를 수 있는지 (빈 칸은 무시하고 항상 안 눌림)</summary>
        public void SetUsable(bool usable)
        {
            if (_usable == usable) return;
            _usable = usable;
            ApplyUsable();
            ApplyFrameColor();
        }

        /// <summary>이름 줄 보이기 (유물 줄처럼 아이콘만 보여줄 때 false)</summary>
        public void SetShowName(bool show)
        {
            _showName = show;
            ApplyKindLayout();
        }

        /// <summary>칸 크기. 이름이 보이는 유물 / 스킬 칸은 NamedMinSize 보다 작아지지 않음</summary>
        public void SetSlotSize(Vector2 size)
        {
            _requestedSize = size;
            ApplyKindLayout();
        }

        /// <summary>아이콘만 (이름 / 테두리 / 바탕 숨김, 선택 강조는 보임)</summary>
        public void SetIconOnly(bool iconOnly)
        {
            _iconOnly = iconOnly;
            ApplyKindLayout();
            ApplyFrameColor();
        }

        // ------------------------------------------------------------ 내부

        bool IsItem => Kind == ItemSlotKind.Relic || Kind == ItemSlotKind.Skill;
        bool NameShown => _showName && !_iconOnly && IsItem;

        void ShowIcon(Sprite sprite, string displayName)
        {
            _icon.sprite = sprite;
            _icon.style.visibility = sprite != null ? Visibility.Visible : Visibility.Hidden;

            var shortName = string.IsNullOrEmpty(displayName) ? "" : displayName.Substring(0, Math.Min(2, displayName.Length));
            _iconText.text = shortName;
            UiStyle.SetShown(_iconText, sprite == null && shortName.Length > 0);

            _name.text = displayName ?? "";
        }

        void ApplyKindLayout()
        {
            bool item = IsItem;
            bool nameShown = NameShown;

            UiStyle.SetShown(_name, nameShown);
            _name.style.fontSize = item ? ItemNameFontSize : EmptyNameFontSize;
            _name.style.height = item ? new StyleLength(NameReserve) : new StyleLength(StyleKeyword.Auto);
            _name.style.marginTop = item ? IconPadding : 4f;

            var size = _requestedSize;
            if (item && nameShown) size = Vector2.Max(size, NamedMinSize);
            style.width = size.x;
            style.height = size.y;

            // 안쪽 여백: 유물 / 스킬은 거의 없앰, 빈 칸은 WBP 값 (위아래 4)
            float padH = item ? IconPadding : 0f, padV = item ? IconPadding : 4f;
            UiStyle.Padding(_background, padH, padV);

            FitIcon();
        }

        // 아이콘을 실제 자리에 맞는 정사각형으로 (언리얼 NativeTick 의 맞춤)
        void FitIcon()
        {
            float w = _iconArea.resolvedStyle.width, h = _iconArea.resolvedStyle.height;
            if (float.IsNaN(w) || float.IsNaN(h) || w < 1f || h < 1f) return;
            float edge = Mathf.Floor(Mathf.Min(w, h));
            if (!IsItem) edge = Mathf.Min(edge, 32f);   // 빈 칸 기본 아이콘 크기
            if (edge < 8f) return;
            if (Mathf.Abs(_icon.resolvedStyle.width - edge) >= 1f || Mathf.Abs(_icon.resolvedStyle.height - edge) >= 1f)
            {
                _icon.style.width = edge;
                _icon.style.height = edge;
            }
        }

        void ApplyFrameColor()
        {
            bool empty = Kind == ItemSlotKind.Empty;
            bool hover = _hovered && !empty && _usable;

            // 테두리 판의 바탕색이 위쪽 띠 (안쪽 판이 나머지를 덮음). 아이콘만 모드면 띠 없이 고를 때(강조) 테두리만
            var band = _iconOnly ? UiColor.Clear : (empty ? EmptyColor : _itemColor);
            var outline = _selected ? SelectedColor
                : _iconOnly ? UiColor.Clear
                : hover ? UiColor.GoldPressed : UiColor.Line;
            float width = _selected ? 2f : 1f;

            _frame.style.backgroundColor = band;
            UiStyle.Border(_frame, width, outline);
            _background.style.backgroundColor = _iconOnly ? UiColor.Clear : empty ? EmptyColor : hover ? UiColor.Disabled : BackgroundColor;
        }

        void ApplyUsable()
        {
            bool canPress = Kind != ItemSlotKind.Empty && _usable;
            EnableInClassList("tc-slot--empty", Kind == ItemSlotKind.Empty);
            EnableInClassList("tc-slot--unusable", Kind != ItemSlotKind.Empty && !_usable);
            EnableInClassList("tc-slot--pressable", canPress);
        }

        void SetHovered(bool hovered)
        {
            if (_hovered == hovered) return;
            _hovered = hovered;
            ApplyFrameColor();
            HoverChanged?.Invoke(this, hovered);
        }

        void HandleClicked()
        {
            if (Kind == ItemSlotKind.Empty || !_usable) return;
            Clicked?.Invoke(this);
        }

        VisualElement BuildTooltip()
        {
            if (!_tooltipEnabled) return null;
            switch (Kind)
            {
                case ItemSlotKind.Skill: return _skillTooltip;
                case ItemSlotKind.Relic: return _relicTooltip;
                default: return null;
            }
        }
    }
}
