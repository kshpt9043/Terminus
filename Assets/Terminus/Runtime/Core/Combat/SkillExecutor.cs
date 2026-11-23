// 스킬 한 줄(SkillRow)을 CombatStats 동사로 바꿔주는 다리 (언리얼 Combat/SkillExecutor 이식)
// 들고 있는 값이 없어서 객체를 안 만들고 static 으로만 씀
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Combat
{
    public static class SkillExecutor
    {
        // 퓨전 굴림에 rng 를 안 넘겼을 때만 씀. 서버는 시드 고정한 Random 을 넘길 것
        static readonly Random FallbackRandom = new Random();

        static bool _bSecondaryHit;

        // 지금 들어가는 피해가 광역 보조 피해(SecondaryValue)인가. 타격 시(OnHitEnemy) 유물은 주 공격에만 (11-04 엑셀 v1.3)
        public static bool IsSecondaryHit() => _bSecondaryHit;

        /// <summary>
        /// Opponents = 시전자의 상대 편 전체. 광역 추가 피해(SingleAoe) / 적 전체 상태(SingleAoePoison) 가 씀. 비면(null 포함) Targets 로 대신함.
        /// rng = 퓨전(중독 / 용암 / 얼음 중 하나) 굴림. null 이면 내부 Random
        /// </summary>
        public static void Execute(SkillRow skill,
                                   CombatStats caster,
                                   IReadOnlyList<CombatStats> targets,
                                   IReadOnlyList<CombatStats> opponents = null,
                                   Random rng = null)
        {
            if (caster == null)
            {
                return;
            }
            if (targets == null)
            {
                targets = Array.Empty<CombatStats>();
            }
            if (rng == null)
            {
                rng = FallbackRandom;
            }

            // ① 최종 수치. 스텟에 딱지까지 합산해서 Ratio 곱함
            int stat = GetEffectiveStat(caster, skill.ScalingStat);
            int amount = skill.BaseValue + CombatMath.RoundToInt(stat * skill.ScalingRatio);

            // ② 무엇을 누구한테 할지
            Recipe recipe = GetRecipe(skill.ActionKind);
            IReadOnlyList<CombatStats> allOpponents = (opponents != null && opponents.Count > 0) ? opponents : targets;

            var receivers = new List<CombatStats>(targets);
            if (recipe.bOnCaster)
            {
                receivers.Clear();
                receivers.Add(caster);
            }

            // ③ BaseValue 적용. 흡혈은 체력을 깎은 타격 수를 셈
            int drainHits = 0;
            foreach (var r in receivers)
            {
                if (r == null)
                {
                    continue;
                }

                switch (recipe.Use)
                {
                    case ValueUse.Damage:
                        for (int i = 0; i < Math.Max(1, skill.HitCount); ++i)
                        {
                            if (r.ApplyDamage(amount, caster) > 0)   // 시전자 = 때린 쪽 (공포 / 반격 / 타격 판정)
                            {
                                ++drainHits;
                            }
                        }
                        break;
                    case ValueUse.Shield:
                        r.AddShield(amount);
                        break;
                    case ValueUse.Heal:
                        r.Heal(amount);
                        break;
                }
            }

            // ④ SecondaryValue: 광역 추가 피해는 공격, 시전자 보호막 / 회복은 방어 보정 ("(2+공격)", "(3+방어)")
            if (recipe.bAoeDamage)
            {
                int aoe = skill.SecondaryValue + GetEffectiveStat(caster, ScalingStat.ATK);
                bool previous = _bSecondaryHit;   // 언리얼 TGuardValue
                _bSecondaryHit = true;            // 이 피해로는 타격 시 유물이 안 걸림
                try
                {
                    foreach (var o in allOpponents)
                    {
                        if (o != null && !o.IsDead())
                        {
                            o.ApplyDamage(aoe, caster);
                        }
                    }
                }
                finally
                {
                    _bSecondaryHit = previous;
                }
            }
            int secondaryDef = skill.SecondaryValue + GetEffectiveStat(caster, ScalingStat.DEF);
            if (recipe.SecondaryOnCaster == ValueUse.Shield)
            {
                caster.AddShield(secondaryDef);
            }
            else if (recipe.SecondaryOnCaster == ValueUse.Heal)
            {
                caster.Heal(secondaryDef);
            }
            if (recipe.bDrain && drainHits > 0)
            {
                caster.Heal(skill.SecondaryValue * drainHits);
            }

            // 에너지 회복 (신의 축복 / 아드레날린). 최대치는 안 넘김
            if (recipe.bGainEnergy)
            {
                caster.AddEnergy(amount);
            }

            // ⑤ 상태이상. 컬럼이 None 이 아니면 레시피가 말한 쪽한테
            List<CombatStats> statusReceivers;
            switch (recipe.StatusTo)
            {
                case StatusTo.Caster: statusReceivers = new List<CombatStats> { caster }; break;
                case StatusTo.Opponents: statusReceivers = new List<CombatStats>(allOpponents); break;
                default: statusReceivers = new List<CombatStats>(targets); break;
            }

            void ApplyTo(StatusEffect type, int value)
            {
                foreach (var t in statusReceivers)
                {
                    if (t != null && !t.IsDead())
                    {
                        // 받는 쪽마다 따로 굴림 (언리얼과 같음)
                        t.ApplyStatus(ResolveFusion(type, rng), value, skill.StatusDuration);
                    }
                }
            }

            if (skill.StatusEffect != StatusEffect.None)
            {
                // 반격은 반사 데미지가 SecondaryValue (+ 스텟 보정). Enum_State: "SecondaryValue 만큼의 데미지"
                int value = skill.StatusValue;
                if (skill.StatusEffect == StatusEffect.Counter)
                {
                    value = skill.SecondaryValue + CombatMath.RoundToInt(stat * skill.ScalingRatio);
                }
                else if (recipe.bScaleStatus)
                {
                    value = skill.StatusValue + CombatMath.RoundToInt(stat * skill.ScalingRatio);
                }
                ApplyTo(skill.StatusEffect, value);
            }
            if (recipe.ExtraStatus != StatusEffect.None)
            {
                ApplyTo(recipe.ExtraStatus, skill.StatusValue);
            }
            if (recipe.bStatusFromBase && recipe.BaseStatus != StatusEffect.None && skill.BaseValue > 0)
            {
                caster.ApplyStatus(recipe.BaseStatus, skill.BaseValue, skill.StatusDuration);
            }

            // ⑥ 반동. 컬럼이 0 보다 크면 시전자가 받음
            if (skill.SelfDamage > 0)
            {
                caster.ApplyDamage(skill.SelfDamage);
            }
        }

        // 실행하지 않고 최종 수치만 (BaseValue + 스텟 보정). 몬스터 행동 예고에 "공격 7" 처럼 띄울 때
        public static int PreviewAmount(SkillRow skill, CombatStats caster)
        {
            if (caster == null)
            {
                return skill.BaseValue;
            }

            // Execute 의 ① 과 같은 계산
            int stat = GetEffectiveStat(caster, skill.ScalingStat);
            return skill.BaseValue + CombatMath.RoundToInt(stat * skill.ScalingRatio);
        }

        // BaseValue 를 무엇으로 쓰는지만 알려 줌 (Execute 의 ② 와 같은 표). 행동 계획 공유가 "피해 6 / 회복 4" 를 띄울 때
        // 셋 다 false 면 수치를 안 쓰는 스킬. bOutOnCaster = 대상 말고 시전자에게
        public static void DescribeValueUse(ActionKind kind, out bool bOutDamage, out bool bOutShield, out bool bOutHeal, out bool bOutOnCaster)
        {
            Recipe recipe = GetRecipe(kind);
            bOutDamage = recipe.Use == ValueUse.Damage;
            bOutShield = recipe.Use == ValueUse.Shield;
            bOutHeal = recipe.Use == ValueUse.Heal;
            bOutOnCaster = recipe.bOnCaster;
        }

        // 상태까지 더한 실제 공격 / 방어 (용기 +, 허약 -, 가호 +, 용암 -). 전투 화면 스텟 표시도 이걸 씀
        public static int GetEffectiveStat(CombatStats who, ScalingStat stat)
        {
            CharacterStats b = who.Stats;

            switch (stat)
            {
                case ScalingStat.ATK:
                    // 얼음 그룹 = 허약(얼음 / 침수)
                    return b.Attack
                        + who.GetStatusValue(StatusEffect.Brave)
                        - who.GetStatusValue(StatusEffect.Ice);

                case ScalingStat.DEF:
                    // 가호와 견고는 같은 그룹이라 하나만 걸림 -> 한 번만 더함
                    return b.Defense
                        + who.GetStatusValue(StatusEffect.Protection)
                        - who.GetStatusValue(StatusEffect.Lava);

                default:
                    return 0;
            }
        }

        // 이 스킬이 거는 상태 전부 (StatusEffect 칸 + 조합 스킬이 같이 거는 것). 스킬 툴팁의 상태이상 설명 창
        public static List<StatusEffect> GetAppliedStatuses(SkillRow skill)
        {
            var output = new List<StatusEffect>();
            Recipe recipe = GetRecipe(skill.ActionKind);
            foreach (var type in new[] { skill.StatusEffect, recipe.ExtraStatus, recipe.BaseStatus })
            {
                if (type != StatusEffect.None && type != StatusEffect.SelfDamage) AddUnique(output, type);
            }
            // 퓨전은 셋 중 하나로 바뀜 -> 후보도 같이 보여 줌
            if (output.Contains(StatusEffect.Fusion))
            {
                AddUnique(output, StatusEffect.Poison);
                AddUnique(output, StatusEffect.Lava);
                AddUnique(output, StatusEffect.Ice);
            }
            return output;
        }

        static void AddUnique(List<StatusEffect> list, StatusEffect type)
        {
            if (!list.Contains(type)) list.Add(type);
        }

        // ------------------------------------------------------------ 레시피

        // 수치를 뭘로 쓰는지
        enum ValueUse
        {
            None,
            Damage,
            Shield,
            Heal,
        }

        // 상태이상을 누구한테 거는지
        enum StatusTo
        {
            Targets,     // 고른 대상 (Self 스킬이면 시전자)
            Caster,      // 적을 때리고 자기한테 버프 (HitEnemyProtect 등)
            Opponents,   // 상대 편 전체 (주 대상 + 광역 + 적 전체 중독)
        }

        // ActionKind 하나 = 원자 몇 개의 조합. Enum_Reference 시트의 "참조 ActionKind" 칸 그대로
        // 기본값이 전부 0 (None / Targets / false) 이라 struct 로 둠
        struct Recipe
        {
            // BaseValue (+ 스텟)
            public ValueUse Use;
            public bool bOnCaster;              // true 면 대상 말고 시전자한테

            // SecondaryValue
            public bool bAoeDamage;             // 상대 편 전체에 (Secondary + 공격) 추가 피해
            public ValueUse SecondaryOnCaster;  // 시전자에게 (Secondary + 방어) 보호막 / 회복
            public bool bDrain;                 // 체력을 깎은 타격마다 시전자가 Secondary 만큼 회복

            public bool bGainEnergy;            // BaseValue (+ 스텟) 만큼 시전자 에너지

            public StatusTo StatusTo;
            public StatusEffect ExtraStatus;    // 같은 수치 / 지속으로 하나 더 (BraveAllSolidAll 의 견고 등)
            public bool bScaleStatus;           // 상태 수치에도 스텟 보정 (은신: (1+방어) 만큼 민첩)
            public bool bStatusFromBase;        // 상태 수치 = BaseValue (EvasionSelfPoisonAll 의 민첩)
            public StatusEffect BaseStatus;     // bStatusFromBase 로 거는 상태
        }

        static Recipe GetRecipe(ActionKind kind)
        {
            var r = new Recipe();

            switch (kind)
            {
                // 피해 -> 대상, 상태 -> 대상
                case ActionKind.HitEnemy:
                case ActionKind.HitEnemyMulti:
                case ActionKind.HitAllEnemies:
                case ActionKind.HitAllEnemiesMulti:
                case ActionKind.HitEnemyMark:
                case ActionKind.HitEnemyMultiMark:
                case ActionKind.HitEnemyPoison:
                case ActionKind.HitEnemySelfDmg:
                case ActionKind.HitEnemyLava:
                case ActionKind.HitEnemyIce:
                case ActionKind.HitEnemyAcid:
                case ActionKind.HitEnemyFear:
                case ActionKind.HitEnemyInternalInjury:
                case ActionKind.HitEnemyDark:
                case ActionKind.HitEnemyFlooding:
                case ActionKind.HitEnemyContrarywind:
                case ActionKind.HitAllEnemiesMark:
                case ActionKind.HitAllEnemiesLavaAll:
                case ActionKind.HitAllEnemiesIceAll:
                case ActionKind.HitAllEnemiesPoisonAll:
                case ActionKind.HitAllEnemiesAcidAll:
                case ActionKind.HitAllEnemiesFearAll:
                case ActionKind.HitAllEnemiesFusionAll:
                case ActionKind.HitAllEnemiesInternalInjuryAll:
                case ActionKind.HitAllEnemiesDarkAll:
                case ActionKind.HitAllEnemiesFloodingAll:
                case ActionKind.HitAllEnemiesContrarywindAll:
                    r.Use = ValueUse.Damage;
                    break;

                // 주 대상 피해 + 상대 전체 (Secondary + 공격). 상태는 주 대상
                case ActionKind.HitEnemySingleAoe:
                case ActionKind.HitEnemySingleAoeLava:
                case ActionKind.HitEnemySingleAoeMark:
                    r.Use = ValueUse.Damage;
                    r.bAoeDamage = true;
                    break;

                // 위 + 상대 전체 중독
                case ActionKind.HitEnemySingleAoePoison:
                    r.Use = ValueUse.Damage;
                    r.bAoeDamage = true;
                    r.StatusTo = StatusTo.Opponents;
                    break;

                // 피해 -> 대상, 버프 -> 시전자
                case ActionKind.HitEnemyProtect:
                case ActionKind.HitEnemyEvasionSelf:
                    r.Use = ValueUse.Damage;
                    r.StatusTo = StatusTo.Caster;
                    break;

                // 피해 + 시전자 보호막 (Secondary + 방어)
                case ActionKind.HitEnemyShield:
                case ActionKind.HitAllEnemiesShield:
                    r.Use = ValueUse.Damage;
                    r.SecondaryOnCaster = ValueUse.Shield;
                    break;

                // 피해 + 시전자 보호막 + 시전자 강철
                case ActionKind.HitEnemyShieldMetal:
                case ActionKind.HitAllEnemiesShieldMetal:
                    r.Use = ValueUse.Damage;
                    r.SecondaryOnCaster = ValueUse.Shield;
                    r.StatusTo = StatusTo.Caster;
                    break;

                // 피해 + 시전자 회복 (Secondary + 방어). 얼음은 대상에게
                case ActionKind.HitEnemyHealSelf:
                case ActionKind.HitAllEnemiesHealSelf:
                case ActionKind.HitEnemyHealSelfIce:
                case ActionKind.HitAllEnemiesHealSelfIceAll:
                    r.Use = ValueUse.Damage;
                    r.SecondaryOnCaster = ValueUse.Heal;
                    break;

                // 반동 + 피해 + 체력을 깎은 타격마다 Secondary 회복 (반동은 SelfDamage 칸)
                case ActionKind.HitEnemySelfDmgDrain:
                case ActionKind.HitEnemyMultiSelfDmgDrain:
                case ActionKind.HitAllSelfDmgDrain:
                    r.Use = ValueUse.Damage;
                    r.bDrain = true;
                    break;

                // 보호막 -> 시전자 (상태는 대상: Self 스킬이면 시전자, 독 연막이면 적 전체)
                case ActionKind.ShieldSelf:
                case ActionKind.ShieldSelfCounter:
                case ActionKind.ShieldSelfPoisonAll:
                case ActionKind.ShieldSelfSolid:
                case ActionKind.ShieldSelfAbsorption:
                case ActionKind.ShieldSelfProtect:
                case ActionKind.ShieldSelfEvasion:
                case ActionKind.ShieldSelfIronWall:
                case ActionKind.ShieldSelfBraveSelf:
                    r.Use = ValueUse.Shield;
                    r.bOnCaster = true;
                    break;

                // 보호막 -> 대상
                case ActionKind.ShieldAll:
                case ActionKind.ShieldAllSolidAll:
                    r.Use = ValueUse.Shield;
                    break;

                // 회복 -> 대상
                case ActionKind.HealAlly:
                case ActionKind.HealAll:
                case ActionKind.HealAllBraveAll:
                    r.Use = ValueUse.Heal;
                    break;

                // 회복 -> 시전자 (유물)
                case ActionKind.HealSelf:
                case ActionKind.HealSelfProtect:
                case ActionKind.HealSelfBraveSelf:
                    r.Use = ValueUse.Heal;
                    r.bOnCaster = true;
                    break;

                // 에너지 회복 (+ 자신 버프)
                case ActionKind.GainEnergy:
                case ActionKind.GainEnergyProtectSelf:
                case ActionKind.GainEnergyIndomitableSelf:
                case ActionKind.GainEnergyBraveSelf:
                case ActionKind.GainEnergyBlindnessSelf:
                    r.bGainEnergy = true;
                    break;

                // 은신: (StatusValue + 방어) 만큼 민첩
                case ActionKind.EvasionSelf:
                    r.bScaleStatus = true;
                    break;

                // 시전자 민첩(BaseValue) + 적 전체 중독
                case ActionKind.EvasionSelfPoisonAll:
                    r.bStatusFromBase = true;
                    r.BaseStatus = StatusEffect.Evasion;
                    break;

                // 상태 두 개를 같은 수치로
                case ActionKind.BraveAllSolidAll:
                    r.ExtraStatus = StatusEffect.Solid;
                    break;
                case ActionKind.EvasionSelfBraveSelf:
                case ActionKind.EvasionSelfCounterSelf:
                    r.ExtraStatus = StatusEffect.Evasion;
                    break;
                case ActionKind.StatusMarkFearAll:
                    r.ExtraStatus = StatusEffect.Fear;
                    break;

                // 나머지(상태만 거는 것, 반동만, 유물 전용)는 BaseValue 를 안 씀. 상태 / 반동은 컬럼이 따로 말해줌
                default:
                    break;
            }

            return r;
        }

        static StatusEffect ResolveFusion(StatusEffect type, Random rng)
        {
            if (type != StatusEffect.Fusion)
            {
                return type;
            }

            // 퓨전은 중독 용암 얼음 중 하나. CombatStats 가 퓨전을 거절하니까 여기서 바꿔서 넘김
            switch (rng.Next(0, 3))
            {
                case 0: return StatusEffect.Poison;
                case 1: return StatusEffect.Lava;
                default: return StatusEffect.Ice;
            }
        }
    }
}
