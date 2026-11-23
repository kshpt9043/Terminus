// 스킬 실행기 자동 테스트 (언리얼 Tests/SkillExecutorTests.cpp 의 Terminus.Skill.*)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Data;
using static Terminus.Tests.CombatTestUtil;

namespace Terminus.Tests
{
    public class SkillExecutorTests
    {
        // Terminus.Skill.AllRowsExecute
        // DT_Skill 의 모든 행이 터지지 않고 돌고, 피해 스킬은 적 체력을 깎는지
        [Test]
        public void AllRowsExecute()
        {
            TestData.LoadGameData();
            Assert.Greater(GameData.Skills.Count, 0, "DT_Skill");

            // 언리얼 TestTrue 처럼 실패해도 계속 돌고 끝에 모아서 보여 줌
            var failures = new List<string>();
            int count = 0;
            foreach (var row in GameData.Skills.RowNames)
            {
                SkillRow skill = GameData.FindSkillRow(row);
                if (skill == null) continue;

                CombatStats caster = MakeBattler(100, 1, 1);
                CombatStats ally = MakeBattler(100);
                CombatStats enemyA = MakeBattler(1000);
                CombatStats enemyB = MakeBattler(1000);
                caster.ApplyDamage(30);   // 회복 스킬이 보이게
                ally.ApplyDamage(30);
                int casterBefore = Health(caster);

                List<CombatStats> targets = null;
                switch (skill.TargetType)
                {
                    case TargetType.Self: targets = new List<CombatStats> { caster }; break;
                    case TargetType.SingleEnemy: targets = new List<CombatStats> { enemyA }; break;
                    case TargetType.AllEnemies: targets = new List<CombatStats> { enemyA, enemyB }; break;
                    case TargetType.SingleAlly: targets = new List<CombatStats> { ally }; break;
                    case TargetType.AllAllies: targets = new List<CombatStats> { caster, ally }; break;
                }

                SkillExecutor.Execute(skill, caster, targets, new[] { enemyA, enemyB });
                ++count;

                SkillExecutor.DescribeValueUse(skill.ActionKind, out bool bDamage, out bool bShield, out bool bHeal, out bool bOnCaster);
                if (bDamage && skill.BaseValue > 0 && !(Health(enemyA) < 1000))
                {
                    failures.Add($"{row}: 적 체력이 깎임");
                }
                if (bShield && skill.BaseValue > 0 && !(Shield(bOnCaster ? caster : targets[0]) > 0))
                {
                    failures.Add($"{row}: 보호막 생김");
                }
                if (bHeal && bOnCaster && skill.BaseValue > 0 && !(Health(caster) > casterBefore - skill.SelfDamage))
                {
                    failures.Add($"{row}: 시전자 회복");
                }
                if (skill.StatusEffect != StatusEffect.None && skill.StatusEffect != StatusEffect.SelfDamage && skill.StatusDuration != 0)
                {
                    bool bAny = caster.HasStatus(skill.StatusEffect) || ally.HasStatus(skill.StatusEffect)
                        || enemyA.HasStatus(skill.StatusEffect) || enemyB.HasStatus(skill.StatusEffect)
                        || skill.StatusEffect == StatusEffect.Fusion;
                    if (!bAny) failures.Add($"{row}: 상태 {skill.StatusEffect} 걸림");
                }
            }

            TestContext.WriteLine($"스킬 {count}개 실행");
            Assert.IsEmpty(failures, string.Join("\n", failures));
            Assert.Greater(count, 0);
        }

        // ------------------------------------------------------------ Terminus.Skill.Rules
        // 조합형 / 새 규칙 수치 확인 (언리얼은 한 테스트 안의 블록들 -> 블록마다 하나씩)

