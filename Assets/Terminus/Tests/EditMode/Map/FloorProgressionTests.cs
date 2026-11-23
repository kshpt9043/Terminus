// 층 진행 / 테마 고르기 / 투표 결과 적용 / 런 끝 (언리얼 AMapManager 의 HandleBossCleared, AdvanceFloor, PickThemeForFloor, EndRun*)
using System;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;
using Terminus.Map;

namespace Terminus.Tests
{
    public class FloorProgressionTests
    {
        static readonly DungeonTheme Slime = new DungeonTheme("slime", "슬라임 왕국");
        static readonly DungeonTheme Lizard = new DungeonTheme("lizard", "리자드의 늪");
        static readonly DungeonTheme Flower = new DungeonTheme("flower", "꽃의 정원");
        static readonly DungeonTheme Deep = new DungeonTheme("deep", "심연");

        static MapSettings SettingsWithThemes()
        {
            var s = new MapSettings();
            s.TierThemes.Add(new TierThemes(Slime));
            s.TierThemes.Add(new TierThemes(Lizard, null, Flower));
            s.TierThemes.Add(new TierThemes(Deep));
            return s;
        }

        [Test]
        public void StartNewMap_PicksFirstTierTheme_AndBroadcasts()
        {
            var p = new FloorProgression(SettingsWithThemes(), new Random(1));
            List<RoomNode> broadcast = null;
            p.MapGenerated += r => broadcast = r;
            int refreshed = 0;
            p.RequestPlayerCountRefresh = () => { refreshed++; p.RefreshPlayerCount(2, 1); };

            p.StartNewMap();

            Assert.That(p.FloorTheme, Is.SameAs(Slime));
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(2));
            Assert.That(refreshed, Is.EqualTo(1));
            Assert.That(broadcast, Is.SameAs(p.Rooms));
            Assert.That(p.GetLevelCount(), Is.EqualTo(12));
        }

