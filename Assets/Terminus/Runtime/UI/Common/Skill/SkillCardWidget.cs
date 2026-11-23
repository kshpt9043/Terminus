// 스킬 카드 한 장 (언리얼 USkillCardWidget + WBP_SkillCard 300x400). 시작 강화 스킬 고르기 / 보상에서 씀
//  [   이름 (20 금 굵게, 가운데)     ]
//  [        아이콘 96 (가운데)        ]
//  [ 설명 (14 본문)                   ]
//  [ 공격 · 적 1명    스킬 에너지 1   ]
// 카드 전체가 버튼. 호버 #2E333D + 어두운 금 테두리, 눌림 #23272E + 금 테두리
using System;
using Terminus.Art;
using Terminus.Data;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class SkillCardWidget : VisualElement
    {
        public static readonly Vector2 DefaultCardSize = new Vector2(300f, 400f);
        public static readonly Vector2 DefaultIconSize = new Vector2(96f, 96f);

        readonly Label _nameText;
        readonly VisualElement _iconArea;
        readonly Image _icon;
        readonly Label _descText;
        readonly Label _typeText;
        readonly Label _costText;
        bool _enabled = true;

        /// <summary>DT_Skill 행 이름</summary>
        public string SkillRow { get; private set; } = "";

        /// <summary>카드를 누름 (행 이름)</summary>
        public event Action<string> Clicked;

        /// <summary>마우스가 들어옴(true) / 나감(false)</summary>
        public event Action<SkillCardWidget, bool> HoverChanged;

        public SkillCardWidget() : this(DefaultCardSize) { }

        public SkillCardWidget(Vector2 size)
        {
            UiStyle.Use(this, "Common", "Skill");
            AddToClassList("tc-skillcard");
            style.width = size.x;
            style.height = size.y;

            _nameText = UiStyle.WrapText("", 20, UiColor.Gold, TextAnchor.UpperCenter, true);
            _nameText.AddToClassList("tc-skillcard__name");
            Add(_nameText);

            _iconArea = new VisualElement { pickingMode = PickingMode.Ignore };
            _iconArea.AddToClassList("tc-skillcard__iconarea");
            _icon = new Image { pickingMode = PickingMode.Ignore, scaleMode = ScaleMode.ScaleToFit };
            _icon.AddToClassList("tc-skillcard__icon");
            SetIconSize(DefaultIconSize);
            _iconArea.Add(_icon);
            Add(_iconArea);

            _descText = UiStyle.WrapText("", 14, UiColor.TextBody, TextAnchor.UpperLeft);
            _descText.AddToClassList("tc-skillcard__desc");
            Add(_descText);

            var row = UiStyle.Row();
            row.AddToClassList("tc-skillcard__row");
            _typeText = UiStyle.Text("", UiStyle.Aux, UiColor.TextAux, TextAnchor.MiddleLeft);
            _costText = UiStyle.Text("", UiStyle.Aux, UiColor.GoldHover, TextAnchor.MiddleRight, true);
            row.Add(_typeText);
            row.Add(UiStyle.FlexSpacer());
            row.Add(_costText);
            Add(row);

            this.AddManipulator(new Clickable(() =>
            {
                if (_enabled) Clicked?.Invoke(SkillRow);
            }));
            RegisterCallback<PointerEnterEvent>(_ => HoverChanged?.Invoke(this, true));
            RegisterCallback<PointerLeaveEvent>(_ => HoverChanged?.Invoke(this, false));
        }

        /// <summary>DT_Skill 행 이름으로 카드 채우기</summary>
        public void SetSkill(string skillRow)
        {
            SkillRow = skillRow ?? "";
            var row = GameData.FindSkillRow(SkillRow);
            if (row == null)
            {
                _nameText.text = SkillRow;
                _descText.text = "(DT_Skill 에 없는 스킬)";
                _typeText.text = "";
                _costText.text = "";
                _icon.sprite = null;
                UiStyle.SetShown(_icon, false);
                return;
            }

            _nameText.text = row.DisplayName_KR;
            var sprite = GameIcons.FindSkillIcon(row.IconID, row.SkillType.ToString());
            _icon.sprite = sprite;
            UiStyle.SetShown(_icon, sprite != null);
            _typeText.text = UiText.SkillTypeLine(row);
            _costText.text = UiText.SkillCardCostText(row);
            _descText.text = UiText.FixNewlines(row.Description_KR);
        }

        public void SetIconSize(Vector2 size)
        {
            _icon.style.width = size.x;
            _icon.style.height = size.y;
        }

        /// <summary>누를 수 있는지 (버튼 비활성 모양)</summary>
        public void SetCardEnabled(bool enabled)
        {
            _enabled = enabled;
            EnableInClassList("tc-skillcard--disabled", !enabled);
        }

        /// <summary>고른 카드 강조 (금 2px 테두리)</summary>
        public void SetSelected(bool selected) => EnableInClassList("tc-skillcard--selected", selected);
    }
}
