// 런 세이브 / 이어하기 / 런 세션
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunSaveServiceTests
    {
        MemorySaveStorage _storage;
        RunXmlTestSerializer _serializer;
        DateTime _now;

        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        [SetUp]
        public void Setup()
        {
            _storage = new MemorySaveStorage();
            _serializer = new RunXmlTestSerializer();
            _now = new DateTime(2026, 10, 5, 14, 32, 10);
        }

        RunSaveService NewService() => new RunSaveService(_storage, _serializer, () => _now);
        RunSession NewSession(int seed = 11) => new RunSession(new Random(seed), () => _now);

        static RunPlayer MakePlayer(CharacterClass c, string name)
        {
            var p = new RunPlayer(new Random(1), name);
            p.SetCharacterClass(c);
            p.BeginRun(UpgradeSettings.Default);
            p.AddCurrency(120);
            p.GainRelic("RLC_Common_001");
            p.EquipEnhanceSkill("gale_step");
            p.AdvanceToRoom(3, 1);
            p.AdvanceToRoom(6, 2);
            p.SetSelectedRoomId(9);
            return p;
        }

        static List<RoomNode> MakeRooms()
        {
            return new List<RoomNode>
            {
                new RoomNode { RoomId = 0, Row = 0, Col = 0, Type = RoomType.QUEST, ConnectedRoomIds = new List<int> { 1 } },
                new RoomNode { RoomId = 1, Row = 1, Col = 0, Type = RoomType.GUARDIAN, SpecialRoom = "Room_AcidShrine" },
            };
        }

        [Test]
        public void SingleSave_RoundTrip()
        {
            var svc = NewService();
            var run = NewSession();
            run.SetRoomName("  배고픈 슬라임 원정대  ");
            run.BeginNewRun(1);
            run.Floor = 2;
            run.ThemePath = "Theme_Slime";
            run.AddSeenGuardian("mon_acidslime");
            run.AddSeenEvent("EVT_001");

            var player = MakePlayer(CharacterClass.Fighter, "Joe");
            var req = new RunSaveRequest
            {
                MapRooms = MakeRooms(),
                MapFloor = 2,
                Players = { RunSavePlayer.FromLive("steam:1", "Joe", player.RunState, 0) },
            };
            int changed = 0;
            svc.RunSavesChanged += () => ++changed;
            Assert.That(svc.SaveCurrentRun(run, req), Is.True);
            Assert.That(changed, Is.EqualTo(1));
            Assert.That(run.SaveSlot, Is.EqualTo(RunSaveService.SingleRunSlot));

            var list = svc.GetRunSaves();
            Assert.That(list.Count, Is.EqualTo(1));
            var summary = list[0];
            Assert.That(summary.SlotName, Is.EqualTo("SingleRun"));
            Assert.That(summary.RoomName, Is.EqualTo("배고픈 슬라임 원정대"));
            Assert.That(summary.SavedAtTime, Is.EqualTo(_now));
            Assert.That(summary.bMultiplayer, Is.False);
            Assert.That(summary.Floor, Is.EqualTo(2));
            Assert.That(summary.RoomsCleared, Is.EqualTo(2));
            Assert.That(summary.PlayerNames, Is.EqualTo(new[] { "Joe" }));
            Assert.That(summary.PlayerClasses, Is.EqualTo(new[] { CharacterClass.Fighter }));
            Assert.That(svc.FindSingleRunSave().SlotName, Is.EqualTo("SingleRun"));
            Assert.That(svc.GetRunSaves(true), Is.Empty);

            var save = svc.LoadRunSave("SingleRun");
            Assert.That(save.Version, Is.EqualTo(RunSave.CurrentVersion));
            Assert.That(save.Floor, Is.EqualTo(2));
            Assert.That(save.Theme, Is.EqualTo("Theme_Slime"), "지도 테마가 비면 런 세션 것");
            Assert.That(save.RunSeed, Is.EqualTo(run.RunSeed));
            Assert.That(save.SeenGuardians, Is.EqualTo(new[] { "mon_acidslime" }));
            Assert.That(save.SeenEvents, Is.EqualTo(new[] { "EVT_001" }));
            Assert.That(save.Rooms.Count, Is.EqualTo(2));
            Assert.That(save.Rooms[0].ConnectedRoomIds, Is.EqualTo(new[] { 1 }));
            Assert.That(save.Rooms[1].SpecialRoom, Is.EqualTo("Room_AcidShrine"));

            var st = save.Players[0].RunState;
            Assert.That(save.Players[0].PlayerId, Is.EqualTo("steam:1"));
            Assert.That(st.SelectedRoomId, Is.EqualTo(-1), "고른 방은 비움");
            Assert.That(st.SavedHealth, Is.EqualTo(1), "체력 0 이면 1");
            Assert.That(st.Currency, Is.EqualTo(120));
            Assert.That(st.Relics, Is.EqualTo(new[] { "RLC_Fighter_001", "RLC_Common_001" }));
            Assert.That(st.EnhanceSkills, Is.EqualTo(new[] { "gale_step" }));
            Assert.That(st.VisitedRoomIds, Is.EqualTo(new[] { 3, 6 }));
            Assert.That(st.Stats.MaxHealth, Is.EqualTo(70));
            Assert.That(player.RunState.SelectedRoomId, Is.EqualTo(9), "살아 있는 상태는 안 건드림");
        }

        [Test]
        public void SingleSave_ReplacesOtherSingleSaves()
        {
            var svc = NewService();
            var p = MakePlayer(CharacterClass.Paladin, "Kim");

            // 멀티 세이브 하나
            var multi = NewSession();
            multi.BeginNewRun(2);
            Assert.That(svc.SaveCurrentRun(multi, new RunSaveRequest { bMultiplayer = true, Players = { RunSavePlayer.FromLive("", "Kim", p.RunState, null) } }), Is.True);

            // 예전 방식으로 남은 다른 싱글 세이브 (슬롯 이름이 SingleRun 이 아님)
            var old = new RunSave();
            old.Players.Add(RunSavePlayer.FromLive("", "Kim", p.RunState, 5));
            old.Summary.SlotName = "Run_old_single";
            old.Summary.SavedAt = _now.AddDays(-1).Ticks;
            SaveCrypto.SaveToSlot(_storage, _serializer, old, "Run_old_single");
            var index = SaveCrypto.LoadFromSlot<RunSaveIndex>(_storage, _serializer, RunSaveService.RunSaveIndexSlot, out _);
            index.Entries.Add(old.Summary);
            SaveCrypto.SaveToSlot(_storage, _serializer, index, RunSaveService.RunSaveIndexSlot);
            Assert.That(svc.GetRunSaves().Count, Is.EqualTo(2));

            var run = NewSession();
            run.BeginNewRun(1);
            _now = _now.AddMinutes(5);
            Assert.That(svc.SaveCurrentRun(run, new RunSaveRequest { Players = { RunSavePlayer.FromLive("", "Kim", p.RunState, 30) } }), Is.True);
            _now = _now.AddMinutes(5);
            Assert.That(svc.SaveCurrentRun(run, new RunSaveRequest { Players = { RunSavePlayer.FromLive("", "Kim", p.RunState, 25) } }), Is.True);

            var list = svc.GetRunSaves();
            Assert.That(list.Count, Is.EqualTo(2), "멀티 1 + 싱글 1");
            Assert.That(list[0].SlotName, Is.EqualTo("SingleRun"), "최근 것 먼저");
            Assert.That(list[1].bMultiplayer, Is.True);
            Assert.That(_storage.Exists("Run_old_single"), Is.False, "다른 싱글 세이브 파일은 지움");
            Assert.That(svc.LoadRunSave("SingleRun").Players[0].RunState.SavedHealth, Is.EqualTo(25));
        }

        [Test]
        public void MultiplayerSave_UsesRunSlot_WithDepartedPlayers()
        {
            var svc = NewService();
            var run = NewSession();
            run.BeginNewRun(3);
            Assert.That(run.SaveSlot, Is.EqualTo("Run_20261005_143210"));

            var a = MakePlayer(CharacterClass.Fighter, "A");
            var b = MakePlayer(CharacterClass.Engineer, "B");
            b.SetHardMode(true);
            var gone = MakePlayer(CharacterClass.Assassin, "C");
            gone.AdvanceToRoom(10, 3);

            var req = new RunSaveRequest
            {
                bMultiplayer = true,
                Players = { RunSavePlayer.FromLive("1", "A", a.RunState, 40), RunSavePlayer.FromLive("2", "B", b.RunState, null) },
                DepartedPlayers = { RunSavePlayer.FromDeparted("3", "C", gone.RunState) },
            };
            run.SetRooms(MakeRooms());
            Assert.That(svc.SaveCurrentRun(run, req), Is.True);

            var summary = svc.GetRunSaves(true)[0];
            Assert.That(summary.SlotName, Is.EqualTo("Run_20261005_143210"));
            Assert.That(summary.PlayerNames, Is.EqualTo(new[] { "A", "B", "C" }));
            Assert.That(summary.RoomsCleared, Is.EqualTo(3), "가장 앞선 사람 (나간 사람 포함)");
            Assert.That(summary.bHardMode, Is.True);
            Assert.That(RunSaveService.DescribeTitle(summary), Is.EqualTo("멀티 3인 · 0층 표층 · 방 3개 지남 · 하드"));
            Assert.That(RunSaveService.DescribePlayers(summary), Is.EqualTo("무도가 A, 마도 공학자 B, 암살자 C"));

            var save = svc.LoadRunSave(summary.SlotName);
            Assert.That(save.Players[1].RunState.SavedHealth, Is.EqualTo(-1), "배틀러 없음 = 최대 체력");
            Assert.That(save.Rooms.Count, Is.EqualTo(2), "지도가 없으면 런 세션 사본");
        }

        [Test]
        public void Save_FailsWithoutPlayers()
        {
            var svc = NewService();
            var run = NewSession();
            run.BeginNewRun(1);
            Assert.That(svc.SaveCurrentRun(run, new RunSaveRequest()), Is.False);
            Assert.That(svc.HasRunSaves(), Is.False);
        }

        [Test]
        public void Index_PrunesMissingFiles_AndDelete()
        {
            var svc = NewService();
            var run = NewSession();
            run.BeginNewRun(2);
            var p = MakePlayer(CharacterClass.Fighter, "A");
            svc.SaveCurrentRun(run, new RunSaveRequest { bMultiplayer = true, Players = { RunSavePlayer.FromLive("", "A", p.RunState, 10) } });
            Assert.That(svc.HasRunSaves(), Is.True);

            _storage.Delete(run.SaveSlot);   // 파일을 직접 지움
            Assert.That(svc.GetRunSaves(), Is.Empty);

            svc.SaveCurrentRun(run, new RunSaveRequest { bMultiplayer = true, Players = { RunSavePlayer.FromLive("", "A", p.RunState, 10) } });
            Assert.That(svc.TryBeginContinue(run.SaveSlot, run, out _), Is.False, "지도가 없는 세이브는 이어할 수 없음");
            Assert.That(svc.DeleteRunSave(run.SaveSlot), Is.True);
            Assert.That(_storage.Exists(run.SaveSlot), Is.False);
            Assert.That(svc.HasRunSaves(), Is.False);

            svc.DeleteSingleRunSaves();   // 없어도 괜찮음
        }

        [Test]
        public void Continue_LoadsPending_AndBeginLoadedRun()
        {
            var svc = NewService();
            var run = NewSession(1);
            run.SetRoomName("수상한 열쇠 일행");
            run.BeginNewRun(1);
            run.Floor = 3;
            run.SetRooms(MakeRooms());
            var p = MakePlayer(CharacterClass.Fighter, "Joe");
            svc.SaveCurrentRun(run, new RunSaveRequest { Players = { RunSavePlayer.FromLive("", "Joe", p.RunState, 44) } });

            Assert.That(svc.TryBeginContinue("Nope", run, out var error), Is.False);
            Assert.That(error, Is.EqualTo("세이브 파일을 읽을 수 없습니다. 손상되었거나 변조된 파일입니다."));
            Assert.That(svc.PendingLoad, Is.Null);

            var fresh = NewSession(2);
            Assert.That(svc.TryBeginContinue("SingleRun", fresh, out error), Is.True);
            Assert.That(svc.PendingLoad, Is.Not.Null);
            Assert.That(fresh.RoomName, Is.EqualTo("수상한 열쇠 일행"));

            fresh.BeginLoadedRun(svc.PendingLoad);
            Assert.That(fresh.PartySize, Is.EqualTo(1));
            Assert.That(fresh.Floor, Is.EqualTo(3));
            Assert.That(fresh.RunSeed, Is.EqualTo(run.RunSeed));
            Assert.That(fresh.SaveSlot, Is.EqualTo("SingleRun"));
            Assert.That(fresh.Rooms.Count, Is.EqualTo(2));

            // 세이브에서 이어하는 사람
            var joined = new RunPlayer(new Random(1), "Joe");
            joined.SetRunState(svc.PendingLoad.Players[0].RunState);
            joined.LockClassFromSave(svc.PendingLoad.Players[0].RunState.CharacterClass);
            Assert.That(joined.ConsumeSavedHealth(), Is.EqualTo(44));

            // 지우면 대기도 비워짐
            svc.DeleteSingleRunSaves();
            Assert.That(svc.PendingLoad, Is.Null);
            Assert.That(svc.FindSingleRunSave(), Is.Null);
        }

        [Test]
        public void TamperedRunSave_CannotContinue()
        {
            var svc = NewService();
            var run = NewSession();
            run.BeginNewRun(1);
            run.SetRooms(MakeRooms());
            var p = MakePlayer(CharacterClass.Fighter, "Joe");
            svc.SaveCurrentRun(run, new RunSaveRequest { Players = { RunSavePlayer.FromLive("", "Joe", p.RunState, 44) } });

            _storage.TryRead("SingleRun", out var data);
            data[40] ^= 0x55;
            _storage.Write("SingleRun", data);
            Assert.That(svc.LoadRunSave("SingleRun"), Is.Null);
            Assert.That(svc.TryBeginContinue("SingleRun", run, out _), Is.False);
        }

        [Test]
        public void LegacySeedZero_GetsNewSeed()
        {
            var save = new RunSave { RunSeed = 0, Floor = 0 };
            save.Players.Add(new RunSavePlayer());
            save.Summary.SlotName = "Run_X";
            var run = NewSession(4);
            run.BeginLoadedRun(save);
            Assert.That(run.RunSeed, Is.Not.EqualTo(0));
            Assert.That(run.Floor, Is.EqualTo(1), "최소 1층");
            Assert.That(run.RoomName, Is.Not.Empty, "이름이 없으면 랜덤");
        }

        [Test]
        public void RunSession_NamesAndSeeds()
        {
            var run = NewSession();
            run.SetRoomName(new string('가', 40));
            Assert.That(run.RoomName.Length, Is.EqualTo(RunSession.MaxRoomNameLength));

            var rng = new Random(0);
            for (int i = 0; i < 20; ++i)
            {
                var name = RunSession.MakeRandomRoomName(rng);
                Assert.That(name.Split(' ').Length, Is.GreaterThanOrEqualTo(3), name);
                Assert.That(name.Length, Is.LessThanOrEqualTo(RunSession.MaxRoomNameLength));
            }

            Assert.That(RunSession.MakeNewSlotName(new DateTime(2026, 1, 2, 3, 4, 5)), Is.EqualTo("Run_20260102_030405"));

            run.BeginNewRun(0);
            Assert.That(run.PartySize, Is.EqualTo(1));
            int seed = run.GetRoomSeed(1, 5);
            Assert.That(run.GetRoomSeed(1, 5), Is.EqualTo(seed), "같은 방은 같은 시드");
            Assert.That(run.GetRoomSeed(1, 6), Is.Not.EqualTo(seed));
            Assert.That(run.GetRoomSeed(2, 5), Is.Not.EqualTo(seed));

            run.AddSeenGuardian("g1");
            run.AddSeenGuardian("g1");
            run.AddSeenGuardian("g2");
            run.ForgetSeenGuardians(new[] { "g1" });
            Assert.That(run.SeenGuardians, Is.EqualTo(new[] { "g2" }));
        }

        [Test]
        public void HashCombine_MatchesUnrealReference()
        {
            // 언리얼 Templates/TypeHash.h HashCombine 을 Python 으로 따로 계산한 값
            Assert.That(RunSession.HashCombine(0u, 0u), Is.EqualTo(3175731469u));
            Assert.That(RunSession.HashCombine(1u, 2u), Is.EqualTo(1983808119u));
            Assert.That(RunSession.HashCombine(RunSession.HashCombine(12345u, 3u), 7u), Is.EqualTo(2369636980u));
        }
    }
}