        // 마도 캐논: 주 대상 (5+공격), 상대 전체 (2+공격) 추가
        [Test]
        public void Rules_SingleAoe()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.HitEnemySingleAoe,
                TargetType = TargetType.SingleEnemy,
                BaseValue = 5,
                ScalingStat = ScalingStat.ATK,
                SecondaryValue = 2,
            };
            CombatStats caster = MakeBattler(100, 1);
            CombatStats a = MakeBattler();
            CombatStats b = MakeBattler();
            SkillExecutor.Execute(skill, caster, new[] { a }, new[] { a, b });
            Assert.AreEqual(100 - 9, Health(a), "광역: 주 대상 6 + 3");
            Assert.AreEqual(100 - 3, Health(b), "광역: 나머지 3");
        }

        // 피해 + 시전자 보호막 (Secondary + 방어)
        [Test]
        public void Rules_HitEnemyShield()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.HitEnemyShield,
                BaseValue = 6,
                ScalingStat = ScalingStat.ATK,
                SecondaryValue = 3,
            };
            CombatStats caster = MakeBattler(100, 0, 2);
            CombatStats a = MakeBattler();
            SkillExecutor.Execute(skill, caster, new[] { a });
            Assert.AreEqual(94, Health(a), "공격+보호막: 피해 6");
            Assert.AreEqual(5, Shield(caster), "공격+보호막: 보호막 3+2");
        }

        // 흡혈 다단: 체력을 깎은 타격마다 Secondary 회복, 반동은 SelfDamage
        [Test]
        public void Rules_MultiSelfDmgDrain()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.HitEnemyMultiSelfDmgDrain,
                BaseValue = 7,
                ScalingStat = ScalingStat.ATK,
                HitCount = 2,
                SecondaryValue = 3,
                SelfDamage = 3,
            };
            CombatStats caster = MakeBattler();
            caster.ApplyDamage(20);
            CombatStats a = MakeBattler();
            SkillExecutor.Execute(skill, caster, new[] { a });
            Assert.AreEqual(86, Health(a), "흡혈: 적 7x2");
            Assert.AreEqual(83, Health(caster), "흡혈: 80 + 6 - 3");
        }

        // 공격하면서 자기 가호 (예전엔 적에게 걸렸음)
        [Test]
        public void Rules_HitEnemyProtect_BuffsCaster()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.HitEnemyProtect,
                BaseValue = 4,
                StatusEffect = StatusEffect.Protection,
                StatusValue = 1,
                StatusDuration = 1,
            };
            CombatStats caster = MakeBattler();
            CombatStats a = MakeBattler();
            SkillExecutor.Execute(skill, caster, new[] { a });
            Assert.IsTrue(caster.HasStatus(StatusEffect.Protection), "가호는 시전자에게");
            Assert.IsFalse(a.HasStatus(StatusEffect.Protection), "적에겐 안 걸림");
        }

        // 용기 + 견고 같이
        [Test]
        public void Rules_BraveAllSolidAll()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.BraveAllSolidAll,
                TargetType = TargetType.AllAllies,
                StatusEffect = StatusEffect.Brave,
                StatusValue = 2,
                StatusDuration = 2,
            };
            CombatStats caster = MakeBattler();
            SkillExecutor.Execute(skill, caster, new[] { caster });
            Assert.AreEqual(2, caster.GetStatusValue(StatusEffect.Brave), "용기 2");
            Assert.AreEqual(2, caster.GetStatusValue(StatusEffect.Solid), "견고 2");
        }

        // 은신: (StatusValue + 방어) 만큼 민첩
        [Test]
        public void Rules_EvasionSelf_ScalesWithDefense()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.EvasionSelf,
                TargetType = TargetType.Self,
                ScalingStat = ScalingStat.DEF,
                StatusEffect = StatusEffect.Evasion,
                StatusValue = 1,
                StatusDuration = 3,
            };
            CombatStats caster = MakeBattler(100, 0, 2);
            SkillExecutor.Execute(skill, caster, new[] { caster });
            Assert.AreEqual(3, caster.GetStatusValue(StatusEffect.Evasion), "민첩 1+2");
        }
    }
}
