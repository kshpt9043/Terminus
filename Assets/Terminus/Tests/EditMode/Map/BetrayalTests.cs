// 배신 결과 정산 조정 (언리얼 AMapManager::HandleBetrayalResult) + 전투 순서 / 퀘스트 특수 방 넣기
using System;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Tests
{
    public class BetrayalTests
    {
        static RelicRow Relic(string row, string name, int sellGold) =>
            new RelicRow { RowName = row, RelicName = name, SellPrice_Gold = sellGold };

        static readonly Dictionary<string, RelicRow> Relics = new Dictionary<string, RelicRow>
        {
            { "RLC_BOSS_A", Relic("RLC_BOSS_A", "왕관", 300) },
            { "RLC_BOSS_B", Relic("RLC_BOSS_B", "홀", 100) },
            { "RLC_NOSELL", Relic("RLC_NOSELL", "저주", -1) },
        };

        static RelicRow Find(string row) => Relics.TryGetValue(row, out var r) ? r : null;

        static BetrayalParticipant P(ulong id, string name, string bossRelic, string bossSkill = "") =>
            new BetrayalParticipant(id, name, new RunState { BossRelic = bossRelic, BossSkill = bossSkill });

        [Test]
        public void BetrayerWon_TakesTeamBossRelics_TeamLosesBossRewards()
        {
            var players = new List<BetrayalParticipant>
            {
                P(1, "A", "RLC_BOSS_A", "SK_1"),
                P(2, "B", "RLC_BOSS_A"),          // 같은 유물은 한 번만
                P(3, "C", ""),
                P(9, "배신왕", "RLC_BOSS_B", "SK_9"),
            };

            var r = BetrayalRules.ComputeBetrayalResult(9, true, players, Find);

            Assert.That(r.Keys, Is.EquivalentTo(new ulong[] { 1, 2, 3, 9 }));
            Assert.That(r[9].Message, Is.EqualTo("배신에 성공했습니다! 동료들의 보스 유물을 빼앗았습니다."));
            Assert.That(r[9].Adjust.ExtraRelics, Is.EqualTo(new[] { "RLC_BOSS_A" }));
            Assert.That(r[9].Adjust.LostRelics, Is.Empty, "이긴 배신자는 자기 보스 유물을 지킴");

            Assert.That(r[1].Message, Is.EqualTo("배신자 배신왕 에게 패배했습니다. 보스전에서 얻은 유물과 스킬을 잃었습니다."));
            Assert.That(r[1].Adjust.LostRelics, Is.EqualTo(new[] { "RLC_BOSS_A" }));
            Assert.That(r[1].Adjust.LostSkills, Is.EqualTo(new[] { "SK_1" }));
            Assert.That(r[3].Adjust.LostRelics, Is.Empty);
            Assert.That(r.Values.All(x => x.Adjust.BonusGold == 0));
        }

        [Test]
        public void BetrayerLost_LosesBossRewards_TeamSplitsSalePrice()
        {
            var players = new List<BetrayalParticipant>
            {
                P(9, "배신왕", "RLC_BOSS_A", "SK_9"),
                P(1, "A", "RLC_BOSS_B"),
                P(2, "B", ""),
            };

            var r = BetrayalRules.ComputeBetrayalResult(9, false, players, Find);

            Assert.That(r[9].Message, Is.EqualTo("배신에 실패했습니다. 보스전에서 얻은 유물과 스킬을 잃었습니다."));
            Assert.That(r[9].Adjust.LostRelics, Is.EqualTo(new[] { "RLC_BOSS_A" }));
            Assert.That(r[9].Adjust.LostSkills, Is.EqualTo(new[] { "SK_9" }));

            foreach (ulong id in new ulong[] { 1, 2 })
            {
                Assert.That(r[id].Message, Is.EqualTo("배신자 배신왕 을(를) 물리쳤습니다!"));
                Assert.That(r[id].Adjust.BonusGold, Is.EqualTo(150), "300 골드를 2명이 나눔");
                Assert.That(r[id].Adjust.BonusReason, Is.EqualTo("배신자의 보스 유물 [왕관] 판매 분배"));
                Assert.That(r[id].Adjust.LostRelics, Is.Empty, "이긴 쪽은 평소 정산");
            }
        }

        [Test]
        public void BetrayerLost_UnsellableOrNoRelic_NoBonus()
        {
            var players = new List<BetrayalParticipant> { P(9, "X", "RLC_NOSELL"), P(1, "A", "") };
            var r = BetrayalRules.ComputeBetrayalResult(9, false, players, Find);
            Assert.That(r[1].Adjust.BonusGold, Is.EqualTo(0));
            Assert.That(r[1].Adjust.BonusReason, Is.Empty);

            players = new List<BetrayalParticipant> { P(9, "X", ""), P(1, "A", "") };
            r = BetrayalRules.ComputeBetrayalResult(9, false, players, Find);
            Assert.That(r[1].Adjust.BonusGold, Is.EqualTo(0));
            Assert.That(r[9].Adjust.LostRelics, Is.Empty);
        }

        [Test]
        public void BetrayerLost_ShareRoundsDown()
        {
            var players = new List<BetrayalParticipant> { P(9, "X", "RLC_BOSS_B"), P(1, "A", ""), P(2, "B", ""), P(3, "C", "") };
            var r = BetrayalRules.ComputeBetrayalResult(9, false, players, Find);
            Assert.That(r[1].Adjust.BonusGold, Is.EqualTo(33));
        }

        [Test]
        public void UnknownBetrayer_TeamStillSettles()
        {
            var players = new List<BetrayalParticipant> { P(1, "A", "RLC_BOSS_A"), P(2, "B", "") };
            var lost = BetrayalRules.ComputeBetrayalResult(null, false, players, Find);
            Assert.That(lost.Keys, Is.EquivalentTo(new ulong[] { 1, 2 }));
            Assert.That(lost[1].Message, Is.EqualTo("배신자 배신자 을(를) 물리쳤습니다!"));

            var won = BetrayalRules.ComputeBetrayalResult(null, true, players, Find);
            Assert.That(won.Keys, Is.EquivalentTo(new ulong[] { 1, 2 }), "배신자 몫은 아무에게도 안 감");
        }

        [Test]
        public void HandleBetrayalResult_EndsRunWithPerPlayerResults()
        {
            var p = new FloorProgression(new MapSettings(), new Random(1));
            RunEndInfo end = null;
            p.RunEnded += e => end = e;

            p.HandleBetrayalResult(9, false, new List<BetrayalParticipant> { P(9, "X", "RLC_BOSS_A"), P(1, "A", "") }, Find);

            Assert.That(p.IsRunEnded);
            Assert.That(end.Reason, Is.EqualTo("배신 전투가 끝나 던전을 나갑니다."));
            Assert.That(end.bDeath, Is.False);
            Assert.That(end.MessageFor(1), Is.EqualTo("배신자 X 을(를) 물리쳤습니다!"));
            Assert.That(end.AdjustFor(1).BonusGold, Is.EqualTo(300));
            Assert.That(end.MessageFor(55), Is.EqualTo(end.Reason), "결과가 없는 사람은 공통 안내");
            Assert.That(end.AdjustFor(55).BonusGold, Is.EqualTo(0));
        }

        [Test]
        public void HandleBetrayalResult_DefaultRelicLookupUsesGameData()
        {
            TestData.LoadGameData();
            var sellable = GameData.Relics.Rows.FirstOrDefault(r => r.CanSellAtSettlement() && r.SellPrice_Gold > 0);
            Assume.That(sellable, Is.Not.Null, "정산 판매 가능한 유물이 데이터에 없음");

            var r = BetrayalRules.ComputeBetrayalResult(9, false,
                new List<BetrayalParticipant> { P(9, "X", sellable.RowName), P(1, "A", "") });
            Assert.That(r[1].Adjust.BonusGold, Is.EqualTo(sellable.SellPrice_Gold));
            Assert.That(r[1].Adjust.BonusReason, Does.Contain(sellable.RelicName));
        }

        [Test]
        public void BattleOrder_TeamFirstBetrayerLast()
        {
            var order = BetrayalRules.BuildBattleOrder(new ulong[] { 5, 9, 2 }, 9);
            Assert.That(order, Is.EqualTo(new ulong[] { 5, 2, 9 }));
            Assert.That(BetrayalRules.CanStartBattle(9, order), Is.True);

            var alone = BetrayalRules.BuildBattleOrder(new ulong[] { 9 }, 9);
            Assert.That(BetrayalRules.CanStartBattle(9, alone), Is.False, "상대가 없음");
            Assert.That(BetrayalRules.CanStartBattle(null, BetrayalRules.BuildBattleOrder(new ulong[] { 1, 2 }, null)), Is.False);
            Assert.That(BetrayalRules.StartNotice("X"), Is.EqualTo("X 님이 배신했습니다! 배신 전투가 시작됩니다."));
        }

        // ---------------------------------------------------------------
        // 퀘스트 특수 방 (지도 쪽)
        // ---------------------------------------------------------------

        [Test]
        public void SpecialRoom_RowForDepth()
        {
            // 퀘스트 층 (1): 12칸 기준 Depth - 1. 퀘스트 줄이 없는 층 (2): 한 칸 당겨져 Depth - 2
            Assert.That(SpecialRoomPlacement.RowForDepth(5, 1, 11), Is.EqualTo(4));
            Assert.That(SpecialRoomPlacement.RowForDepth(5, 2, 10), Is.EqualTo(3));
            Assert.That(SpecialRoomPlacement.RowForDepth(1, 1, 11), Is.EqualTo(1), "퀘스트 줄에는 안 넣음");
            Assert.That(SpecialRoomPlacement.RowForDepth(1, 2, 10), Is.EqualTo(0));
            Assert.That(SpecialRoomPlacement.RowForDepth(12, 2, 10), Is.EqualTo(10), "12 이상 = 보스");
            Assert.That(SpecialRoomPlacement.RowForDepth(11, 2, 10), Is.EqualTo(9));
        }

        [Test]
        public void SpecialRoom_TryPlace_MakesGuardianOnce()
        {
            var rooms = new MapGenerator(new MapSettings(), new Random(4)).GenerateMap(1, 2);
            var placed = SpecialRoomPlacement.TryPlace(rooms, 1, "QR_SLIME_1", 5, new Random(1));

            Assert.That(placed, Is.Not.Null);
            Assert.That(placed.Row, Is.EqualTo(4));
            Assert.That(placed.Type, Is.EqualTo(RoomType.GUARDIAN));
            Assert.That(placed.SpecialRoom, Is.EqualTo("QR_SLIME_1"));
            Assert.That(SpecialRoomPlacement.ContainsSpecialRoom(rooms, "QR_SLIME_1"));
            Assert.That(SpecialRoomPlacement.TryPlace(rooms, 1, "QR_SLIME_1", 5, new Random(1)), Is.Null, "이미 있으면 안 넣음");
            Assert.That(SpecialRoomPlacement.TryPlace(rooms, 1, "", 5, new Random(1)), Is.Null);

            var boss = SpecialRoomPlacement.TryPlace(rooms, 1, "QR_BOSS", 12, new Random(1));
            Assert.That(boss.Type, Is.EqualTo(RoomType.BOSS), "보스방은 보스 그대로");
        }
    }
}
