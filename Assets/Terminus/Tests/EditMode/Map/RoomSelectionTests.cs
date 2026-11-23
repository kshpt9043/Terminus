// 방 고르기 규칙 (언리얼 HandleSelectRoomRequest / IsValidNextRoom / CheckAllPlayersReadyAndStart / AdvanceToRoom)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Tests
{
    public class RoomSelectionTests
    {
        // 0줄: 1(퀘스트) / 1줄: 2(정원 1), 3(정원 2) / 2줄: 4(보스)
        static List<RoomNode> SmallMap()
        {
            var q = MapGenerator.CreateRoom(1, 0, 1, RoomType.QUEST, 4);
            var a = MapGenerator.CreateRoom(2, 1, 0, RoomType.MONSTER, 1);
            var b = MapGenerator.CreateRoom(3, 1, 1, RoomType.MONSTER, 2);
            var boss = MapGenerator.CreateRoom(4, 2, 1, RoomType.BOSS, 4);
            q.ConnectedRoomIds.AddRange(new[] { 2, 3 });
            a.ConnectedRoomIds.Add(4);
            b.ConnectedRoomIds.Add(4);
            return new List<RoomNode> { q, a, b, boss };
        }

        static RunState ReadyRun() => new RunState { bStartSkillChosen = true, bStartRelicsChosen = true };

        static RoomSelectRequest Req(int roomId, RunState run, params int[] others)
        {
            return new RoomSelectRequest { RoomId = roomId, RequesterRun = run, OtherSelectedRoomIds = new List<int>(others) };
        }

        [Test]
        public void IsValidNextRoom_StartOnlyRow0_ThenConnected()
        {
            var map = SmallMap();
            var run = ReadyRun();
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[0]), Is.True);
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[1]), Is.False);

            RoomSelectionRules.AdvanceToRoom(run, 1, 0);
            Assert.That(run.CurrentMapLevel, Is.EqualTo(1));
            Assert.That(run.VisitedRoomIds, Is.EqualTo(new[] { 1 }));
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[1]), Is.True);
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[2]), Is.True);
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[3]), Is.False, "건너뛰기 불가");

            run.CurrentRoomId = 99;
            Assert.That(RoomSelectionRules.IsValidNextRoom(map, run, map[1]), Is.False, "지금 방이 지도에 없음");
        }

        [Test]
        public void Evaluate_ChecksInUnrealOrder()
        {
            var map = SmallMap();

            Assert.That(RoomSelectionRules.EvaluateSelectRoom(map, Req(42, ReadyRun())).Outcome, Is.EqualTo(RoomSelectOutcome.Ignored), "없는 방은 조용히 무시");

            var notReady = new RunState { bStartSkillChosen = true };
            var r = RoomSelectionRules.EvaluateSelectRoom(map, Req(1, notReady));
            Assert.That(r.Outcome, Is.EqualTo(RoomSelectOutcome.Rejected));
            Assert.That(r.FailMessage, Is.EqualTo(RoomSelectionRules.MsgStartChoicesRequired));

            var req = Req(1, ReadyRun());
            req.bFloorVoteActive = true;
            req.bHasDepartedPlayers = true;
            Assert.That(RoomSelectionRules.EvaluateSelectRoom(map, req).FailMessage, Is.EqualTo(RoomSelectionRules.MsgFloorVoteActive));
            req.bFloorVoteActive = false;
            Assert.That(RoomSelectionRules.EvaluateSelectRoom(map, req).FailMessage, Is.EqualTo(RoomSelectionRules.MsgDepartedPlayers));
            req.bHasDepartedPlayers = false;
            req.bAnyRoomInProgress = true;
            Assert.That(RoomSelectionRules.EvaluateSelectRoom(map, req).FailMessage, Is.EqualTo(RoomSelectionRules.MsgRoomInProgress));

            Assert.That(RoomSelectionRules.EvaluateSelectRoom(map, Req(2, ReadyRun())).FailMessage, Is.EqualTo(RoomSelectionRules.MsgInvalidPath));
        }

        [Test]
        public void Evaluate_Single_EntersImmediately_IgnoresCapacity()
        {
            var map = SmallMap();
            var run = ReadyRun();
            RoomSelectionRules.AdvanceToRoom(run, 1, 0);
            var req = Req(2, run, 2, 2);
            req.bSinglePlayerRun = true;
            var r = RoomSelectionRules.EvaluateSelectRoom(map, req);
            Assert.That(r.Outcome, Is.EqualTo(RoomSelectOutcome.EnterNow));
            Assert.That(r.NewSelectedRoomId, Is.EqualTo(2));
        }

        [Test]
        public void Evaluate_Multi_Toggle_Capacity_Select()
        {
            var map = SmallMap();
            var run = ReadyRun();
            RoomSelectionRules.AdvanceToRoom(run, 1, 0);

            // 방 2 정원 1: 다른 사람이 이미 고름 -> 가득 참
            var full = RoomSelectionRules.EvaluateSelectRoom(map, Req(2, run, 2));
            Assert.That(full.Outcome, Is.EqualTo(RoomSelectOutcome.Rejected));
            Assert.That(full.FailMessage, Is.EqualTo(RoomSelectionRules.MsgRoomFull));

            // 방 3 정원 2: 한 명 있음 -> 들어감
            var ok = RoomSelectionRules.EvaluateSelectRoom(map, Req(3, run, 3, -1));
            Assert.That(ok.Outcome, Is.EqualTo(RoomSelectOutcome.Selected));
            Assert.That(ok.NewSelectedRoomId, Is.EqualTo(3));

            // 같은 방을 다시 누르면 취소 (정원 검사보다 먼저)
            run.SelectedRoomId = 3;
            var toggle = RoomSelectionRules.EvaluateSelectRoom(map, Req(3, run, 3, 3));
            Assert.That(toggle.Outcome, Is.EqualTo(RoomSelectOutcome.Deselected));
            Assert.That(toggle.NewSelectedRoomId, Is.EqualTo(-1));

            // 다른 방에서 옮겨올 때 내 기존 선택은 안 셈 (OtherSelectedRoomIds 에 나는 없음)
            var move = RoomSelectionRules.EvaluateSelectRoom(map, Req(2, run, 3));
            Assert.That(move.Outcome, Is.EqualTo(RoomSelectOutcome.Selected));
        }

        [Test]
        public void IsSinglePlayerRun_Rules()
        {
            Assert.That(RoomSelectionRules.IsSinglePlayerRun(1, false), Is.True);
            Assert.That(RoomSelectionRules.IsSinglePlayerRun(1, true), Is.False, "나간 사람을 기다리면 멀티");
            Assert.That(RoomSelectionRules.IsSinglePlayerRun(2, false), Is.False);
            Assert.That(RoomSelectionRules.IsSinglePlayerRun(null, false), Is.False, "모르면 멀티 (전원 대기 쪽이 안전)");
        }

        [Test]
        public void CheckAllPlayersReady_Cases()
        {
            Assert.That(RoomSelectionRules.CheckAllPlayersReady(new int[0], null, out _), Is.EqualTo(PartyReadyResult.NotReady));
            Assert.That(RoomSelectionRules.CheckAllPlayersReady(new[] { 2, -1 }, null, out _), Is.EqualTo(PartyReadyResult.NotReady));
            Assert.That(RoomSelectionRules.CheckAllPlayersReady(new[] { 2, 3 }, new string[0], out string n0), Is.EqualTo(PartyReadyResult.Start));
            Assert.That(n0, Is.Empty);

            var r = RoomSelectionRules.CheckAllPlayersReady(new[] { 2, 3 }, new[] { "철수", "영희" }, out string notice);
            Assert.That(r, Is.EqualTo(PartyReadyResult.WaitingForDeparted));
            Assert.That(notice, Is.EqualTo("철수, 영희 님이 돌아올 때까지 출발할 수 없습니다."));
        }

        [Test]
        public void BeginFloor_ResetsMapProgress()
        {
            var run = ReadyRun();
            RoomSelectionRules.AdvanceToRoom(run, 5, 3);
            run.SelectedRoomId = 7;
            run.BossRelic = "RLC_X";
            run.BossSkill = "SK_X";
            run.FloorMonsterRooms = 2;
            run.FloorGuardianRooms = 1;

            RoomSelectionRules.BeginFloor(run);

            Assert.That(run.CurrentRoomId, Is.EqualTo(-1));
            Assert.That(run.CurrentMapLevel, Is.EqualTo(0));
            Assert.That(run.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(run.VisitedRoomIds, Is.Empty);
            Assert.That(run.BossRelic, Is.Empty);
            Assert.That(run.BossSkill, Is.Empty);
            Assert.That(run.FloorMonsterRooms + run.FloorGuardianRooms, Is.EqualTo(0));
        }

        [Test]
        public void GeneratedMap_WalkFromStartToBoss()
        {
            // 만든 지도를 실제 규칙으로 처음부터 보스까지 걸어 봄
            var rng = new System.Random(9);
            for (int floor = 1; floor <= 2; ++floor)
            {
                var map = new MapGenerator(new MapSettings(), new System.Random(floor)).GenerateMap(floor, 1);
                var run = ReadyRun();
                RoomNode current = null;
                for (int step = 0; step < 20; ++step)
                {
                    var options = map.FindAll(r => RoomSelectionRules.IsValidNextRoom(map, run, r));
                    Assert.That(options, Is.Not.Empty, $"floor {floor} step {step} 갈 곳 없음");
                    current = options[rng.Next(options.Count)];
                    var res = RoomSelectionRules.EvaluateSelectRoom(map, new RoomSelectRequest { RoomId = current.RoomId, RequesterRun = run, bSinglePlayerRun = true });
                    Assert.That(res.Outcome, Is.EqualTo(RoomSelectOutcome.EnterNow));
                    RoomSelectionRules.AdvanceToRoom(run, current.RoomId, current.Row);
                    if (current.Type == RoomType.BOSS) break;
                }
                Assert.That(current.Type, Is.EqualTo(RoomType.BOSS));
            }
        }
    }
}
