// 스킬 정보 툴팁 (언리얼 USkillTooltipWidget + WBP_SkillTooltip)
//  [ 이름 (17 금 굵게)                ]  [ 상태이상 창 (거는 상태가 있을 때만) ]
//  [ 설명 (15 본문)                    ]  [ [해로움] 허약(얼음)                  ]
//  [ 공격 · 적 1명        에너지 1     ]  [ 수치만큼 공격 스텟이 하락한다.       ]
// 강화 복사본을 주면 설명의 "(5+공격)" -> "(6+공격)" 처럼 숫자를 바꾸고, 상태 수치 강화는 줄을 덧붙임
//
// 보통은 ItemSlotWidget / ChoiceCardWidget 이 알아서 붙임. 직접 쓸 때: UiTooltip.Attach(el, () => SkillTooltipWidget.For(row))
using System.Collections.Generic;
using System.Text;
using Terminus.Combat;
using Terminus.Data;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class SkillTooltipWidget : VisualElement
    {
        /// <summary>본 판 폭 (설명이 길면 줄바꿈)</summary>
        public const float DefaultWidth = 320f;

        readonly VisualElement _main;
        readonly Label _nameText;
        readonly Label _descText;
        readonly Label _typeText;
        readonly Label _costText;
        readonly VisualElement _statusPanel;
        readonly Label _statusText;

        public SkillRow Skill { get; private set; }

        public SkillTooltipWidget()
        {
            UiStyle.Use(this, "Common", "Skill");
            AddToClassList("tc-skilltip");
            pickingMode = PickingMode.Ignore;

            _main = new VisualElement();
            _main.AddToClassList("tc-skilltip__main");
            Add(_main);

            _nameText = UiStyle.WrapText("", UiStyle.Subtitle, UiColor.Gold, TextAnchor.UpperLeft, true);
            _nameText.AddToClassList("tc-skilltip__name");
            _main.Add(_nameText);

            _descText = UiStyle.WrapText("", 15, UiColor.TextBody, TextAnchor.UpperLeft);
            _descText.AddToClassList("tc-skilltip__desc");
            _main.Add(_descText);

            var row = UiStyle.Row();
            row.AddToClassList("tc-skilltip__row");
            _typeText = UiStyle.Text("", UiStyle.Aux, UiColor.TextAux, TextAnchor.MiddleLeft);
            _costText = UiStyle.Text("", UiStyle.Aux, UiColor.TextAux, TextAnchor.MiddleRight);
            row.Add(_typeText);
            row.Add(UiStyle.FlexSpacer());
            row.Add(_costText);
            _main.Add(row);

            _statusPanel = new VisualElement();
            _statusPanel.AddToClassList("tc-skilltip__status");
            _statusText = UiStyle.WrapText("", 14, UiColor.TextBody, TextAnchor.UpperLeft);
            _statusText.AddToClassList("tc-skilltip__statustext");
            _statusPanel.Add(_statusText);
            Add(_statusPanel);
            UiStyle.SetShown(_statusPanel, false);
        }

        /// <summary>표의 행 이름으로. upgraded = 강화 복사본 (없으면 표 그대로)</summary>
        public static SkillTooltipWidget For(string skillRow, SkillRow upgraded = null)
        {
            var original = GameData.FindSkillRow(skillRow);
            if (original == null) return null;
            var t = new SkillTooltipWidget();
            t.SetSkill(upgraded ?? original, original);
            return t;
        }

        public void SetSkill(string skillRow, SkillRow upgraded = null)
        {
            var original = GameData.FindSkillRow(skillRow);
            if (original != null) SetSkill(upgraded ?? original, original);
        }

        /// <summary>shown = 보여줄 수치 (강화 복사본), original = 강화 전 표의 행 (다르면 설명 숫자를 바꿈)</summary>
        public void SetSkill(SkillRow shown, SkillRow original = null)
        {
            if (shown == null) return;
            Skill = shown;

            _nameText.text = shown.DisplayName_KR;
            _typeText.text = UiText.SkillTypeLine(shown);
            _costText.text = UiText.SkillCostText(shown);
            _descText.text = BuildDescription(shown, original);

            var lines = BuildStatusLines(shown);
            _statusText.text = lines;
            UiStyle.SetShown(_statusPanel, lines.Length > 0);
        }

        /// <summary>설명 (줄바꿈 고침 + 강화 숫자 바꿈 + "(강화: 상태 수치 +N)")</summary>
        public static string BuildDescription(SkillRow shown, SkillRow original)
        {
            if (shown == null) return "";
            var desc = UiText.FixNewlines(shown.Description_KR);
            // 강화로 BaseValue 가 오르면 설명의 "(5+공격)" -> "(6+공격)" (11-04 피드백: 텍스트 수치도 바뀌게)
            if (original != null && original.BaseValue != shown.BaseValue)
            {
                desc = desc.Replace("(" + original.BaseValue + "+", "(" + shown.BaseValue + "+");
                desc = desc.Replace("(" + original.BaseValue + ")", "(" + shown.BaseValue + ")");
            }
            // 훈련소 강화가 상태 수치에 붙은 기본 특수 스킬은 문구 대신 줄을 덧붙임
            if (original != null && original.StatusValue != shown.StatusValue)
            {
                desc += "\n(강화: 상태 수치 +" + (shown.StatusValue - original.StatusValue) + ")";
            }
            return desc;
        }

        /// <summary>상태이상 창 글자: "[해로움] 허약(얼음)\n수치만큼 공격 스텟이 하락한다." 를 상태마다 빈 줄로 구분</summary>
        public static string BuildStatusLines(SkillRow skill)
        {
            if (skill == null) return "";
            List<StatusEffect> statuses;
            try { statuses = SkillExecutor.GetAppliedStatuses(skill); }
            catch { return ""; }

            var sb = new StringBuilder();
            foreach (var type in statuses)
            {
                var desc = StatusInfo.GetDescription(type);
                if (string.IsNullOrEmpty(desc)) continue;
                if (sb.Length > 0) sb.Append("\n\n");
                sb.Append(StatusInfo.IsDebuff(type) ? "[해로움] " : "[이로움] ");
                sb.Append(StatusInfo.GetDisplayName(type));
                sb.Append('\n');
                sb.Append(desc);
            }
            return sb.ToString();
        }
    }
}
