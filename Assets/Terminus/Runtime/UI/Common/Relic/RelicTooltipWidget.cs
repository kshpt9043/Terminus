// 유물 정보 툴팁 (언리얼 URelicTooltipWidget + WBP_RelicTooltip, 최대 폭 280)
//  [ 아이콘 48 (있을 때)          ]
//  [ 이름 (17 금 굵게)            ]
//  [ ◆ 표층 유물 (13, 등급 색)    ]
//  [ 설명 (15 본문)               ]
// 보통은 ItemSlotWidget 이 알아서 붙임. 직접: UiTooltip.Attach(el, () => RelicTooltipWidget.For(row))
using Terminus.Art;
using Terminus.Data;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class RelicTooltipWidget : VisualElement
    {
        public const float MaxWidth = 280f;

        readonly Image _icon;
        readonly Label _nameText;
        readonly Label _tierText;
        readonly Label _descText;

        public RelicRow Relic { get; private set; }

        public RelicTooltipWidget()
        {
            UiStyle.Use(this, "Common", "Relic");
            AddToClassList("tc-relictip");
            pickingMode = PickingMode.Ignore;

            _icon = new Image { pickingMode = PickingMode.Ignore, scaleMode = ScaleMode.ScaleToFit };
            _icon.AddToClassList("tc-relictip__icon");
            Add(_icon);
            UiStyle.SetShown(_icon, false);

            _nameText = UiStyle.WrapText("", UiStyle.Subtitle, UiColor.Gold, TextAnchor.UpperLeft, true);
            _nameText.AddToClassList("tc-relictip__name");
            Add(_nameText);

            _tierText = UiStyle.WrapText("", UiStyle.Aux, UiColor.TextAux, TextAnchor.UpperLeft, true);
            _tierText.AddToClassList("tc-relictip__tier");
            Add(_tierText);

            _descText = UiStyle.WrapText("", 15, UiColor.TextBody, TextAnchor.UpperLeft);
            _descText.AddToClassList("tc-relictip__desc");
            Add(_descText);
        }

        public static RelicTooltipWidget For(string relicRow)
        {
            var relic = GameData.FindRelicRow(relicRow);
            if (relic == null) return null;
            var t = new RelicTooltipWidget();
            t.SetRelic(relic);
            return t;
        }

        public void SetRelic(string relicRow) => SetRelic(GameData.FindRelicRow(relicRow));

        public void SetRelic(RelicRow relic)
        {
            if (relic == null) return;
            Relic = relic;

            // 이름은 금색, 등급은 마름모 색으로
            _nameText.text = relic.RelicName;
            _tierText.text = "◆ " + UiText.RelicTierLabel(relic.RelicTier) + " 유물";
            _tierText.style.color = UiText.RelicTierColor(relic.RelicTier);
            _descText.text = UiText.FixNewlines(relic.RelicDesc);

            var sprite = GameIcons.FindRelicIcon(relic.Icon);
            _icon.sprite = sprite;
            UiStyle.SetShown(_icon, sprite != null);
        }
    }
}
