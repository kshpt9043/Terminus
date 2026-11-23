// 피해 계산 자동 테스트 (언리얼 Tests/SkillExecutorTests.cpp 의 Terminus.Combat.DamageRules)
// 상태 그룹 / 체술 / 보호막 관통. 언리얼은 한 테스트 안의 블록들 -> 블록마다 하나씩
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Data;
using static Terminus.Tests.CombatTestUtil;

namespace Terminus.Tests
{
    public class DamageRulesTests
    {
        // 같은 그룹은 쌓임 + 화면엔 한 칸: 얼음 1 다음 침수 3 -> 허약 4, 표시 이름은 나중 것(침수)
        [Test]
        public void SameGroup_StacksAndShowsAsOne()
        {
            CombatStats s = MakeBattler();
            s.ApplyStatus(StatusEffect.Ice, 1, 2);
            s.ApplyStatus(StatusEffect.Flooding, 3, 2);
            Assert.AreEqual(4, s.GetStatusValue(StatusEffect.Ice), "그룹: 얼음으로 물어도 합");
            var shown = s.GetDisplayStatuses();
            Assert.AreEqual(1, shown.Count, "그룹: 화면엔 한 칸");
            Assert.AreEqual(4, shown[0].Value, "그룹: 표시 수치");
            Assert.AreEqual("허약(침수)", StatusInfo.GetDisplayName(shown[0].Type), "그룹: 표시 이름");
        }

        // 중독 쌓임 (11-03 피드백 예시): 2(2사이클) + 2(1사이클) + 1(3사이클) -> 턴마다 5 / 3 / 1
        [Test]
        public void Poison_StacksAndTicksPerOwnTurn()
        {
            CombatStats s = MakeBattler();
            s.ApplyStatus(StatusEffect.Poison, 2, 2);
            s.ApplyStatus(StatusEffect.Poison, 2, 1);
            s.ApplyStatus(StatusEffect.Poison, 1, 3);
            var shown = s.GetDisplayStatuses();
            Assert.AreEqual(1, shown.Count, "중독: 화면엔 한 칸");
            Assert.AreEqual(5, shown[0].Value, "중독: 표시 수치");
            Assert.AreEqual(3, shown[0].Duration, "중독: 표시 남은 사이클 = 가장 긴 것");
            foreach (int expected in new[] { 5, 3, 1 })
            {
                int before = Health(s);
                s.OnTurnStart();
                Assert.AreEqual(expected, before - Health(s), $"중독: 이번 턴 시작 {expected}");
                s.OnTurnEnd();
            }
            Assert.IsFalse(s.HasStatus(StatusEffect.Poison), "중독: 다 풀림");
        }

        // 자기 턴에 자기에게 건 1사이클 상태는 그 턴 끝에 안 사라지고 다음 자기 턴 끝까지 (11-04 피드백: 몬스터 강철)
        [Test]
        public void SelfAppliedOnOwnTurn_SkipsFirstTick()
        {
            CombatStats s = MakeBattler();
            s.OnTurnStart();
            s.ApplyStatus(StatusEffect.Metal, 0, 1);
            s.OnTurnEnd();
            Assert.IsTrue(s.HasStatus(StatusEffect.Indomitable), "강철: 건 턴 끝엔 남음");
            s.OnTurnStart();
            s.OnTurnEnd();
            Assert.IsFalse(s.HasStatus(StatusEffect.Indomitable), "강철: 다음 자기 턴 끝에 풀림");
        }

        // 남이 건 1사이클 상태(자기 턴 밖)는 다음 자기 턴 끝에 풀림 -> 그 턴엔 효과가 있음
        [Test]
        public void AppliedOutsideOwnTurn_LastsThroughNextOwnTurn()
        {
            CombatStats s = MakeBattler();
            s.ApplyStatus(StatusEffect.Ice, 1, 1);
            s.OnTurnStart();
            Assert.IsTrue(s.HasStatus(StatusEffect.Ice), "허약: 자기 턴 동안 걸려 있음");
            s.OnTurnEnd();
            Assert.IsFalse(s.HasStatus(StatusEffect.Ice), "허약: 자기 턴 끝에 풀림");
        }

        // 은형보 두 번 -> 민첩이 더해짐 (11-03 피드백: 덮어써서 안 오르던 문제)
        [Test]
        public void EvasionSelfTwice_Adds()
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
            int b = caster.GetEvasion();
            SkillExecutor.Execute(skill, caster, new[] { caster });
            SkillExecutor.Execute(skill, caster, new[] { caster });
            Assert.AreEqual(6, caster.GetEvasion() - b, "은형보 2번: 회피 +(1+2)x2");
        }

        // 불굴(정령화): 한 대 1 고정
        [Test]
        public void Spirit_FixesDamageToOne()
        {
            CombatStats s = MakeBattler();
            s.ApplyStatus(StatusEffect.Spirit, 0, 1);
            s.ApplyDamage(50);
            Assert.AreEqual(99, Health(s), "정령화: 1 만 받음");
        }

        // 체술: 회피 4. 3 은 무시, 보호막 5 가 먼저 받고 남은 5 에서 4 를 뺌
        [Test]
        public void EvasionReduction_AfterShield()
        {
            CombatStats s = MakeBattler(100, 0, 0, 4);
            s.SetEvasionReduction(true);
            s.ApplyDamage(3);
            Assert.AreEqual(100, Health(s), "체술: 회피 이하 무시");
            s.AddShield(5);
            s.ApplyDamage(10);
            Assert.AreEqual(99, Health(s), "체술: 보호막 뒤 남은 피해에서 감소");
            Assert.AreEqual(0, Shield(s), "체술: 보호막 다 깎임");
            s.ApplyStatus(StatusEffect.Evasion, 2, 1);
            s.ApplyDamage(7);
            Assert.AreEqual(98, Health(s), "체술: 민첩도 더함 (7-6)");
        }

        // 체술 유물이 없으면 회피가 있어도 그대로 받음
        [Test]
        public void NoEvasionReduction_TakesFull()
        {
            CombatStats s = MakeBattler(100, 0, 0, 4);
            s.ApplyDamage(3);
            Assert.AreEqual(97, Health(s), "체술 없음");
        }

        // 보호막 관통: 공격자 +20%. 보호막 없는 대상엔 그대로
        [Test]
        public void ShieldBonusPercent_OnlyAgainstShield()
        {
            CombatStats attacker = MakeBattler();
            attacker.SetShieldBonusPercent(20);
            CombatStats t = MakeBattler();
            t.AddShield(100);
            t.ApplyDamage(10, attacker);
            Assert.AreEqual(88, Shield(t), "관통: 12 흡수");
            CombatStats u = MakeBattler();
            u.ApplyDamage(10, attacker);
            Assert.AreEqual(90, Health(u), "관통: 보호막 없으면 10");
        }

        // ApplyDamage 반환값 = 깎인 체력
        [Test]
        public void ApplyDamage_ReturnsHealthLost()
        {
            CombatStats s = MakeBattler();
            s.AddShield(4);
            Assert.AreEqual(6, s.ApplyDamage(10), "반환: 보호막 뒤 6");
        }
    }
}
