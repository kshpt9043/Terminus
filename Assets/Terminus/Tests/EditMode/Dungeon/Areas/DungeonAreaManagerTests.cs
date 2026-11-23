// 구역 배정 / 완료 -> 지도 복귀 / 나간 사람 / 배신 / 관전 (언리얼 UDungeonAreaSubsystem)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using static Terminus.Tests.AreaTestKit;

namespace Terminus.Tests
{
    public class DungeonAreaManagerTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        [Test]
        public void Timers_FireInOrder_AndReplaceOnSameHandle()
        {
            var t = new DungeonTimers();
            var log = new List<string>();
            var a = new TimerHandle();
            var b = new TimerHandle();

            t.Set(a, 2, () => log.Add("a"));
            t.Set(b, 1, () => { log.Add("b"); t.Set(b, 0.5, () => log.Add("b2")); });
            t.Advance(1.9);
            Assert.That(log, Is.EqualTo(new[] { "b", "b2" }), "콜백 안에서 건 타이머는 울린 시각(1.0) 기준");

            t.Set(a, 5, () => log.Add("a-late"));   // 같은 핸들 -> 앞의 것 취소
            t.Advance(3);
            Assert.That(log, Does.Not.Contain("a"));
            t.Advance(3);
            Assert.That(log, Does.Contain("a-late"));
            Assert.That(a.IsActive, Is.False);
        }

        [Test]
        public void ThemeData_PicksRoomTypeSet_ThenDefault_SkippingEmpty()
        {
            var data = new DungeonThemeData(Slime);
            var rng = new System.Random(1);
            Assert.That(data.PickAreaSet(RoomType.MONSTER, rng), Is.Null, "후보가 없으면 null");

            data.AreaSets.Add(new DungeonAreaSet(""));
            data.AreaSets.Add(new DungeonAreaSet("Base"));
            data.RoomTypeAreaSets[RoomType.STORE] = new DungeonAreaSetList { Sets = { new DungeonAreaSet("") } };
            data.RoomTypeAreaSets[RoomType.BREAK] = new DungeonAreaSetList { Sets = { new DungeonAreaSet("Camp") } };

            for (int i = 0; i < 10; ++i)
            {
                Assert.That(data.PickAreaSet(RoomType.MONSTER, rng).Id, Is.EqualTo("Base"), "비워 둔 칸은 건너뜀");
                Assert.That(data.PickAreaSet(RoomType.STORE, rng).Id, Is.EqualTo("Base"), "타입 목록이 다 비었으면 기본 목록");
                Assert.That(data.PickAreaSet(RoomType.BREAK, rng).Id, Is.EqualTo("Camp"));
            }
        }

