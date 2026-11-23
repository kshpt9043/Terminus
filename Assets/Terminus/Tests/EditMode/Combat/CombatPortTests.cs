// 이식하면서 생긴 부분 확인 (언리얼 테스트엔 없음): 반올림 규칙, 복제 알림, 퓨전 굴림 주입
using System;
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Data;
using static Terminus.Tests.CombatTestUtil;

namespace Terminus.Tests
{
    public class CombatPortTests
    {
        // FMath::RoundToInt 는 0.5 를 항상 위로. C# Math.Round 는 짝수 쪽이라 2.5 -> 2 가 됨
        [Test]
        public void RoundToInt_MatchesUnreal()
        {
            Assert.AreEqual(3, CombatMath.RoundToInt(2.5f));
            Assert.AreEqual(4, CombatMath.RoundToInt(3.5f));
            Assert.AreEqual(-2, CombatMath.RoundToInt(-2.5f));
            // 공포: 10 * 0.75 = 7.5 -> 8
            CombatStats attacker = MakeBattler();
            attacker.ApplyStatus(StatusEffect.Fear, 0, 1);
            CombatStats t = MakeBattler();
            t.ApplyDamage(10, attacker);
            Assert.AreEqual(92, Health(t));
        }

        // 언리얼 OnRep_State: 클라는 살아 있다가 죽은 값을 받으면 죽음 알림
        [Test]
        public void ApplyReplicatedState_NotifiesDeath()
        {
            CombatStats client = MakeBattler();
            client.HasAuthority = false;
            int died = 0;
            client.OnDied += _ => ++died;

            var dead = client.State.Clone();
            dead.Health = 0;
            client.ApplyReplicatedState(dead);
            Assert.AreEqual(1, died);
            Assert.IsTrue(client.IsDead());

            // 권한 없으면 동사는 아무것도 안 함 (언리얼 HasAuth)
            var alive = client.State.Clone();
            alive.Health = 50;
            client.ApplyReplicatedState(alive);
            client.ApplyDamage(10);
            Assert.AreEqual(50, Health(client));
        }

        // 퓨전은 받는 쪽마다 중독 / 용암 / 얼음 중 하나. 같은 시드면 같은 결과
        [Test]
        public void Fusion_UsesInjectedRandom()
        {
            var skill = new SkillRow
            {
                ActionKind = ActionKind.StatusFusion,
                StatusEffect = StatusEffect.Fusion,
                StatusValue = 2,
                StatusDuration = 2,
            };

            string Roll(int seed)
            {
                CombatStats caster = MakeBattler();
                CombatStats a = MakeBattler();
                CombatStats b = MakeBattler();
                SkillExecutor.Execute(skill, caster, new[] { a, b }, null, new Random(seed));
                Assert.AreEqual(1, a.State.Statuses.Count);
                Assert.AreNotEqual(StatusEffect.Fusion, a.State.Statuses[0].Type);
                return a.State.Statuses[0].Type + "/" + b.State.Statuses[0].Type;
            }

            Assert.AreEqual(Roll(1234), Roll(1234));
        }
    }
}
