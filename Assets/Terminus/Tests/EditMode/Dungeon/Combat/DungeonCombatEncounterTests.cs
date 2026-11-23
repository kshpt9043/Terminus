// 던전 전투: 몬스터 고르기 / 스폰 보정 (실제 DT_Monster 로)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatEncounterTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static DungeonPlayer P(ulong id) => DungeonCombatPlayers.Make(id);

        [Test]
        public void NormalRoom_OneToThreeMonstersOfTheme_WithinSlots()
        {
            var counts = new HashSet<int>();
            for (int seed = 1; seed <= 60; ++seed)
            {
                var kit = new DungeonCombatKit(seed, deterministic: false);
                kit.Start(RoomType.MONSTER, DungeonCombatThemes.Slime, P(1));

                int n = kit.Combat.Monsters.Count;
                Assert.That(n, Is.InRange(1, 3));
                counts.Add(n);
                foreach (var m in kit.Combat.Monsters)
                {
                    Assert.AreEqual(MonsterCategory.Normal, m.Row.MonsterCategory);
                    Assert.AreEqual("슬라임 왕국", m.Row.Theme_KR);
                }
                Assert.AreEqual(n, kit.Spawned.Count, "스폰 알림이 몬스터마다 하나");
                Assert.AreEqual(CombatPhase.PlayerTurn, kit.Combat.Phase);
                Assert.AreEqual(1, kit.Combat.Cycle);
            }
            Assert.AreEqual(3, counts.Count, "1, 2, 3 마리가 다 나와야 함");
        }

        [Test]
        public void NormalRoom_CountCappedBySlots()
        {
            for (int seed = 1; seed <= 20; ++seed)
            {
                var kit = new DungeonCombatKit(seed);
                kit.Host.NumMonsterSlots = 1;
                kit.Start(RoomType.MONSTER, DungeonCombatThemes.Slime, P(1));
                Assert.AreEqual(1, kit.Combat.Monsters.Count);
            }
        }

        [Test]
        public void UnknownTheme_FallsBackToWholeCategory()
        {
            var kit = new DungeonCombatKit(3);
            kit.Start(RoomType.MONSTER, DungeonCombatThemes.Unknown, P(1));
            Assert.That(kit.Combat.Monsters.Count, Is.GreaterThan(0));
            foreach (var m in kit.Combat.Monsters) Assert.AreEqual(MonsterCategory.Normal, m.Row.MonsterCategory);
        }

        [Test]
        public void PartyHealthAndStatBonus_ByPlayersInRoom()
        {
            // mon_slime: HP 25, 공 0, 방 0. 랜덤 끔
            int[] expectedHp = { 25, 43, 60, 80 };   // 25 x (1 + 0 / 0.7 / 1.4 / 2.2), 반올림 (42.5 -> 43)
            for (int party = 1; party <= 4; ++party)
            {
                var kit = new DungeonCombatKit();
                var players = new List<DungeonPlayer>();
                for (int i = 0; i < party; ++i) players.Add(P((ulong)(i + 1)));
                kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, players.ToArray());

                var m = kit.Combat.Monsters[0];
                Assert.AreEqual(expectedHp[party - 1], m.Stats.Stats.MaxHealth, $"{party}인");
                Assert.AreEqual(expectedHp[party - 1], m.Stats.State.Health);
                Assert.AreEqual(party, m.Stats.Stats.Attack, "1명당 공격 +1");
                Assert.AreEqual(party, m.Stats.Stats.Defense, "1명당 방어 +1");
            }
        }

        [Test]
        public void SecondFloorOfTheme_BonusForNormalNotBoss()
        {
            var kit = new DungeonCombatKit();
            kit.Host.CurrentFloor = 2;
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slimeking" }, P(1));

            var slime = kit.Combat.Monsters[0];
            Assert.AreEqual(33, slime.Stats.Stats.MaxHealth, "25 x 1.3 = 32.5 -> 33");
            Assert.AreEqual(0 + 1 + 1, slime.Stats.Stats.Attack, "층 +1, 인원 +1");

            var king = kit.Combat.Monsters[1];
            Assert.AreEqual(100, king.Stats.Stats.MaxHealth, "보스는 층 보정 없음");
            Assert.AreEqual(2 + 1, king.Stats.Stats.Attack, "보스는 인원 보정만");
            Assert.AreEqual(3 + 1, king.Stats.Stats.Defense);
        }

        [Test]
        public void FirstFloor_NoSecondFloorBonus()
        {
            var kit = new DungeonCombatKit();
            kit.Host.CurrentFloor = 3;
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, P(1));
            Assert.AreEqual(25, kit.Combat.Monsters[0].Stats.Stats.MaxHealth);
        }

        [Test]
        public void RandomRolls_WithinDesignRange_BossHasNoStatRoll()
        {
            for (int seed = 1; seed <= 40; ++seed)
            {
                var kit = new DungeonCombatKit(seed, deterministic: false);
                kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slimeking" }, P(1));

                var slime = kit.Combat.Monsters[0];
                Assert.That(slime.Stats.Stats.MaxHealth, Is.InRange(24, 28), "25 x (0.95 ~ 1.10)");
                Assert.That(slime.Stats.Stats.Attack, Is.InRange(1, 3), "인원 +1, 랜덤 0~2");
                Assert.That(slime.Stats.Stats.Defense, Is.InRange(1, 3));

                var king = kit.Combat.Monsters[1];
                Assert.That(king.Stats.Stats.MaxHealth, Is.InRange(95, 110), "보스도 체력 랜덤 (10-08)");
                Assert.AreEqual(3, king.Stats.Stats.Attack, "보스는 공방 랜덤 없음");
                Assert.AreEqual(4, king.Stats.Stats.Defense);
            }
        }

        [Test]
        public void NextSpawnHealthScale_AppliesOnceThenResets()
        {
            var kit = new DungeonCombatKit();
            var p = P(1);
            kit.Combat.SetNextSpawnHealthScale(0.7f);
            kit.StartForced(RoomType.GUARDIAN, new[] { "mon_slime" }, p);
            Assert.AreEqual(18, kit.Combat.Monsters[0].Stats.Stats.MaxHealth, "25 x 0.7 = 17.5 -> 18");

            kit.StartForced(RoomType.GUARDIAN, new[] { "mon_slime" }, p);
            Assert.AreEqual(25, kit.Combat.Monsters[0].Stats.Stats.MaxHealth);
        }

        [Test]
        public void ForcedMonsters_UsedInOrderOnce_NotCappedBySlots()
        {
            var kit = new DungeonCombatKit();
            var p = P(1);
            kit.Combat.SetNextForcedMonsters(new[] { "mon_mudslime", "mon_slime", "mon_redslime", "mon_iceslime" });
            kit.Start(RoomType.MONSTER, DungeonCombatThemes.Lizard, p);

            var rows = kit.Combat.Monsters.ConvertAllRows();
            CollectionAssert.AreEqual(new[] { "mon_mudslime", "mon_slime", "mon_redslime", "mon_iceslime" }, rows);
            for (int i = 0; i < kit.Combat.Monsters.Count; ++i)
            {
                Assert.AreEqual(i, kit.Combat.Monsters[i].Index);
                Assert.AreEqual(i, kit.Combat.Monsters[i].SlotIndex);
            }

            // 다음 전투는 평소대로 (리자드 테마 일반)
            kit.Start(RoomType.MONSTER, DungeonCombatThemes.Lizard, p);
            foreach (var m in kit.Combat.Monsters) Assert.AreEqual("리자드의 늪", m.Row.Theme_KR);
            Assert.AreEqual(4, kit.Removed.Count, "앞 전투 몬스터는 제거 알림");
        }

        [Test]
        public void ForcedMonsters_UnknownRowSkipped_SlotIndexKeepsRowPosition()
        {
            var kit = new DungeonCombatKit();
            kit.StartForced(RoomType.MONSTER, new[] { "mon_nope", "mon_slime" }, P(1));
            Assert.AreEqual(1, kit.Combat.Monsters.Count);
            Assert.AreEqual(0, kit.Combat.Monsters[0].Index);
            Assert.AreEqual(1, kit.Combat.Monsters[0].SlotIndex, "언리얼 GetMonsterSlotLocation(i) 는 행 순서");
        }

        [Test]
        public void NoMonsters_ImmediateVictory_ThenRewards()
        {
            var kit = new DungeonCombatKit();
            var p = P(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_nope" }, p);
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.AreEqual(CombatStep.StartRoomRewards, kit.Combat.PendingStep);
        }

        [Test]
        public void NonCombatRoom_DoesNothing()
        {
            var kit = new DungeonCombatKit();
            kit.Start(RoomType.STORE, DungeonCombatThemes.Slime, P(1));
            Assert.AreEqual(CombatPhase.None, kit.Combat.Phase);
            Assert.IsFalse(kit.Combat.IsInCombat());
            Assert.AreEqual(0, kit.Combat.Monsters.Count);
        }

        [Test]
        public void Guardian_NoRepeatUntilAllSeen_ThenStartsOver()
        {
            var kit = new DungeonCombatKit(5);
            var session = new RunSession(new System.Random(1));
            kit.Host.RunSession = session;
            var p = P(1);

            var seen = new List<string>();
            for (int i = 0; i < 4; ++i)
            {
                kit.Start(RoomType.GUARDIAN, DungeonCombatThemes.Slime, p);
                var leader = kit.Combat.Monsters[0];
                Assert.AreEqual(MonsterCategory.Guardian, leader.Row.MonsterCategory);
                CollectionAssert.DoesNotContain(seen, leader.RowName, "테마 가디언이 다 나오기 전엔 중복 없음");
                seen.Add(leader.RowName);
            }
            Assert.AreEqual(4, session.SeenGuardians.Count);

            kit.Start(RoomType.GUARDIAN, DungeonCombatThemes.Slime, p);
            Assert.AreEqual(1, session.SeenGuardians.Count, "다 나왔으면 잊고 처음부터");
        }

        [Test]
        public void Guardian_SlimeGeneral_BringsTwoNormalSlimes()
        {
            var kit = new DungeonCombatKit(2);
            var session = new RunSession(new System.Random(1));
            session.AddSeenGuardian("mon_metalslime");
            session.AddSeenGuardian("mon_acidslime");
            session.AddSeenGuardian("mon_ancientslime");
            kit.Host.RunSession = session;

            kit.Start(RoomType.GUARDIAN, DungeonCombatThemes.Slime, P(1));
            Assert.AreEqual("mon_slimegeneral", kit.Combat.Monsters[0].RowName);
            Assert.AreEqual(3, kit.Combat.Monsters.Count);
            for (int i = 1; i < 3; ++i)
            {
                Assert.AreEqual(MonsterCategory.Normal, kit.Combat.Monsters[i].Row.MonsterCategory);
                Assert.AreEqual("슬라임 왕국", kit.Combat.Monsters[i].Row.Theme_KR);
            }
        }

        [Test]
        public void Guardian_Companions_LimitedBySlots_AndFixedRow()
        {
            var kit = new DungeonCombatKit(2);
            var session = new RunSession(new System.Random(1));
            session.AddSeenGuardian("mon_nereid");
            session.AddSeenGuardian("mon_jinn");
            session.AddSeenGuardian("mon_clay");
            kit.Host.RunSession = session;
            kit.Host.NumMonsterSlots = 3;

            kit.Start(RoomType.GUARDIAN, DungeonCombatThemes.Elemental, P(1));
            CollectionAssert.AreEqual(new[] { "mon_ignis", "mon_salamander" }, kit.Combat.Monsters.ConvertAllRows(), "이그니스 = 샐러맨더 1");

            // 자리가 1 이면 같이 나오는 몬스터 없음 (네 가디언이 다 나와서 처음부터 다시 고름)
            kit.Host.NumMonsterSlots = 1;
            kit.Start(RoomType.GUARDIAN, DungeonCombatThemes.Elemental, P(1));
            Assert.AreEqual(1, kit.Combat.Monsters.Count);
        }

        [Test]
        public void Boss_PickedByThemeFloor_WithCompanions()
        {
            var kit = new DungeonCombatKit(1);
            kit.Host.CurrentFloor = 1;
            kit.Start(RoomType.BOSS, DungeonCombatThemes.Slime, P(1));
            CollectionAssert.AreEqual(new[] { "mon_fusionslime" }, kit.Combat.Monsters.ConvertAllRows(), "1층 = 테마 첫 층 보스");

            kit.Host.CurrentFloor = 2;
            kit.Start(RoomType.BOSS, DungeonCombatThemes.Slime, P(1));
            Assert.AreEqual("mon_slimeking", kit.Combat.Monsters[0].RowName);
            Assert.AreEqual(2, kit.Combat.Monsters.Count, "슬라임 킹 = 가디언 1");
            Assert.AreEqual(MonsterCategory.Guardian, kit.Combat.Monsters[1].Row.MonsterCategory);

            // 리자드: AppearFloor "3층" 은 그 층 그대로
            kit.Host.CurrentFloor = 3;
            kit.Start(RoomType.BOSS, DungeonCombatThemes.Lizard, P(1));
            Assert.AreEqual("mon_lizardchieftain", kit.Combat.Monsters[0].RowName);
            Assert.AreEqual(3, kit.Combat.Monsters.Count, "족장 + 일반 2 (자리 3)");
        }

        [Test]
        public void MonsterPassiveRelics_FromPassiveId()
        {
            CollectionAssert.AreEqual(new[] { "RLC_Monster_SLI01" }, DungeonMonster.GetPassiveRelics(GameData.FindMonsterRow("mon_mudslime")));
            CollectionAssert.IsEmpty(DungeonMonster.GetPassiveRelics(GameData.FindMonsterRow("mon_slime")));
            Assert.AreEqual(2, DungeonMonster.ParseAppearFloor("2층"));
            Assert.AreEqual(0, DungeonMonster.ParseAppearFloor("심층"));
            Assert.AreEqual(0, DungeonMonster.ParseAppearFloor(""));
        }

        [Test]
        public void IntentKind_FromSkillTypeAndTarget()
        {
            Assert.AreEqual(MonsterIntentKind.Attack, DungeonMonster.ToIntentKind(GameData.FindSkillRow("slime_strike")));
            Assert.AreEqual(MonsterIntentKind.Defend, DungeonMonster.ToIntentKind(GameData.FindSkillRow("condense")));
            Assert.AreEqual(MonsterIntentKind.Buff, DungeonMonster.ToIntentKind(GameData.FindSkillRow("kings_roar")));
            Assert.AreEqual(MonsterIntentKind.Buff, DungeonMonster.ToIntentKind(GameData.FindSkillRow("metallic")));
        }
    }

    internal static class DungeonCombatListExt
    {
        public static List<string> ConvertAllRows(this IReadOnlyList<DungeonMonster> monsters)
        {
            var rows = new List<string>();
            foreach (var m in monsters) rows.Add(m.RowName);
            return rows;
        }
    }
}
