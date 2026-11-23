// 거점(직업의 탑) 강화 (언리얼 Data/UpgradeTypes.h/.cpp 이식)
//  - 연무장: 기본 스테이터스 강화 (StatUpgrade)
//  - 훈련소: 기본 스킬 3개 강화 (BaseValue +1, BaseValue 가 0 이면 StatusValue +1)
// 런을 시작할 때(BeginRun) 고른 직업의 강화가 런 스텟에 더해짐
using System;
using System.Collections.Generic;

namespace Terminus.Data
{
    // 연무장 강화 항목. 순서 = 화면 순서 = ClassUpgrades.StatLevels 인덱스 (뒤에만 추가할 것)
    public enum StatUpgrade
    {
        Health,
        Attack,
        Defense,
        Evasion,
        Energy,
        SkillEnergy,    // 전투 시작 때 갖고 시작하는 스킬 에너지
        RelicCapacity,
        PickupSlots,    // 강화 스킬 칸
    }

    public static class RelicCapacity
    {
        public const int Base = 6;
        public const int Max = 15;
    }

    [System.Serializable]
    public sealed class StatUpgradeRule
    {
        public int MaxLevel = 5;
        public int BaseCost = 1000;
        public int CostStep = 500;
        public int AmountPerLevel = 1;

        public StatUpgradeRule() { }

        public StatUpgradeRule(int maxLevel, int baseCost, int costStep, int amount)
        {
            MaxLevel = maxLevel;
            BaseCost = baseCost;
            CostStep = costStep;
            AmountPerLevel = amount;
        }
    }

    // 직업 하나의 강화 단계. 프로필에 저장되고, 멀티에선 손님이 서버에 알려 줌
    [System.Serializable]
    public sealed class ClassUpgrades
    {
        public const int NumBasicSkills = 3;
        public static readonly int StatCount = Enum.GetValues(typeof(StatUpgrade)).Length;

        public CharacterClass Class = CharacterClass.Fighter;
        // 연무장. 인덱스 = StatUpgrade
        public List<int> StatLevels = new List<int>();
        // 훈련소. 인덱스 = 기본 스킬 칸 (0~2, 전투 HUD 기본 칸 순서와 같음)
        public List<int> SkillLevels = new List<int>();

        public int GetStatLevel(StatUpgrade stat)
        {
            int i = (int)stat;
            return i >= 0 && i < StatLevels.Count ? StatLevels[i] : 0;
        }

        public int GetSkillLevel(int skillIndex)
        {
            return skillIndex >= 0 && skillIndex < SkillLevels.Count ? SkillLevels[skillIndex] : 0;
        }

        public ClassUpgrades Clone()
        {
            return new ClassUpgrades { Class = Class, StatLevels = new List<int>(StatLevels), SkillLevels = new List<int>(SkillLevels) };
        }
    }

    /// <summary>강화 비용 / 한도 / 증가량. 언리얼 Project Settings > Terminus Upgrades 기본값 (기획 미정이라 임시)</summary>
    [System.Serializable]
    public sealed class UpgradeSettings
    {
        public static UpgradeSettings Default { get; } = new UpgradeSettings();

        public readonly Dictionary<StatUpgrade, StatUpgradeRule> StatRules = new Dictionary<StatUpgrade, StatUpgradeRule>
        {
            { StatUpgrade.Health,        new StatUpgradeRule(10, 1000,  500, 5) },
            { StatUpgrade.Attack,        new StatUpgradeRule(5,  1000,  500, 1) },
            { StatUpgrade.Defense,       new StatUpgradeRule(5,  1000,  500, 1) },
            { StatUpgrade.Evasion,       new StatUpgradeRule(3,  3000, 1500, 1) },
            { StatUpgrade.Energy,        new StatUpgradeRule(2,  3000, 1500, 1) },
            { StatUpgrade.SkillEnergy,   new StatUpgradeRule(3,  3000, 1500, 1) },
            // 유물 칸: 기본 6 -> 최대 15. 한 단계 +1, 9단계
            { StatUpgrade.RelicCapacity, new StatUpgradeRule(RelicCapacity.Max - RelicCapacity.Base, 1000, 500, 1) },
            { StatUpgrade.PickupSlots,   new StatUpgradeRule(2, 10000, 5000, 1) },
        };