        [Test]
        public void RestoreMap_KeepsSavedMap_OpensSingleRows()
        {
            var p = new FloorProgression(new MapSettings(), new Random(1)) { FloorTheme = Slime };
            var saved = new List<RoomNode>
            {
                MapGenerator.CreateRoom(1, 0, 0, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(2, 0, 1, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(3, 1, 1, RoomType.BOSS, 4),
            };

            p.RestoreMap(saved, 4, Lizard);

            Assert.That(p.CurrentFloor, Is.EqualTo(4));
            Assert.That(p.FloorTheme, Is.SameAs(Lizard));
            Assert.That(p.Rooms.Select(r => r.MaxPlayers), Is.EqualTo(new[] { 4, 4, 4 }), "퀘스트 없는 지도의 첫 줄은 열림");
            Assert.That(saved[0].MaxPlayers, Is.EqualTo(1), "원본은 안 건드림 (사본)");

            // 층 0 / 테마 없음 = 지금 값 유지
            p.RestoreMap(saved, 0, null);
            Assert.That(p.CurrentFloor, Is.EqualTo(4));
            Assert.That(p.FloorTheme, Is.SameAs(Lizard));
        }

        [Test]
        public void PickThemeForFloor_SameTierKeeps_NewTierPicks()
        {
            var p = new FloorProgression(SettingsWithThemes(), new Random(3)) { FloorTheme = Slime };

            Assert.That(p.PickThemeForFloor(1), Is.SameAs(Slime));
            Assert.That(p.PickThemeForFloor(2), Is.SameAs(Slime), "같은 계층 두 번째 층은 그대로");

            var picked = new HashSet<DungeonTheme>();
            for (int i = 0; i < 40; ++i) picked.Add(p.PickThemeForFloor(3));
            Assert.That(picked, Is.EquivalentTo(new[] { Lizard, Flower }), "빈 칸(null)은 건너뛰고 후보 중 랜덤");

            p.FloorTheme = Lizard;
            Assert.That(p.PickThemeForFloor(4), Is.SameAs(Lizard));
            Assert.That(p.PickThemeForFloor(5), Is.SameAs(Deep));
            Assert.That(p.PickThemeForFloor(7), Is.SameAs(Lizard), "후보가 없는 계층이면 지금 테마");
        }

        [Test]
        public void PickThemeForFloor_NoTiers_KeepsLevelTheme()
        {
            var p = new FloorProgression(new MapSettings(), new Random(3)) { FloorTheme = Slime };
            Assert.That(p.PickThemeForFloor(1), Is.SameAs(Slime));
            Assert.That(p.PickThemeForFloor(3), Is.SameAs(Slime));
            p.FloorTheme = null;
            Assert.That(p.PickThemeForFloor(1), Is.Null);
        }

        [Test]
        public void BossCleared_NonThemeEndFloor_AdvancesImmediately()
        {
            var p = new FloorProgression(SettingsWithThemes(), new Random(2));
            p.StartNewMap();
            var events = new List<string>();
            FloorArrival arrival = null;
            p.FloorAdvanced += a => { arrival = a; events.Add("floor"); };
            p.MapGenerated += r => events.Add("map");

            bool advanced = p.HandleBossCleared(false, 3, 10);

            Assert.That(advanced, Is.True);
            Assert.That(p.CurrentFloor, Is.EqualTo(2));
            Assert.That(p.IsFloorVoteActive, Is.False);
            Assert.That(events, Is.EqualTo(new[] { "floor", "map" }), "층 도착(플레이어 / 퀘스트 / 특수 방) 다음에 지도 알림");
            Assert.That(arrival.Floor, Is.EqualTo(2));
            Assert.That(arrival.Title, Is.EqualTo("2층"));
            Assert.That(arrival.Subtitle, Is.EqualTo("표층 · 슬라임 왕국"));
            Assert.That(arrival.ChatText, Is.EqualTo("2층(표층 · 슬라임 왕국)에 도착했습니다. 체력이 모두 회복되었습니다."));
            Assert.That(arrival.bNewTheme, Is.False);
            Assert.That(p.GetLevelCount(), Is.EqualTo(11), "테마 끝 층은 퀘스트 줄 없음");
            Assert.That(p.Rooms.Any(r => r.Type == RoomType.QUEST), Is.False);
        }

        [Test]
        public void BossCleared_ThemeEndFloor_VoteNextFloor_AdvancesToNewTier()
        {
            var p = new FloorProgression(SettingsWithThemes(), new Random(5)) { CurrentFloor = 2, FloorTheme = Slime };
            p.StartNewMap();
            var notices = new List<string>();
            p.SystemNotice += n => notices.Add(n);
            FloorArrival arrival = null;
            p.FloorAdvanced += a => arrival = a;
            var voteStates = new List<FloorVoteState>();
            p.VoteChanged += s => voteStates.Add(s);

            bool advanced = p.HandleBossCleared(false, 2, 0);

            Assert.That(advanced, Is.False, "투표 결과가 나올 때까지 저장 안 함");
            Assert.That(p.IsFloorVoteActive);
            Assert.That(notices.Single(), Does.Contain("2층 보스를 쓰러뜨렸습니다"));
            Assert.That(voteStates.Last().Options, Does.Contain(FloorChoice.Betray));

            Assert.That(p.CastFloorVote(1, FloorChoice.NextFloor, 1), Is.True);
            Assert.That(p.CastFloorVote(2, FloorChoice.NextFloor, 1), Is.True);
            p.Tick(1 + p.Settings.RevealSeconds);
            Assert.That(p.CurrentFloor, Is.EqualTo(2), "결과 보여 주는 중엔 아직");
            p.Tick(1 + p.Settings.RevealSeconds + p.Settings.ResultSeconds);

            Assert.That(p.CurrentFloor, Is.EqualTo(3));
            Assert.That(p.IsFloorVoteActive, Is.False);
            Assert.That(new[] { Lizard, Flower }, Does.Contain(p.FloorTheme));
            Assert.That(arrival.bNewTheme, Is.True, "3층 = 새 테마 (퀘스트 만료)");
            Assert.That(arrival.Subtitle, Does.StartWith("중층 · "));
            Assert.That(p.GetLevelCount(), Is.EqualTo(12));
        }

        [Test]
        public void Vote_Escape_EndsRun()
        {
            var p = new FloorProgression(new MapSettings(), new Random(5)) { CurrentFloor = 2 };
            RunEndInfo end = null;
            p.RunEnded += e => end = e;

            p.HandleBossCleared(true, 1, 0);
            p.CastFloorVote(7, FloorChoice.Escape, 0);
            p.Tick(p.Settings.SingleResultSeconds);

            Assert.That(p.IsRunEnded);
            Assert.That(end.Reason, Is.EqualTo("던전을 탈출했습니다."));
            Assert.That(end.bDeath, Is.False);
            Assert.That(end.MessageFor(7), Is.EqualTo("던전을 탈출했습니다."));
            Assert.That(end.AdjustFor(7).BonusGold, Is.EqualTo(0));
        }

        [Test]
        public void Vote_LastFloor_OnlyEscapeOrBetray()
        {
            var p = new FloorProgression(new MapSettings(), new Random(5)) { CurrentFloor = 6 };
            p.HandleBossCleared(false, 2, 0);
            Assert.That(p.Vote.State.Options, Is.EqualTo(new[] { FloorChoice.Escape, FloorChoice.Betray }));
        }

        [Test]
        public void Vote_Betray_RequestsBattle()
        {
            var p = new FloorProgression(new MapSettings(), new Random(5)) { CurrentFloor = 4 };
            ulong? betrayer = null;
            bool requested = false;
            p.BetrayalRequested += b => { requested = true; betrayer = b; };

            p.HandleBossCleared(false, 3, 0);
            p.CastFloorVote(1, FloorChoice.Escape, 0);
            p.CastFloorVote(3, FloorChoice.Betray, 0);
            p.Tick(5);
            p.Tick(8);

            Assert.That(requested);
            Assert.That(betrayer, Is.EqualTo(3UL));
            Assert.That(p.IsRunEnded, Is.False, "전투 결과를 기다림");
            Assert.That(p.CurrentFloor, Is.EqualTo(4));
        }

        [Test]
        public void EndRunByDeath_And_BetrayalResultIgnoredAfterEnd()
        {
            var p = new FloorProgression(new MapSettings(), new Random(5));
            var ends = new List<RunEndInfo>();
            p.RunEnded += e => ends.Add(e);

            p.EndRunByDeath("전멸했습니다.");
            Assert.That(ends.Single().bDeath);
            Assert.That(p.IsRunEnded);

            p.HandleBetrayalResult(1, true, new List<BetrayalParticipant>(), _ => null);
            Assert.That(ends, Has.Count.EqualTo(1), "런이 끝난 뒤엔 무시");
        }

        [Test]
        public void RefreshPlayerCount_MaxOfPartyAndConnected_Clamped()
        {
            var p = new FloorProgression(new MapSettings { CurrentPlayerCount = 3 }, new Random(1));
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(3));
            p.RefreshPlayerCount(0, 0);
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(3), "둘 다 모르면 그대로");
            p.RefreshPlayerCount(2, 1);
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(2));
            p.RefreshPlayerCount(1, 3);
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(3));
            p.RefreshPlayerCount(9, 0);
            Assert.That(p.CurrentPlayerCount, Is.EqualTo(FloorRules.MaxPartySize));
        }

        [Test]
        public void FullRun_Floors1To6_AllMapsValid()
        {
            var settings = SettingsWithThemes();
            var p = new FloorProgression(settings, new Random(11));
            p.RequestPlayerCountRefresh = () => p.RefreshPlayerCount(4, 4);
            p.StartNewMap();
            double now = 0;

            while (true)
            {
                var result = new MapGenResult();
                Assert.That(MapGenerator.ValidatePathToBoss(p.Rooms, p.GetLevelCount() - 1), $"floor {p.CurrentFloor}");
                Assert.That(MapGenerator.ValidatePlayerCapacity(p.Rooms, 4), $"floor {p.CurrentFloor}");
                Assert.That(p.GetLevelCount(), Is.EqualTo(FloorRules.HasQuestRoom(p.CurrentFloor) ? 12 : 11));

                if (p.CurrentFloor == 6) break;
                if (!p.HandleBossCleared(false, 4, now))
                {
                    for (ulong id = 1; id <= 4; ++id) p.CastFloorVote(id, FloorChoice.NextFloor, now);
                    now += settings.RevealSeconds; p.Tick(now);
                    now += settings.ResultSeconds; p.Tick(now);
                }
            }

            Assert.That(p.FloorTheme, Is.SameAs(Deep));
            Assert.That(p.IsRunEnded, Is.False);
        }
    }
}
