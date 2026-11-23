// 화면에 보이는 이름 / 라벨 / 종류 색 (언리얼 Widgets/Common/TerminusUIText 이식)
// 용어를 바꿀 땐 여기만 고치면 됨. 스킬 / 유물 이름 찾기는 GameData.GetSkillName / GetRelicName
using System.Globalization;
using Terminus.Data;
using Terminus.Map;
using UnityEngine;

namespace Terminus.UI.Common
{
    public static class UiText
    {
        public static string SkillTypeLabel(SkillType type)
        {
            switch (type)
            {
                case SkillType.Attack: return "공격";
                case SkillType.Defense: return "방어";
                default: return "특수";
            }
        }

        public static string TargetTypeLabel(TargetType type)
        {
            switch (type)
            {
                case TargetType.SingleEnemy: return "적 1명";
                case TargetType.AllEnemies: return "적 전체";
                case TargetType.SingleAlly: return "아군 1명";
                case TargetType.AllAllies: return "아군 전체";
                default: return "자신";
            }
        }

        public static string SkillOwnerLabel(SkillOwner owner)
        {
            switch (owner)
            {
                case SkillOwner.Fighter: return "무도가";
                case SkillOwner.Engineer: return "마도 공학자";
                case SkillOwner.Paladin: return "성기사";
                case SkillOwner.Assassin: return "암살자";
                case SkillOwner.Event: return "이벤트";
                case SkillOwner.Quest: return "퀘스트";
                default: return "공용";
            }
        }

        public static string RelicTierLabel(RelicTier tier)
        {
            switch (tier)
            {
                case RelicTier.Basic: return "기본";
                case RelicTier.Upgrade: return "업그레이드";
                case RelicTier.Mid: return "중층";
                case RelicTier.Deep: return "심층";
                default: return "표층";
            }
        }

        public static string RoomTypeLabel(RoomType type)
        {
            switch (type)
            {
                case RoomType.MONSTER: return "몬스터";
                case RoomType.GUARDIAN: return "가디언";
                case RoomType.BOSS: return "보스";
                case RoomType.BREAK: return "휴식터";
                case RoomType.EVENT: return "이벤트";
                case RoomType.STORE: return "상점";
                case RoomType.QUEST: return "퀘스트";
                default: return "방";
            }
        }

        /// <summary>"1,234 G"</summary>
        public static string GoldText(int gold) => gold.ToString("N0", CultureInfo.InvariantCulture) + " G";

        /// <summary>칸 / 툴팁 띠 색: 유물 등급 (스타일 가이드 층 등급: 표층 흐림 / 중층 금 / 심층 경고)</summary>
        public static Color RelicTierColor(RelicTier tier)
        {
            switch (tier)
            {
                case RelicTier.Basic: return UiColor.Line;
                case RelicTier.Upgrade: return UiColor.GoldHover;
                case RelicTier.Mid: return UiColor.Gold;
                case RelicTier.Deep: return UiColor.Alert;
                default: return UiColor.TextFaint;
            }
        }

        /// <summary>스킬 종류 색: 공격 경고색 / 방어 청록 / 특수 금</summary>
        public static Color SkillTypeColor(SkillType type)
        {
            switch (type)
            {
                case SkillType.Attack: return UiColor.Alert;
                case SkillType.Defense: return UiColor.DefenseTeal;
                default: return UiColor.Gold;
            }
        }

        /// <summary>"공격 · 적 1명"</summary>
        public static string SkillTypeLine(SkillRow skill) =>
            skill == null ? "" : SkillTypeLabel(skill.SkillType) + " · " + TargetTypeLabel(skill.TargetType);

        /// <summary>툴팁 비용 줄: "에너지 1  ·  스킬 에너지 2" / "비용 없음" (SkillTooltipWidget)</summary>
        public static string SkillCostText(SkillRow skill)
        {
            if (skill == null) return "";
            var cost = "";
            if (skill.EnergyCost > 0) cost = "에너지 " + skill.EnergyCost;
            if (skill.SkillEnergyCost > 0) cost += (cost.Length == 0 ? "" : "  ·  ") + "스킬 에너지 " + skill.SkillEnergyCost;
            return cost.Length == 0 ? "비용 없음" : cost;
        }

        /// <summary>카드 비용 줄: "스킬 에너지 1  ·  에너지 1" (SkillCardWidget. 스킬 에너지가 먼저, 0 이어도 표시)</summary>
        public static string SkillCardCostText(SkillRow skill)
        {
            if (skill == null) return "";
            var cost = "스킬 에너지 " + skill.SkillEnergyCost;
            if (skill.EnergyCost > 0) cost += "  ·  에너지 " + skill.EnergyCost;
            return cost;
        }

        /// <summary>DT 설명 중 줄바꿈이 "\n" 글자 그대로 들어간 것을 진짜 줄바꿈으로</summary>
        public static string FixNewlines(string s) => string.IsNullOrEmpty(s) ? "" : s.Replace("\\n", "\n");
    }
}