        // 훈련소 (기본 스킬 3개 공통)
        public int SkillMaxLevel = 3;
        public int SkillBaseCost = 5000;
        public int SkillCostStep = 0;

        public static string GetStatUpgradeName(StatUpgrade stat)
        {
            switch (stat)
            {
                case StatUpgrade.Health: return "체력";
                case StatUpgrade.Attack: return "공격";
                case StatUpgrade.Defense: return "방어";
                case StatUpgrade.Evasion: return "회피";
                case StatUpgrade.Energy: return "에너지";
                case StatUpgrade.SkillEnergy: return "스킬 에너지";
                case StatUpgrade.RelicCapacity: return "유물 최대치";
                case StatUpgrade.PickupSlots: return "픽업 스킬 칸";
                default: return "";
            }
        }

        public StatUpgradeRule FindStatRule(StatUpgrade stat)
        {
            return StatRules.TryGetValue(stat, out var rule) ? rule : null;
        }

        // 다음 단계 비용. 이미 최대면 -1
        public int GetStatCost(StatUpgrade stat, int currentLevel)
        {
            var rule = FindStatRule(stat);
            if (rule == null || currentLevel >= rule.MaxLevel) return -1;
            return rule.BaseCost + rule.CostStep * currentLevel;
        }

        public int GetSkillCost(int currentLevel)
        {
            if (currentLevel >= SkillMaxLevel) return -1;
            return SkillBaseCost + SkillCostStep * currentLevel;
        }

        // 강화를 런 스텟에 더함 (BeginRun)
        public void ApplyToStats(ClassUpgrades upgrades, CharacterStats stats)
        {
            int Bonus(StatUpgrade stat)
            {
                var rule = FindStatRule(stat);
                return rule != null ? Math.Min(upgrades.GetStatLevel(stat), rule.MaxLevel) * rule.AmountPerLevel : 0;
            }

            stats.MaxHealth += Bonus(StatUpgrade.Health);
            stats.Attack += Bonus(StatUpgrade.Attack);
            stats.Defense += Bonus(StatUpgrade.Defense);
            stats.Evasion += Bonus(StatUpgrade.Evasion);
            stats.MaxEnergy += Bonus(StatUpgrade.Energy);
            stats.StartSkillEnergy += Bonus(StatUpgrade.SkillEnergy);
            stats.EnhanceSlots += Bonus(StatUpgrade.PickupSlots);

            // 유물 칸: 데이터에 값이 없으면(0) 기본 6 에서
            int baseRelics = stats.MaxRelics > 0 ? stats.MaxRelics : RelicCapacity.Base;
            stats.MaxRelics = Math.Min(baseRelics + Bonus(StatUpgrade.RelicCapacity), RelicCapacity.Max);
        }

        // 단계를 규칙 한도 안으로 (손님이 보낸 값 검증)
        public ClassUpgrades Sanitize(ClassUpgrades input)
        {
            var output = new ClassUpgrades { Class = input.Class };
            for (int i = 0; i < ClassUpgrades.StatCount; ++i)
            {
                var rule = FindStatRule((StatUpgrade)i);
                output.StatLevels.Add(rule != null ? Clamp(input.GetStatLevel((StatUpgrade)i), 0, rule.MaxLevel) : 0);
            }
            for (int i = 0; i < ClassUpgrades.NumBasicSkills; ++i)
            {
                output.SkillLevels.Add(Clamp(input.GetSkillLevel(i), 0, SkillMaxLevel));
            }
            return output;
        }

        static int Clamp(int v, int min, int max) => v < min ? min : (v > max ? max : v);
    }
}
