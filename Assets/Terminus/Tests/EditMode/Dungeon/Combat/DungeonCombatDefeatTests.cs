// 던전 전투: 패배 / 전멸 알림 / 난입으로 이어 하기 / 도중 정리
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatDefeatTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static readonly string[] TwoSlimes = { "mon_slime", "mon_slime" };

        [Test]
        public void Wipe_DefeatThenWipeReportAfterDelay()
        {
            var p = DungeonCombatPlayers.Make(1);
            p.Run.AddTempBuff(new TempStatBuff { Attack = 1, BattlesLeft = 2 });
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.MONSTER, TwoSlimes, p);

            kit.Combat.DebugKillAllPlayers();
            Assert.AreEqual(CombatPhase.Defeat, kit.Combat.Phase);
            Assert.AreEqual(0, kit.Combat.Intents.Count);
            Assert.AreEqual(CombatStep.ReportWipe, kit.Combat.PendingStep);
            Assert.AreEqual(kit.Settings.CombatEndDelay, kit.Combat.PendingStepTime - kit.Now, 1e-6);
            Assert.AreEqual(1, p.Run.RunState.TempBuffs[0].BattlesLeft, "져도 전투 한 번");

            kit.Step();
            Assert.AreEqual(1, kit.Wiped);
            Assert.AreEqual(0, kit.Offers.Count);
            Assert.AreEqual(0, kit.Cleared);
            Assert.IsTrue(p.IsDead(), "전멸은 살리지 않음 (구출 / 난입 대기)");

            // 패배 뒤엔 요청 무시
            kit.Combat.DebugKillAllMonsters();
            Assert.AreEqual(CombatPhase.Defeat, kit.Combat.Phase);
        }

        [Test]
        public void WipeByMonsterAttack()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slimeking" }, p);
            p.Battler.ApplyDamage(DungeonCombatPlayers.Hp(p) - 1);

            kit.EndTurnAll();
            kit.RunMonsterTurn();
            // 왕의 검(15) / 포효 / 단단해지기 중 하나. 공격이면 전멸
            var intentWasAttack = p.IsDead();
            if (intentWasAttack)
            {
                Assert.AreEqual(CombatPhase.Defeat, kit.Combat.Phase);
                Assert.AreEqual(CombatStep.ReportWipe, kit.Combat.PendingStep);
            }
            else
            {
                Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            }
        }

        [Test]
        public void Resume_JoinersContinueAgainstSameMonsters()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var p2 = DungeonCombatPlayers.Make(2);
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.GUARDIAN, TwoSlimes, p1, p2);
            var m0 = kit.Combat.Monsters[0];
            kit.Combat.HandleUseSkill(p1, 0, 0);
            int m0Hp = DungeonCombatMonsters.Hp(m0);

            kit.Combat.DebugKillAllPlayers();
            kit.Step();
            Assert.AreEqual(1, kit.Wiped);

            // 난입: 새 사람만 전투 시작 유물 (가디언방이라 가디언 시작 유물도)
            var joiner = DungeonCombatPlayers.Make(3, CharacterClass.Fighter, false, "RLC_Common_001", "RLC_Common_017", "RLC_Common_003");
            kit.Combat.ResumeWithPlayers(new[] { joiner, p1 });

            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            Assert.AreEqual(2, kit.Combat.Cycle, "사이클은 이어서");
            Assert.AreSame(m0, kit.Combat.Monsters[0]);
            Assert.AreEqual(m0Hp, DungeonCombatMonsters.Hp(m0), "몬스터 체력 그대로");
            Assert.AreEqual(3, kit.Combat.Players.Count, "이미 있던 사람은 다시 안 들어감");
            Assert.AreSame(joiner, kit.Combat.Players[2]);
            Assert.AreEqual(1 + 2, joiner.Battler.GetStatusValue(StatusEffect.Brave), "전투 시작 + 가디언 전투 시작");
            Assert.AreEqual(4, joiner.Battler.State.Energy, "보조 배터리는 에너지 회복 뒤");
            Assert.IsTrue(kit.Combat.IsTurnOf(joiner));
            Assert.AreEqual(2, kit.Combat.Intents.Count);

            // 이 사람만 턴 종료하면 몬스터 턴 (쓰러진 사람은 기다리지 않음)
            kit.Combat.HandleEndTurn(joiner);
            Assert.AreEqual(CombatPhase.MonsterTurn, kit.Combat.Phase);
            kit.RunMonsterTurn();

            // 이기면 쓰러졌던 사람도 체력 1
            kit.Combat.DebugKillAllMonsters();
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.AreEqual(1, DungeonCombatPlayers.Hp(p1));
            Assert.AreEqual(1, DungeonCombatPlayers.Hp(p2));
            kit.Step();
            Assert.AreEqual(3, kit.Offers.Count);
            Assert.AreEqual(1, OfferRelicCount(kit), "가디언방 보상");
        }

        static int OfferRelicCount(DungeonCombatKit kit) => kit.Offers[0].Value.RelicOffers.Count;

        [Test]
        public void Resume_BeforeWipeReport_CancelsIt()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.MONSTER, TwoSlimes, p1);
            kit.Combat.DebugKillAllPlayers();
            Assert.AreEqual(CombatStep.ReportWipe, kit.Combat.PendingStep);

            kit.Combat.ResumeWithPlayers(new[] { DungeonCombatPlayers.Make(2) });
            Assert.AreEqual(CombatStep.None, kit.Combat.PendingStep);
            kit.Now += 100;
            kit.Combat.Tick(kit.Now);
            Assert.AreEqual(0, kit.Wiped);
        }

        [Test]
        public void Resume_IgnoredUnlessDefeat()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.MONSTER, TwoSlimes, p1);
            kit.Combat.ResumeWithPlayers(new[] { DungeonCombatPlayers.Make(2) });
            Assert.AreEqual(1, kit.Combat.Players.Count);
            Assert.AreEqual(1, kit.Combat.Cycle);
        }

        [Test]
        public void AreaContract_PhaseQueries()
        {
            var kit = new DungeonCombatKit(2);
            IAreaCombat area = kit.Combat;
            Assert.IsFalse(area.IsTurnInProgress);

            var p = DungeonCombatPlayers.Make(1);
            kit.StartForced(RoomType.MONSTER, TwoSlimes, p);
            Assert.IsTrue(area.IsTurnInProgress);
            Assert.IsFalse(area.IsVictory);
            Assert.IsFalse(area.IsBetrayal);

            kit.Combat.HandleEndTurn(p);
            Assert.IsTrue(area.IsTurnInProgress, "몬스터 턴도 진행 중");

            kit.Combat.DebugKillAllMonsters();
            Assert.IsTrue(area.IsVictory);
            Assert.IsFalse(area.IsTurnInProgress);
        }

        [Test]
        public void EndCombat_MidFight_DownedPlayersBackAtOneHp()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var p2 = DungeonCombatPlayers.Make(2);
            var kit = new DungeonCombatKit(2);
            kit.StartForced(RoomType.MONSTER, TwoSlimes, p1, p2);
            DungeonCombatPlayers.Kill(p2);
            p1.Battler.ApplyStatus(StatusEffect.Poison, 2, 2);
            var monsters = new System.Collections.Generic.List<DungeonMonster>(kit.Combat.Monsters);

            kit.Combat.EndCombat();
            Assert.AreEqual(CombatPhase.None, kit.Combat.Phase);
            Assert.AreEqual(1, DungeonCombatPlayers.Hp(p2), "쓰러진 사람은 체력 1");
            Assert.AreEqual(0, p1.Battler.State.Statuses.Count, "전투 효과 지움");
            CollectionAssert.AreEqual(monsters, kit.Removed);
            Assert.AreEqual(0, kit.Combat.Monsters.Count);
            Assert.AreEqual(0, kit.Combat.Players.Count);
            Assert.AreEqual(0, kit.Combat.Cycle);

            kit.Combat.HandleEndTurn(p1);
            Assert.IsFalse(kit.Combat.HasEndedTurn(p1));
        }
    }
}
