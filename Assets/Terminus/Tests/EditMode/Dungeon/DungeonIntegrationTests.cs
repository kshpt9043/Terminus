// 구역(DungeonAreaManager / DungeonArea) + 진짜 전투(DungeonCombat) 를 붙여서 방 하나를 끝까지 돌림
// 두 모듈을 따로 옮겼으니 이음새 (IAreaCombat: 시작 / 클리어 / 전멸 이벤트, 몬스터 칸 수) 를 여기서 확인
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    public class DungeonIntegrationTests
    {
        sealed class World
        {
            public DungeonAreaManager Manager;
            public DungeonTimers Timers;
            public readonly List<DungeonCombat> Combats = new List<DungeonCombat>();
            public double Now;
            public int RoomsFinished;
            public bool BossCleared;
            public string WipeReason;
            public readonly List<(DungeonPlayer player, RoomRewardOffer offer)> Offers = new List<(DungeonPlayer, RoomRewardOffer)>();

            // 게임 쪽이 매 프레임 하는 일: 구역 타이머 + 구역마다 전투 Tick
            public void Pump(double seconds, double step = 0.1)
            {
                double end = Now + seconds;
                while (Now < end)
                {
                    Now += step;
                    Timers.Tick(Now);
                    foreach (var c in Combats) c.Tick(Now);
                }
            }
        }

        [OneTimeSetUp]
        public void Load() => TestData.LoadGameData();

        static World MakeWorld(int areaCount, int floor = 1)
        {
            var w = new World { Timers = new DungeonTimers() };
            w.Manager = new DungeonAreaManager(new DungeonAreaSettings(), new Random(11), w.Timers, new RecordingNotifier());
            w.Manager.CurrentFloorProvider = () => floor;
            w.Manager.RoomsFinished += boss => { w.BossCleared = boss; ++w.RoomsFinished; };
            w.Manager.PartyWiped += reason => w.WipeReason = reason;

            var theme = new DungeonThemeData(AreaTestKit.Slime);
            theme.AreaSets.Add(new DungeonAreaSet("Set_Slime_A"));
            w.Manager.RegisterTheme(theme);

            for (int i = 0; i < areaCount; ++i)
            {
                const int monsterSlots = 3;
                var combat = new DungeonCombat(new DungeonCombatSettings(),
                    new DungeonCombatHost { CurrentFloor = floor, NumMonsterSlots = monsterSlots }, new Random(100 + i));
                combat.RoomRewardOffered += (p, offer) => w.Offers.Add((p, offer));
                w.Combats.Add(combat);
                w.Manager.RegisterArea(new DungeonArea(i, combat, 4, monsterSlots));
            }
            return w;
        }

        static void Pick(DungeonPlayer p, int roomId) => p.Run.SetSelectedRoomId(roomId);

        [Test]
        public void SplitParty_TwoMonsterRooms_FightRewardAndReturnToMap()
        {
            var w = MakeWorld(2);
            var a = AreaTestKit.MakePlayer(1, "가", CharacterClass.Fighter, 1);
            var b = AreaTestKit.MakePlayer(2, "나", CharacterClass.Assassin, 2);
            var rooms = new List<RoomNode> { AreaTestKit.Room(10, RoomType.MONSTER), AreaTestKit.Room(11, RoomType.MONSTER) };
            Pick(a, 10);
            Pick(b, 11);

            Assert.IsTrue(w.Manager.StartSelectedRooms(new[] { a, b }, rooms, AreaTestKit.Slime));
            var areaA = w.Manager.GetCurrentArea(a);
            var areaB = w.Manager.GetCurrentArea(b);
            Assert.AreNotSame(areaA, areaB, "갈라진 파티는 구역도 따로");

            var combatA = (DungeonCombat)areaA.Combat;
            var combatB = (DungeonCombat)areaB.Combat;
            Assert.AreEqual(CombatPhase.PlayerTurn, combatA.Phase);
            Assert.AreEqual(CombatPhase.PlayerTurn, combatB.Phase);
            Assert.That(combatA.Monsters.Count, Is.InRange(1, 3), "몬스터방은 1~3마리 (구역 몬스터 칸 3)");
            foreach (var m in combatA.Monsters) Assert.AreEqual("슬라임 왕국", m.Row.Theme_KR);
            Assert.IsTrue(w.Manager.IsAnyRoomInProgress);

            // 한쪽만 이기고 보상을 마쳐도 지도로 안 돌아감 (모든 구역이 끝나야)
            combatA.DebugKillAllMonsters();
            w.Pump(3);
            Assert.AreEqual(CombatPhase.Victory, combatA.Phase);
            Assert.IsTrue(w.Offers.Exists(o => o.player == a), "승리하면 보상 제안");
            int currencyBefore = a.RunState.Currency;
            combatA.HandleRewardFinished(a, "", -1, "", "");
            w.Pump(3);
            Assert.IsTrue(areaA.IsCleared);
            Assert.AreEqual(0, w.RoomsFinished, "다른 구역이 싸우는 중");
            Assert.GreaterOrEqual(a.RunState.Currency, currencyBefore);

            combatB.DebugKillAllMonsters();
            w.Pump(3);
            combatB.HandleRewardFinished(b, "", -1, "", "");
            w.Pump(5);

            Assert.AreEqual(1, w.RoomsFinished, "두 구역 다 끝나면 지도로");
            Assert.IsFalse(w.BossCleared);
            Assert.AreEqual(10, a.RunState.CurrentRoomId);
            Assert.AreEqual(11, b.RunState.CurrentRoomId);
            Assert.IsNull(w.Manager.GetCurrentArea(a), "구역에서 나옴");
            Assert.IsFalse(w.Manager.IsAnyRoomInProgress);
        }

        [Test]
        public void Solo_Wipe_EndsRun()
        {
            var w = MakeWorld(1);
            var a = AreaTestKit.MakePlayer(1, "가");
            Pick(a, 10);
            Assert.IsTrue(w.Manager.StartSelectedRooms(new[] { a }, new List<RoomNode> { AreaTestKit.Room(10, RoomType.MONSTER) }, AreaTestKit.Slime));

            ((DungeonCombat)w.Manager.GetCurrentArea(a).Combat).DebugKillAllPlayers();
            w.Pump(10);

            Assert.IsNotNull(w.WipeReason, "싱글 전멸이면 런 끝");
            Assert.AreEqual(0, w.RoomsFinished);
        }

        [Test]
        public void GuardianRoom_SpawnsGuardian()
        {
            var w = MakeWorld(1);
            var a = AreaTestKit.MakePlayer(1, "가");
            Pick(a, 20);
            Assert.IsTrue(w.Manager.StartSelectedRooms(new[] { a }, new List<RoomNode> { AreaTestKit.Room(20, RoomType.GUARDIAN) }, AreaTestKit.Slime));

            var combat = (DungeonCombat)w.Manager.GetCurrentArea(a).Combat;
            Assert.IsTrue(System.Linq.Enumerable.Any(combat.Monsters, m => m.Row.MonsterCategory == MonsterCategory.Guardian));
        }
    }
}
