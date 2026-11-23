// 던전 전투: 배신 전투 (팀 차례 -> 배신자 차례, 보정, 대상 편 규칙, 결과)
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatBetrayalTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        DungeonCombatKit kit;
        DungeonPlayer a, b, traitor;

        [SetUp]
        public void Begin()
        {
            kit = new DungeonCombatKit(31);
            a = DungeonCombatPlayers.Make(1);
            b = DungeonCombatPlayers.Make(2, CharacterClass.Paladin);
            traitor = DungeonCombatPlayers.Make(3, CharacterClass.Fighter, false, "RLC_Common_003");
            // 순서 = 구역 Occupants: 팀 먼저, 배신자 마지막
            kit.Combat.StartBetrayal(new[] { a, b, traitor }, traitor, 0.5f, 2);
        }

        [Test]
        public void Start_BonusesAndSides()
        {
            Assert.IsTrue(kit.Combat.IsBetrayal());
            Assert.AreSame(traitor, kit.Combat.Betrayer);
            Assert.AreSame(traitor, kit.Combat.GetBetrayer());
            Assert.AreEqual(0, kit.Combat.Monsters.Count);
            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
            Assert.AreEqual(1, kit.Combat.Cycle);

            Assert.AreEqual(35, DungeonCombatPlayers.Hp(a), "현재 체력의 절반을 잃음 (70 -> 35)");
            Assert.AreEqual(35, DungeonCombatPlayers.Hp(b));
            Assert.AreEqual(70, DungeonCombatPlayers.Hp(traitor));
            Assert.AreEqual(4, traitor.Battler.GetStatusValue(StatusEffect.Brave), "상대 2명 x 2");
            Assert.AreEqual(4, traitor.Battler.GetStatusValue(StatusEffect.Solid));

            Assert.IsTrue(kit.Combat.IsSameSide(a, b));
            Assert.IsFalse(kit.Combat.IsSameSide(a, traitor));
            Assert.IsTrue(kit.Combat.IsSameSide(traitor, traitor));
        }

        [Test]
        public void TurnOrder_TeamThenBetrayerThenNextCycle()
        {
            Assert.IsFalse(kit.Combat.IsBetrayerTurn());
            Assert.IsTrue(kit.Combat.IsTurnOf(a));
            Assert.IsTrue(kit.Combat.IsTurnOf(b));
            Assert.IsFalse(kit.Combat.IsTurnOf(traitor));

            // 배신자는 팀 차례에 못 움직임
            kit.Combat.HandleUseSkill(traitor, 0, 0);
            Assert.AreEqual(35, DungeonCombatPlayers.Hp(a));
            kit.Combat.HandleEndTurn(traitor);
            Assert.IsFalse(kit.Combat.HasEndedTurn(traitor));

            kit.Combat.HandleEndTurn(a);
            Assert.IsFalse(kit.Combat.IsBetrayerTurn());
            kit.Combat.HandleEndTurn(b);

            Assert.IsTrue(kit.Combat.IsBetrayerTurn());
            Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase, "몬스터 턴 자리에 배신자 턴");
            Assert.IsTrue(kit.Combat.IsTurnOf(traitor));
            Assert.IsFalse(kit.Combat.IsTurnOf(a));
            Assert.AreEqual(3, traitor.Battler.State.Energy, "배신자 에너지는 자기 턴 시작에 채움");
            Assert.AreEqual(CombatStep.None, kit.Combat.PendingStep, "타이머 없이 바로");

            kit.Combat.HandleEndTurn(traitor);
            Assert.AreEqual(2, kit.Combat.Cycle);
            Assert.IsFalse(kit.Combat.IsBetrayerTurn());
            Assert.IsTrue(kit.Combat.IsTurnOf(a));
            Assert.AreEqual(3, a.Battler.State.Energy);
        }

        [Test]
        public void Targets_EnemyIsOtherSide_AllyIsSameSide()
        {
            // 팀원 a: 적 1명 = 상대 편만. 같은 편(b, 인덱스 1)은 못 때림
            kit.Combat.HandleUseSkill(a, 0, 1);
            Assert.AreEqual(35, DungeonCombatPlayers.Hp(b));
            Assert.AreEqual(3, a.Battler.State.Energy);

            kit.Combat.HandleUseSkill(a, 0, 2);
            Assert.AreEqual(70 - 6, DungeonCombatPlayers.Hp(traitor));

            // 성기사 b: 아군 회복은 같은 편만 (배신자 인덱스 2 는 안 됨)
            kit.Combat.HandleUseSkill(b, 2, 2);
            Assert.AreEqual(70 - 6, DungeonCombatPlayers.Hp(traitor));
            kit.Combat.HandleUseSkill(b, 2, 0);
            Assert.AreEqual(35 + 2, DungeonCombatPlayers.Hp(a));

            kit.Combat.HandleEndTurn(a);
            kit.Combat.HandleEndTurn(b);

            // 배신자: 적 = 팀. 공격 (3 + 용기 4) x 2
            kit.Combat.HandleUseSkill(traitor, 0, 1);
            Assert.AreEqual(35 - 14, DungeonCombatPlayers.Hp(b));
        }

        [Test]
        public void BetrayerDown_TeamWins_ResultReportedAfterDelay()
        {
            DungeonCombatPlayers.Kill(traitor);
            kit.Combat.HandleEndTurn(a);
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.IsFalse(kit.Combat.DidBetrayerWin());
            Assert.AreEqual(CombatStep.ReportBetrayalResult, kit.Combat.PendingStep);

            kit.Step();
            Assert.AreEqual(1, kit.BetrayalResults.Count);
            Assert.AreSame(traitor, kit.BetrayalResults[0].Key);
            Assert.IsFalse(kit.BetrayalResults[0].Value);
            Assert.AreEqual(0, kit.Offers.Count, "배신 전투는 보상 없음");
            Assert.AreEqual(0, kit.Cleared, "구역 클리어 없음");
        }

        [Test]
        public void TeamDown_BetrayerWins()
        {
            DungeonCombatPlayers.Kill(a);
            kit.Combat.HandleEndTurn(b);   // b 가 마지막 -> 배신자 차례
            Assert.IsTrue(kit.Combat.IsBetrayerTurn());
            DungeonCombatPlayers.Kill(b);
            kit.Combat.HandleEndTurn(traitor);
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.IsTrue(kit.Combat.DidBetrayerWin());
            kit.Step();
            Assert.IsTrue(kit.BetrayalResults[0].Value);
        }

        [Test]
        public void Leaver_BetrayerLeaves_TeamWins()
        {
            traitor.bConnected = false;
            kit.Combat.HandleBetrayalLeaver(traitor);
            Assert.IsTrue(traitor.IsDead());
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.IsFalse(kit.Combat.DidBetrayerWin());
        }

        [Test]
        public void Leaver_LastPendingTeamMember_AdvancesToBetrayer()
        {
            kit.Combat.HandleEndTurn(a);
            kit.Combat.HandleBetrayalLeaver(b);
            Assert.IsTrue(b.IsDead(), "나간 사람은 쓰러진 걸로");
            Assert.AreEqual(3, kit.Combat.Players.Count, "목록에서 빼진 않음 (대상 인덱스 유지)");
            Assert.IsTrue(kit.Combat.IsBetrayerTurn());
        }

        [Test]
        public void BetrayerBattleStartRelic_FiresBeforeOwnRefill()
        {
            // 언리얼 그대로: 배신자의 전투 시작 유물은 팀 차례 시작에 발동하고, 배신자 에너지는 자기 턴에 다시 채움
            Assert.AreEqual(3 + 1, traitor.Battler.State.Energy);
            kit.Combat.HandleEndTurn(a);
            kit.Combat.HandleEndTurn(b);
            Assert.IsTrue(kit.Combat.IsBetrayerTurn());
            Assert.AreEqual(3, traitor.Battler.State.Energy, "보조 배터리 +1 은 배신자 첫 턴엔 남지 않음");
        }
    }
}