        [Test]
        public void StartSelectedRooms_GroupsSplitParty_ByRoom_InPlayerOrder()
        {
            var rig = MakeRig(areaCount: 0);
            // 등록 순서와 번호가 달라도 번호순으로 배정
            var c2 = new FakeAreaCombat();
            var c0 = new FakeAreaCombat();
            var area2 = new DungeonArea(2, c2);
            var area0 = new DungeonArea(0, c0);
            rig.Manager.RegisterArea(area2);
            rig.Manager.RegisterArea(area0);

            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var c = MakePlayer(3, "C");
            var rooms = new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.GUARDIAN) };

            var changed = new List<string>();
            rig.Manager.PlayerAreaChanged += (p, ar) => changed.Add($"{p.Name}->{(ar != null ? ar.AreaIndex.ToString() : "map")}");
            var areaChanges = new List<int>();
            rig.Manager.AreaChanged += ar => areaChanges.Add(ar.AreaIndex);

            // B 가 먼저 11 번을 고름 -> 11 번 방이 첫 구역
            Assert.That(Enter(rig, rooms, (b, 11), (a, 10), (c, 11)), Is.True);

            Assert.That(area0.IsInUse && area2.IsInUse, Is.True);
            Assert.That(area0.Room.RoomId, Is.EqualTo(11));
            Assert.That(area0.Occupants, Is.EqualTo(new[] { b, c }));
            Assert.That(area2.Room.RoomId, Is.EqualTo(10));
            Assert.That(area2.Occupants, Is.EqualTo(new[] { a }));

            Assert.That(area0.TryGetSlot(c, out var slot), Is.True);
            Assert.That(slot.bMonsterSide, Is.False);
            Assert.That(slot.Index, Is.EqualTo(1));
            Assert.That(rig.Manager.GetCurrentArea(a), Is.SameAs(area2));
            Assert.That(changed, Is.EqualTo(new[] { "B->0", "C->0", "A->2" }));
            Assert.That(areaChanges, Is.EqualTo(new[] { 0, 2 }), "구역마다 복제할 값이 한 번 바뀜");

            Assert.That(c0.LastRoom.Type, Is.EqualTo(RoomType.GUARDIAN));
            Assert.That(c0.LastPlayers, Is.EqualTo(new[] { b, c }));
            Assert.That(c0.LastTheme, Is.SameAs(Slime));
            Assert.That(area0.CurrentSet.Id, Is.EqualTo("Set_Slime_A"));
            Assert.That(rig.Manager.HasRoomStartSnapshot(a), Is.True);
            Assert.That(rig.Manager.IsAnyRoomInProgress, Is.True);

            // 진행 중엔 새로 시작 안 함
            Assert.That(rig.Manager.StartSelectedRooms(new[] { a }, rooms, Slime), Is.False);
        }

        [Test]
        public void StartSelectedRooms_NotEnoughAreas_DoesNothing()
        {
            var rig = MakeRig(areaCount: 1);
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var rooms = new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.MONSTER) };

            Assert.That(Enter(rig, rooms, (a, 10), (b, 11)), Is.False, "일부만 배정하면 나머지 사람이 붕 뜸 -> 전부 아니면 아예 안 함");
            Assert.That(rig.Areas[0].IsInUse, Is.False);
            Assert.That(rig.Manager.StartSelectedRooms(new List<DungeonPlayer>(), rooms, Slime), Is.False);
        }

        [Test]
        public void EnterSelectedRooms_WithoutAreas_AdvancesImmediately()
        {
            var rig = MakeRig(areaCount: 0);
            var a = MakePlayer(1, "A");
            a.Run.SetSelectedRoomId(5);
            rig.Manager.EnterSelectedRooms(new[] { a }, new[] { Room(5, RoomType.EVENT, row: 3) }, Slime);

            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(5));
            Assert.That(a.RunState.CurrentMapLevel, Is.EqualTo(4));
            Assert.That(a.RunState.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(a.RunState.VisitedRoomIds, Is.EqualTo(new[] { 5 }));
        }

        [Test]
        public void AllAreasMustFinish_ThenEveryoneAdvances_AndReturnsToMap()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var rooms = new[] { Room(10, RoomType.MONSTER, row: 2), Room(11, RoomType.MONSTER, row: 2) };
            Enter(rig, rooms, (a, 10), (b, 11));

            rig.Combats[0].Clear();
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(0), "다른 구역이 아직 싸우는 중");
            Assert.That(rig.Areas[0].IsCleared, Is.True);
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(-1));

            rig.Combats[1].Clear();
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(rig.BossCleared, Is.False);
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(10));
            Assert.That(b.RunState.CurrentRoomId, Is.EqualTo(11));
            Assert.That(a.RunState.CurrentMapLevel, Is.EqualTo(3));
            Assert.That(rig.Areas[0].IsInUse || rig.Areas[1].IsInUse, Is.False);
            Assert.That(rig.Manager.GetCurrentArea(a), Is.Null);
            Assert.That(rig.Combats[0].Count("EndCombat"), Is.EqualTo(1));
            Assert.That(rig.Manager.HasRoomStartSnapshot(a), Is.False);
        }

        [Test]
        public void BossRoomCleared_ReportsBoss()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            Enter(rig, new[] { Room(30, RoomType.BOSS, row: 10) }, (a, 30));
            rig.Combats[0].Clear();

            Assert.That(rig.BossCleared, Is.True);
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(30));
        }

        [Test]
        public void AbortAllRooms_ClosesWithoutProgress()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            Enter(rig, new[] { Room(10, RoomType.STORE) }, (a, 10));
            Assert.That(rig.Areas[0].IsShopping, Is.True);

            rig.Manager.AbortAllRooms();

            Assert.That(rig.Areas[0].IsInUse, Is.False);
            Assert.That(a.RunState.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(-1), "보상 / 진행 없음");
            Assert.That(rig.Notifier.Count("CloseShop", a), Is.EqualTo(1));
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(0));
        }

        [Test]
        public void Leaver_WhileFighting_RollsBackEveryoneToRoomStart()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            a.Run.AddCurrency(100);
            int startHealth = a.Battler.State.Health - 5;
            a.Battler.ApplyDamage(5);

            Enter(rig, new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.STORE) }, (a, 10), (b, 11));

            // 방 안에서 바뀐 것
            a.Run.AddCurrency(-60);
            a.Run.GainRelic("RLC_Common_001");
            a.Battler.ApplyDamage(20);

            rig.Manager.HandlePlayerLeft(b);

            Assert.That(rig.Areas[0].IsInUse || rig.Areas[1].IsInUse, Is.False);
            Assert.That(a.RunState.Currency, Is.EqualTo(100));
            Assert.That(a.RunState.Relics, Does.Not.Contain("RLC_Common_001"));
            Assert.That(a.RunState.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(-1));
            Assert.That(a.Battler.State.Health, Is.EqualTo(startHealth));
            Assert.That(b.RunState.SelectedRoomId, Is.EqualTo(-1), "나간 사람도 되돌림");
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(0));
        }

        [Test]
        public void Leaver_AfterAllFightsWon_AcceptsResult()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Enter(rig, new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.MONSTER) }, (a, 10), (b, 11));

            rig.Combats[0].Win();   // 보상 화면
            rig.Combats[1].Clear();

            rig.Manager.HandlePlayerLeft(b);

            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(10));
            Assert.That(b.RunState.CurrentRoomId, Is.EqualTo(11));
        }

        [Test]
        public void Betrayal_TeamOnPlayerSlots_BetrayerOnMonsterSlot()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var c = MakePlayer(3, "C");
            DungeonPlayer reportedBetrayer = null;
            bool? reportedWin = null;
            rig.Manager.BetrayalFinished += (p, won) => { reportedBetrayer = p; reportedWin = won; };

            // 배신자를 섞어서 넘겨도 팀 먼저, 배신자 마지막
            Assert.That(rig.Manager.StartBetrayal(new[] { a, b, c }, b, Slime, 0.3f, 1), Is.True);

            var area = rig.Areas[0];
            Assert.That(area.Room.Type, Is.EqualTo(RoomType.BOSS));
            Assert.That(area.Room.RoomId, Is.EqualTo(-1));
            Assert.That(area.Occupants, Is.EqualTo(new[] { a, c, b }));
            area.TryGetSlot(b, out var bs);
            area.TryGetSlot(c, out var cs);
            Assert.That(bs.bMonsterSide && bs.Index == 0, Is.True);
            Assert.That(!cs.bMonsterSide && cs.Index == 1, Is.True);

            var combat = rig.Combats[0];
            Assert.That(combat.LastPlayers, Is.EqualTo(new[] { a, c, b }));
            Assert.That(combat.Betrayer, Is.SameAs(b));
            Assert.That(combat.VictimHealthCut, Is.EqualTo(0.3f));
            Assert.That(rig.Manager.IsBetrayalInProgress, Is.True);

            // 진행 중엔 두 번 못 염
            Assert.That(rig.Manager.StartBetrayal(new[] { a, b }, b, Slime, 0.3f, 1), Is.False);

            // 배신 전투 중 누가 나감 -> 전투가 처리, 롤백 안 함
            rig.Manager.HandlePlayerLeft(c);
            Assert.That(combat.BetrayalLeaver, Is.SameAs(c));
            Assert.That(area.IsInUse, Is.True);

            combat.FinishBetrayal(b, true);
            Assert.That(reportedBetrayer, Is.SameAs(b));
            Assert.That(reportedWin, Is.True);
        }

        [Test]
        public void StartBetrayal_NoAreas_ReturnsFalse()
        {
            var rig = MakeRig(areaCount: 0);
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Assert.That(rig.Manager.StartBetrayal(new[] { a, b }, b, Slime, 0.3f, 1), Is.False);
        }

        [Test]
        public void Spectate_OnlyWhenNotFighting_AndOtherAreasActive()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Enter(rig, new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.EVENT) }, (a, 10), (b, 11));

            Assert.That(rig.Manager.CanSpectate(a), Is.False, "싸우는 중");
            Assert.That(rig.Manager.CanSpectate(b), Is.True, "이벤트 방은 턴이 도는 전투가 아님");
            Assert.That(rig.Manager.GetSpectatableAreas(b), Is.EqualTo(new[] { rig.Areas[0] }));
            Assert.That(DungeonAreaManager.GetAreaLabel(rig.Areas[0]), Is.EqualTo("A (몬스터)"));

            rig.Combats[0].Win();
            Assert.That(rig.Manager.CanSpectate(a), Is.True, "이겼으면 다른 구역을 볼 수 있음");

            var dead = MakePlayer(3, "Dead");
            Assert.That(rig.Manager.CanSpectate(dead), Is.True, "구역 밖 사람도 진행 중인 구역을 봄");
        }
    }
}
