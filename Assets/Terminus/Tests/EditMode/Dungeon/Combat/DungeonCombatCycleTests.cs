// 던전 전투: 사이클 순서 (보호막 / 에너지 / 중독 / 예고 / 몬스터 차례)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatCycleTests
    {
        [SetUp]
        public void LoadData() => TestData.LoadGameData();   // 데이터를 고치는 테스트가 있어서 매번 새로

        [OneTimeTearDown]
        public void RestoreData() => TestData.LoadGameData();   // 테스트용 몬스터 행을 다음 묶음에 남기지 않게

        static DungeonPlayer P(ulong id) => DungeonCombatPlayers.Make(id);

        [Test]
        public void Start_IntentsFromMonsterSkills()
        {
            var kit = new DungeonCombatKit(4);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_redslime" }, P(1));

            Assert.AreEqual(2, kit.Combat.Intents.Count);
            for (int i = 0; i < 2; ++i)
            {
                var m = kit.Combat.Monsters[i];
                var intent = kit.Combat.FindIntent(m);
                Assert.IsNotNull(intent);
                Assert.AreSame(m, intent.Monster);
                var skillRows = DungeonMonster.GetSkillSlots(m.Row).ConvertAll(s => s.Skill);
                CollectionAssert.Contains(skillRows, intent.SkillRow);

                var skill = GameData.FindSkillRow(intent.SkillRow);
                Assert.AreEqual(SkillExecutor.PreviewAmount(skill, m.Stats), intent.Amount);
                Assert.AreEqual(DungeonMonster.ToIntentKind(skill), intent.Kind);
                Assert.AreEqual(1, intent.HitCount);
            }
        }

        [Test]
        public void Intents_FallbackSkillsWhenMonsterHasNone()
        {
            GameData.Monsters.Add(new MonsterRow { RowName = "test_noskill", BaseHP = 10, MonsterCategory = MonsterCategory.Normal });

            var kit = new DungeonCombatKit(4);
            kit.Settings.FallbackMonsterSkills = new List<WeightedSkill> { new WeightedSkill("condense", 5) };
            kit.StartForced(RoomType.MONSTER, new[] { "test_noskill" }, P(1));

            Assert.AreEqual("condense", kit.Combat.Intents[0].SkillRow);
            Assert.AreEqual(MonsterIntentKind.Defend, kit.Combat.Intents[0].Kind);
        }

        [Test]
        public void Intents_UnknownSkill_MonsterSkipsCycle()
        {
            GameData.Monsters.Add(new MonsterRow { RowName = "test_badskill", BaseHP = 10, Skill1_ID = "no_such_skill", Skill1_Weight = 1 });

            var kit = new DungeonCombatKit(4);
            kit.StartForced(RoomType.MONSTER, new[] { "test_badskill", "mon_slime" }, P(1));
            Assert.AreEqual(1, kit.Combat.Intents.Count);
            Assert.IsNull(kit.Combat.FindIntent(kit.Combat.Monsters[0]));

            kit.EndTurnAll();
            kit.RunMonsterTurn();
            CollectionAssert.AreEqual(new[] { kit.Combat.Monsters[1] }, kit.Acted, "예고 없는 몬스터는 아무것도 안 함");
        }

        [Test]
        public void FullCycle_Order()
        {
            var kit = new DungeonCombatKit(7);
            var p1 = P(1);
            var p2 = P(2);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_redslime" }, p1, p2);
            var m0 = kit.Combat.Monsters[0];
            var m1 = kit.Combat.Monsters[1];

            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            Assert.AreEqual(3, p1.Battler.State.Energy);
            Assert.IsTrue(kit.Combat.IsTurnOf(p1));

            // 플레이어 턴: P1 방어 (보호막 6), P2 는 중독
            kit.Combat.HandleUseSkill(p1, 1, -1);
            Assert.AreEqual(6, p1.Battler.State.Shield);
            Assert.AreEqual(2, p1.Battler.State.Energy);
            p2.Battler.ApplyStatus(StatusEffect.Poison, 3, 2);
            m1.Stats.AddShield(10);   // 몬스터 보호막은 자기 차례 직전에 사라짐

            // P1 턴 종료: 에너지 0, 아직 P2 차례
            kit.Combat.HandleEndTurn(p1);
            Assert.IsTrue(kit.Combat.HasEndedTurn(p1));
            Assert.AreEqual(0, p1.Battler.State.Energy);
            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            kit.Combat.HandleUseSkill(p1, 0, 0);
            Assert.AreEqual(m0.Stats.Stats.MaxHealth, DungeonCombatMonsters.Hp(m0), "턴 종료한 사람은 스킬 못 씀");

            // 전원 턴 종료 -> 몬스터 턴 (간격 두고 한 마리씩)
            kit.Combat.HandleEndTurn(p2);
            Assert.AreEqual(CombatPhase.MonsterTurn, kit.Combat.Phase);
            Assert.AreEqual(CombatStep.MonsterAction, kit.Combat.PendingStep);
            Assert.AreEqual(kit.Settings.MonsterActionInterval, kit.Combat.PendingStepTime - kit.Now, 1e-6);
            kit.Combat.HandleUseSkill(p2, 0, 0);
            Assert.AreEqual(m0.Stats.Stats.MaxHealth, DungeonCombatMonsters.Hp(m0), "몬스터 턴엔 스킬 못 씀");

            kit.Step();
            CollectionAssert.AreEqual(new[] { m0 }, kit.Acted);
            Assert.AreEqual(10, m1.Stats.State.Shield, "아직 m1 차례 전");

            var m1Intent = kit.Combat.FindIntent(m1);
            kit.Step();
            CollectionAssert.AreEqual(new[] { m0, m1 }, kit.Acted);
            Assert.AreEqual(m1Intent.Kind == MonsterIntentKind.Defend ? m1Intent.Amount : 0, m1.Stats.State.Shield,
                "행동 직전에 보호막을 버림 (방어 예고면 새로 얻은 만큼만)");
            Assert.AreEqual(CombatPhase.MonsterTurn, kit.Combat.Phase, "마지막 몬스터 뒤에도 한 칸 쉼");

            int p2HpBefore = DungeonCombatPlayers.Hp(p2);
            kit.Step();

            // 다음 사이클: 보호막 제거, 에너지 최대, 중독은 자기 턴 시작에
            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            Assert.AreEqual(2, kit.Combat.Cycle);
            Assert.AreEqual(0, p1.Battler.State.Shield);
            Assert.AreEqual(3, p1.Battler.State.Energy);
            Assert.AreEqual(3, p2.Battler.State.Energy);
            Assert.AreEqual(p2HpBefore - 3, DungeonCombatPlayers.Hp(p2), "중독 3");
            Assert.IsFalse(kit.Combat.HasEndedTurn(p1));
            Assert.AreEqual(2, kit.Combat.Intents.Count);
        }

        [Test]
        public void MonsterTurn_PoisonTicksBeforeAction_PoisonDeathSkipsAction()
        {
            var kit = new DungeonCombatKit(3);
            var p = P(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slime", "mon_slime" }, p);
            var m0 = kit.Combat.Monsters[0];
            var m1 = kit.Combat.Monsters[1];
            var m2 = kit.Combat.Monsters[2];

            m0.Stats.ApplyStatus(StatusEffect.Poison, 4, 3);
            m1.Stats.ApplyDamage(DungeonCombatMonsters.Hp(m1) - 2);
            m1.Stats.ApplyStatus(StatusEffect.Poison, 5, 3);
            int m0Hp = DungeonCombatMonsters.Hp(m0);

            kit.EndTurnAll();
            kit.Step();
            Assert.AreEqual(m0Hp - 4, DungeonCombatMonsters.Hp(m0), "자기 차례 시작에 중독");
            CollectionAssert.AreEqual(new[] { m0 }, kit.Acted);

            kit.Step();
            Assert.IsTrue(m1.IsDead(), "중독으로 죽음");
            CollectionAssert.AreEqual(new[] { m0 }, kit.Acted, "죽은 몬스터는 행동 없음");
            CollectionAssert.Contains(kit.HideRequested, m1);
            Assert.AreEqual(CombatStep.MonsterAction, kit.Combat.PendingStep, "다음 몬스터는 간격 두고");

            kit.Step();
            CollectionAssert.AreEqual(new[] { m0, m2 }, kit.Acted);
            kit.Step();
            Assert.AreEqual(2, kit.Combat.Cycle);
            Assert.AreEqual(2, kit.Combat.Intents.Count, "죽은 몬스터는 예고 없음");
            Assert.IsNull(kit.Combat.FindIntent(m1));
        }

        [Test]
        public void StatusDuration_TicksAtOwnTurnEnd_Vitality()
        {
            var kit = new DungeonCombatKit(3);
            var p = P(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, p);

            kit.Combat.HandleEndTurn(p);
            // 몬스터 턴 중에 걸린 것 (자기 턴 밖)
            p.Battler.ApplyStatus(StatusEffect.Brave, 1, 1);
            p.Battler.ApplyStatus(StatusEffect.Vitality, 2, 3);
            kit.RunMonsterTurn();

            Assert.AreEqual(2, kit.Combat.Cycle);
            Assert.AreEqual(3 + 2, p.Battler.State.Energy, "활력: 턴 시작 에너지 + 수치");
            Assert.IsTrue(p.Battler.HasStatus(StatusEffect.Brave));
            kit.Combat.HandleEndTurn(p);
            Assert.IsFalse(p.Battler.HasStatus(StatusEffect.Brave), "자기 턴 끝에 1 감소 -> 0");
        }

        [Test]
        public void MonsterAttack_HitsOnlyAlivePlayers()
        {
            var kit = new DungeonCombatKit(3);
            var p1 = P(1);
            var p2 = P(2);
            kit.Settings.FallbackMonsterSkills = new List<WeightedSkill> { new WeightedSkill("slime_strike", 1) };
            GameData.Monsters.Add(new MonsterRow { RowName = "test_striker", BaseHP = 50 });
            kit.StartForced(RoomType.MONSTER, new[] { "test_striker" }, p1, p2);

            DungeonCombatPlayers.Kill(p2);
            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            int hp1 = DungeonCombatPlayers.Hp(p1);

            kit.Combat.HandleEndTurn(p1);
            Assert.AreEqual(CombatPhase.MonsterTurn, kit.Combat.Phase, "죽은 사람은 기다리지 않음");
            kit.Step();
            // slime_strike = 5 + 공격(0 + 인원 2) -> 전체
            Assert.AreEqual(hp1 - 7, DungeonCombatPlayers.Hp(p1));
            Assert.AreEqual(0, DungeonCombatPlayers.Hp(p2));
        }

        [Test]
        public void DisconnectedPlayer_NotWaitedFor()
        {
            var kit = new DungeonCombatKit(3);
            var p1 = P(1);
            var p2 = P(2);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, p1, p2);

            p2.bConnected = false;
            kit.Combat.HandleEndTurn(p1);
            Assert.AreEqual(CombatPhase.MonsterTurn, kit.Combat.Phase);
        }

        [Test]
        public void KillingAllMonstersWithSkills_Victory()
        {
            var kit = new DungeonCombatKit(3);
            var p = P(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, p);
            var m = kit.Combat.Monsters[0];
            m.Stats.ApplyDamage(DungeonCombatMonsters.Hp(m) - 1);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.IsTrue(m.IsDead());
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            CollectionAssert.AreEqual(new[] { m }, kit.HideRequested);
            Assert.AreEqual(0, kit.Combat.Intents.Count);
        }

        [Test]
        public void StateChanged_RaisedOnProgress()
        {
            var kit = new DungeonCombatKit(3);
            int changes = 0;
            kit.Combat.StateChanged += () => ++changes;
            var p = P(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, p);
            Assert.That(changes, Is.GreaterThan(0));

            int before = changes;
            kit.Combat.HandleEndTurn(p);
            Assert.That(changes, Is.GreaterThan(before));
        }
    }
}
