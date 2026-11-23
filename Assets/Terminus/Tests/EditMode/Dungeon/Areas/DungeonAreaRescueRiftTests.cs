// 전멸 -> 구출 / 난입 투표 / 런 끝, 이공간 (언리얼 UDungeonAreaSubsystem 사망 순서도 + ADungeonGameMode 나간 사람)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;
using static Terminus.Tests.AreaTestKit;

namespace Terminus.Tests
{
    public class DungeonAreaRescueTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        // A 는 10 번 방(전멸), B / C 는 11 번 방(클리어)
        static Rig WipeOneArea(out DungeonPlayer a, out DungeonPlayer b, out DungeonPlayer c)
        {
            var rig = MakeRig();
            a = MakePlayer(1, "A");
            b = MakePlayer(2, "B");
            c = MakePlayer(3, "C");
            Enter(rig, new[] { Room(10, RoomType.MONSTER, row: 4), Room(11, RoomType.MONSTER, row: 4) }, (a, 10), (b, 11), (c, 11));

            rig.Combats[1].Clear();
            a.Battler.ApplyDamage(9999);
            rig.Combats[0].Wipe();
            return rig;
        }

        [Test]
        public void Wipe_WaitsForOtherAreas_ThenStartsRescueVote()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Enter(rig, new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.MONSTER) }, (a, 10), (b, 11));

            rig.Combats[0].Wipe();
            Assert.That(rig.Areas[0].IsWiped, Is.True);
            Assert.That(rig.Manager.IsRescueVoting, Is.False, "다른 구역이 아직 싸우는 중");
            Assert.That(rig.Areas[0].IsFightOver(), Is.False);

            rig.Combats[1].Clear();
            Assert.That(rig.Manager.IsRescueVoting, Is.True);
            Assert.That(rig.Manager.RescueVoters, Is.EqualTo(new[] { b }));
            Assert.That(rig.Notifier.RescueChooser[b], Is.True);
            Assert.That(rig.Notifier.RescueChooser[a], Is.False);
            Assert.That(rig.Notifier.LastRescueCanIntervene, Is.True);
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("A 님이 쓰러졌습니다"));
        }

        [Test]
        public void Rescue_SurvivorsPayHealth_WipedRevivedAtOne_EveryoneMovesToWipedRoom()
        {
            var rig = WipeOneArea(out var a, out var b, out var c);
            b.Battler.ApplyDamage(b.Battler.State.Health - 50);   // 체력 50
            int cHealth = c.Battler.State.Health;

            rig.Manager.HandleRescueChoice(b, false);
            Assert.That(rig.Manager.IsRescueVoting, Is.True, "C 가 아직");
            rig.Manager.HandleRescueChoice(a, false);   // 쓰러진 사람은 투표 못 함
            rig.Manager.HandleRescueChoice(c, false);

            Assert.That(rig.Manager.IsRescueVoting, Is.False);
            Assert.That(b.Battler.State.Health, Is.EqualTo(30), "현재 체력의 40% 를 잃음");
            Assert.That(c.Battler.State.Health, Is.EqualTo(cHealth - CombatMath.RoundToInt(cHealth * 0.4f)));
            Assert.That(a.Battler.State.Health, Is.EqualTo(1));
            foreach (var p in new[] { a, b, c })
            {
                Assert.That(p.RunState.CurrentRoomId, Is.EqualTo(10), "살리러 간 방에 같이 있는 걸로 침");
            }
            Assert.That(rig.Areas[0].IsInUse || rig.Areas[1].IsInUse, Is.False);
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(rig.Notifier.Count("CloseRescue"), Is.EqualTo(3));
            Assert.That(rig.Notifier.Chat, Has.Some.StartsWith("구출!"));
        }

        [Test]
        public void Intervention_MajorityJoinsWipedArea_AndResumesFight()
        {
            var rig = WipeOneArea(out var a, out var b, out var c);
            rig.Manager.HandleRescueChoice(b, true);
            rig.Manager.HandleRescueChoice(c, true);

            var wipedArea = rig.Areas[0];
            Assert.That(rig.Areas[1].IsInUse, Is.False);
            Assert.That(wipedArea.IsWiped, Is.False);
            Assert.That(wipedArea.Occupants, Is.EqualTo(new[] { a, b, c }));
            wipedArea.TryGetSlot(c, out var slot);
            Assert.That(slot.Index, Is.EqualTo(2));
            Assert.That(rig.Manager.GetCurrentArea(b), Is.SameAs(wipedArea));
            Assert.That(rig.Combats[0].Joiners, Is.EqualTo(new[] { b, c }));
            Assert.That(rig.Notifier.Chat, Has.Some.StartsWith("난입!"));

            // 이기면 전원 그 방으로
            rig.Combats[0].Clear();
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(b.RunState.CurrentRoomId, Is.EqualTo(10));
        }

        [Test]
        public void Intervention_LostAgain_EndsRun()
        {
            var rig = WipeOneArea(out var a, out var b, out var c);
            rig.Manager.HandleRescueChoice(b, true);
            rig.Manager.HandleRescueChoice(c, true);

            rig.Combats[0].Wipe();
            Assert.That(rig.WipeReason, Is.EqualTo(DungeonAreaManager.MsgPartyWiped));
        }

        [Test]
        public void RescueVote_Timeout_NoVotes_MeansRescue()
        {
            var rig = WipeOneArea(out var a, out var b, out var c);
            rig.Timers.Advance(19.9);
            Assert.That(rig.Manager.IsRescueVoting, Is.True);
            rig.Timers.Advance(0.2);

            Assert.That(rig.Manager.IsRescueVoting, Is.False);
            Assert.That(a.Battler.State.Health, Is.EqualTo(1));
            Assert.That(b.RunState.CurrentRoomId, Is.EqualTo(10));
        }

        [Test]
        public void TwoWipedAreas_RescueOnly()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var c = MakePlayer(3, "C");
            Enter(rig, new[] { Room(10, RoomType.MONSTER), Room(11, RoomType.MONSTER), Room(12, RoomType.BREAK) }, (a, 10), (b, 11), (c, 12));

            rig.Combats[0].Wipe();
            rig.Combats[1].Wipe();
            rig.Manager.HandleRestExplore(c);
            rig.Timers.Advance(1.5);

            Assert.That(rig.Manager.IsRescueVoting, Is.True);
            Assert.That(rig.Manager.RescueCanIntervene, Is.False, "전멸한 방이 둘 이상이면 구출만");

            rig.Manager.HandleRescueChoice(c, true);   // 난입은 거절
            Assert.That(rig.Manager.IsRescueVoting, Is.True);
            rig.Manager.HandleRescueChoice(c, false);
            Assert.That(rig.Manager.IsRescueVoting, Is.False);
            Assert.That(c.RunState.CurrentRoomId, Is.EqualTo(10), "첫 번째 전멸한 방");
            Assert.That(b.RunState.CurrentRoomId, Is.EqualTo(10));
        }

        [Test]
        public void EveryoneWiped_EndsRun_SoloAndParty()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            Enter(rig, new[] { Room(10, RoomType.MONSTER) }, (a, 10));
            rig.Combats[0].Wipe();
            Assert.That(rig.WipeReason, Is.EqualTo(DungeonAreaManager.MsgSoloDied));

            var rig2 = MakeRig();
            var b = MakePlayer(2, "B");
            var c = MakePlayer(3, "C");
            Enter(rig2, new[] { Room(10, RoomType.MONSTER) }, (b, 10), (c, 10));
            rig2.Combats[0].Wipe();
            Assert.That(rig2.WipeReason, Is.EqualTo(DungeonAreaManager.MsgPartyWiped));
            Assert.That(rig2.Manager.IsRescueVoting, Is.False);
        }

        [Test]
        public void Leaver_DuringRescueVote_ClosesVote_AndRollsBack()
        {
            var rig = WipeOneArea(out var a, out var b, out var c);
            Assert.That(rig.Manager.IsRescueVoting, Is.True);

            rig.Manager.HandlePlayerLeft(c);

            Assert.That(rig.Manager.IsRescueVoting, Is.False);
            Assert.That(rig.Notifier.Count("CloseRescue"), Is.EqualTo(3));
            Assert.That(a.Battler.State.Health, Is.EqualTo(a.Battler.Stats.MaxHealth), "들어가기 전으로");
            Assert.That(rig.Areas[0].IsInUse || rig.Areas[1].IsInUse, Is.False);
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(-1));
            rig.Timers.Advance(30);
            Assert.That(rig.RoomsFinishedCount, Is.Zero, "닫힌 투표 타이머는 안 울림");
        }
    }

    public class DungeonRiftTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        [Test]
        public void GuestLeaves_RollbackThenWaitInRift_ReturnRestoresState()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var connected = new List<DungeonPlayer> { a, b };
            var rift = new DungeonRift(rig.Manager, rig.Notifier, () => connected);
            bool? listing = null;
            rift.RejoinListingChanged += open => listing = open;

            b.Run.AddCurrency(40);
            Enter(rig, new[] { Room(10, RoomType.MONSTER) }, (a, 10), (b, 10));
            b.Run.AddCurrency(100);       // 방 안에서 얻음 -> 롤백
            b.Battler.ApplyDamage(10);    // 들어가기 전 체력으로 돌아감

            connected.Remove(b);
            rift.HandlePlayerLeft(b, "steam-b", bGuestLeft: true, bRunEnded: false);

            Assert.That(rig.Areas[0].IsInUse, Is.False);
            Assert.That(rift.HasDepartedPlayers, Is.True);
            Assert.That(rift.GetDepartedNames(), Is.EqualTo(new[] { "B" }));
            var departed = rift.DepartedPlayers[0];
            Assert.That(departed.PlayerId, Is.EqualTo("steam-b"));
            Assert.That(departed.RunState.Currency, Is.EqualTo(40));
            Assert.That(departed.RunState.SavedHealth, Is.EqualTo(b.Battler.Stats.MaxHealth));
            Assert.That(departed.RunState.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(listing, Is.True);
            Assert.That(rig.Notifier.Count("EnterRift", a), Is.EqualTo(1));
            Assert.That(rig.Notifier.Count("EnterRift", b), Is.Zero, "나간 사람에겐 안 보냄");
            Assert.That(rig.Notifier.RiftWaiting[a], Is.EqualTo(new[] { "B" }));
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("B 님이 나갔습니다. 돌아올 때까지"));

            // 들어올 수 있나 (PreLogin)
            Assert.That(rift.CanJoin("steam-x", out var error), Is.False);
            Assert.That(error, Is.EqualTo(DungeonRift.MsgAlreadyInProgress));
            Assert.That(rift.CanJoin("steam-b", out _), Is.True);

            // 돌아옴
            var restored = rift.ClaimDeparted("steam-b", "B", bLocal: false);
            Assert.That(restored, Is.Not.Null);
            Assert.That(restored.Currency, Is.EqualTo(40));
            Assert.That(rift.HasDepartedPlayers, Is.False);

            var b2 = MakePlayer(5, "B");
            b2.Run.SetRunState(restored);
            connected.Add(b2);
            rift.AnnounceReturn("B");
            Assert.That(listing, Is.False);
            Assert.That(rig.Notifier.Count("LeaveRift", a), Is.EqualTo(1));
            Assert.That(rig.Notifier.Count("LeaveRift", b2), Is.EqualTo(1));
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("B 님이 돌아왔습니다. 이공간을 빠져나와 지도로 갑니다."));
        }

        [Test]
        public void HostOrRunEnded_NoRift()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var rift = new DungeonRift(rig.Manager, rig.Notifier, () => new[] { a });

            rift.HandlePlayerLeft(b, "id", bGuestLeft: true, bRunEnded: true);
            Assert.That(rift.HasDepartedPlayers, Is.False, "런이 끝났으면 기다릴 것 없음");

            rift.HandlePlayerLeft(b, "id", bGuestLeft: false, bRunEnded: false);
            Assert.That(rift.HasDepartedPlayers, Is.False, "방장이 나가면 세션이 끝남");
            Assert.That(rig.Notifier.Chat[rig.Notifier.Chat.Count - 1], Is.EqualTo("B 님이 나갔습니다."));
        }

        [Test]
        public void Betrayal_LeaverGoesToCombat_NotRift()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            var rift = new DungeonRift(rig.Manager, rig.Notifier, () => new[] { a });
            rig.Manager.StartBetrayal(new[] { a, b }, b, Slime, 0.3f, 1);

            rift.HandlePlayerLeft(b, "id", bGuestLeft: true, bRunEnded: false);
            Assert.That(rig.Combats[0].BetrayalLeaver, Is.SameAs(b));
            Assert.That(rift.HasDepartedPlayers, Is.False);
        }

        [Test]
        public void MultiplayerSave_GathersEveryone_LooseMatchForHost()
        {
            var rig = MakeRig();
            var connected = new List<DungeonPlayer>();
            var rift = new DungeonRift(rig.Manager, rig.Notifier, () => connected);

            var save = new RunSave();
            save.Summary.bMultiplayer = true;
            save.Players.Add(RunSavePlayer.FromLive("steam-host", "Host", new RunState { Currency = 11, SelectedRoomId = 4 }, 30));
            save.Players.Add(RunSavePlayer.FromLive("steam-guest", "Guest", new RunState { Currency = 22 }, null));
            rift.BeginFromSave(save);

            Assert.That(rift.IsGatheringFromSave, Is.True);
            Assert.That(rift.GetDepartedNames(), Is.EqualTo(new[] { "Host", "Guest" }));

            // 방장은 아이디가 달라도 자리를 받음 (계정을 바꿨거나 PIE)
            Assert.That(rift.ClaimDeparted("other-id", "Somebody", bLocal: false), Is.Null);
            var host = rift.ClaimDeparted("other-id", "Host", bLocal: true);
            Assert.That(host.Currency, Is.EqualTo(11));
            Assert.That(host.SelectedRoomId, Is.EqualTo(-1));
            Assert.That(host.SavedHealth, Is.EqualTo(30));

            var hp = MakePlayer(1, "Host");
            connected.Add(hp);
            rift.AnnounceReturn("Host");
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("Host 님이 합류했습니다. 아직 기다리는 중: Guest"));
            Assert.That(rig.Notifier.Count("EnterRift", hp), Is.EqualTo(1));

            var guest = rift.ClaimDeparted("steam-guest", "Guest", bLocal: false);
            Assert.That(guest.Currency, Is.EqualTo(22));
            rift.AnnounceReturn("Guest");
            Assert.That(rift.IsGatheringFromSave, Is.False);

            // 싱글 세이브는 아무것도 안 함
            var rift2 = new DungeonRift(rig.Manager, rig.Notifier, () => connected);
            var single = new RunSave();
            single.Players.Add(new RunSavePlayer { PlayerName = "Solo" });
            rift2.BeginFromSave(single);
            Assert.That(rift2.HasDepartedPlayers, Is.False);
        }
    }
}
