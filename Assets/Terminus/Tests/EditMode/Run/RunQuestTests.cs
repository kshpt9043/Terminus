// 퀘스트: 후보 고르기 / 진행 / 완료 보상 / 특수 방 (실제 DT_Quest / DT_QuestRoom)
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunQuestTests
    {
        const string SlimeTheme = "슬라임 왕국";

        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static RunPlayer NewPlayer(CharacterClass c = CharacterClass.Fighter)
        {
            var p = new RunPlayer(new Random(5), "Tester");
            p.SetCharacterClass(c);
            p.BeginRun(UpgradeSettings.Default);
            return p;
        }

        [Test]
        public void Data_QuestTablesLoaded()
        {
            Assert.That(GameData.Quests.Count, Is.GreaterThan(90));
            Assert.That(GameData.QuestRooms.Count, Is.GreaterThan(5));
            Assert.That(GameData.Quests.Warnings, Is.Empty);
        }

        [Test]
        public void MakeOffers_ThreeFromThemeOrTier_OneOwnFaction()
        {
            for (int seed = 0; seed < 50; ++seed)
            {
                var p = NewPlayer(CharacterClass.Fighter);   // 모험가 길드
                var offers = Quests.MakeOffers(p, 1, SlimeTheme, new Random(seed));
                Assert.That(offers.Count, Is.EqualTo(3));
                Assert.That(offers, Is.Unique);

                int own = 0;
                foreach (var row in offers)
                {
                    var q = GameData.FindQuestRow(row);
                    Assert.That(q.Theme == SlimeTheme || q.Theme == "표층" || q.Theme == "Any", Is.True, row);
                    if (q.Force == "모험가 길드") ++own;
                }
                Assert.That(own, Is.GreaterThanOrEqualTo(1), $"seed {seed}");
            }
        }

        [Test]
        public void MakeOffers_SameSeedSameOffers_AndTierOnFloor3()
        {
            var p = NewPlayer(CharacterClass.Paladin);   // 테르미누스
            var a = Quests.MakeOffers(p, 3, "리자드의 늪", new Random(42));
            var b = Quests.MakeOffers(p, 3, "리자드의 늪", new Random(42));
            Assert.That(a, Is.EqualTo(b));
            foreach (var row in a)
            {
                var q = GameData.FindQuestRow(row);
                Assert.That(q.Theme == "리자드의 늪" || q.Theme == "중층", Is.True, row);
            }

            // 모르는 테마 + 계층 이름으로만
            var tierOnly = Quests.MakeOffers(p, 5, "없는 테마", new Random(1), 30);
            foreach (var row in tierOnly) Assert.That(GameData.FindQuestRow(row).Theme, Is.EqualTo("심층"));
            Assert.That(tierOnly.Count, Is.EqualTo(10), "심층 퀘스트 10개가 전부");
        }

        [Test]
        public void KillBoss_CompletesAndGivesRelic()
        {
            var p = NewPlayer();
            var notices = new List<string>();
            p.Notice += notices.Add;

            Quests.Accept(p, "Q_SLI_001", 1);   // 슬라임 킹 처치 -> RLC_Surface_Q008
            Assert.That(p.RunState.ActiveQuest.Quest, Is.EqualTo("Q_SLI_001"));
            Assert.That(Quests.DescribeActive(p.RunState), Is.EqualTo("[퀘스트] 슬라임 학살자 0 / 1"));

            Quests.OnMonstersKilled(p, new[] { "mon_acidslime" });   // 다른 몬스터
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(0));

            Quests.OnMonstersKilled(p, new[] { "mon_slimeking" });
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.RunState.Relics, Does.Contain("RLC_Surface_Q008"));
            Assert.That(p.HasSeenRelic("RLC_Surface_Q008"), Is.True);
            Assert.That(Quests.DescribeActive(p.RunState), Is.EqualTo("[퀘스트] 슬라임 학살자 (완료)"));
            Assert.That(notices[notices.Count - 1], Does.StartWith("[퀘스트 완료] 슬라임 학살자 — 유물 ["));

            // 완료 뒤엔 더 안 셈
            Quests.OnMonstersKilled(p, new[] { "mon_slimeking" });
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(1));
        }

        [Test]
        public void KillGuardians_CountsByCategory_UpgradeSkillReward()
        {
            var p = NewPlayer();
            p.EquipEnhanceSkill("gale_triple_kick");
            Quests.Accept(p, "Q_SLI_004", 1);   // 가디언 2마리 -> 픽업 스킬 2회 강화

            Quests.OnMonstersKilled(p, new[] { "mon_acidslime" });
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(1));
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.False);

            Quests.OnMonstersKilled(p, new[] { "mon_ancientslime", "mon_acidslime" });   // 넘쳐도 목표에서 멈춤
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(2));
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.GetPickupSkillLevel("gale_triple_kick"), Is.EqualTo(2));
        }

        [Test]
        public void EarnCurrency_GoldRewardGoesToSettlement()
        {
            var p = NewPlayer();
            Quests.Accept(p, "Q_S_001", 1);   // 던전 재화 300 -> 골드 200 (정산)

            p.AddCurrency(120);
            p.AddCurrency(-50);   // 쓴 건 안 빠짐
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(120));
            p.AddCurrency(500);
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.RunState.QuestGold, Is.EqualTo(200));
            Assert.That(p.RunState.Currency, Is.EqualTo(570), "골드 보상은 던전 재화가 아님");
        }

        [Test]
        public void ReachFloor_AcceptRewardThenCompleteOnFloor()
        {
            var p = NewPlayer();
            Quests.Accept(p, "Q_S_006", 1);   // 2층 도착. 수락 보상 던전 재화 100, 완료 골드 400
            Assert.That(p.RunState.Currency, Is.EqualTo(100));
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.False);

            Quests.OnFloorReached(p, 2);
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.RunState.QuestGold, Is.EqualTo(400));

            // 이미 목표 층이면 수락하자마자 완료
            var q = NewPlayer();
            Quests.Accept(q, "Q_S_006", 2);
            Assert.That(q.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(q.RunState.QuestGold, Is.EqualTo(400));
        }

        [Test]
        public void EnterRoomInSingleFloor_CountsThisFloorOnly()
        {
            var p = NewPlayer();
            Quests.Accept(p, "Q_S_010", 1);   // 한 층에서 가디언 방 2번 -> 던전 재화 200

            Quests.OnRoomEntered(p, RoomType.GUARDIAN);
            Quests.OnRoomEntered(p, RoomType.MONSTER);
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(1));
            Assert.That(p.RunState.FloorMonsterRooms, Is.EqualTo(1));

            p.BeginFloor();   // 층이 바뀌면 다시 0 부터
            Quests.OnRoomEntered(p, RoomType.GUARDIAN);
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(1));
            Quests.OnRoomEntered(p, RoomType.GUARDIAN);
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.RunState.Currency, Is.EqualTo(200));
        }

        [Test]
        public void AcquireRelic_AndSpecialRoomCleared()
        {
            var p = NewPlayer();
            Quests.Accept(p, "Q_M_006", 3);   // 유물 2개 -> 던전 재화 300
            p.GainRelic("RLC_Common_001");
            p.GainRelic("RLC_Common_001");   // 이미 있음 -> 안 셈
            Assert.That(p.RunState.ActiveQuest.Progress, Is.EqualTo(1));
            p.GainRelic("RLC_Common_002");
            Assert.That(p.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(p.RunState.Currency, Is.EqualTo(300));

            var q = NewPlayer();
            Quests.Accept(q, "Q_SLI_016", 1);   // 산성 성소 클리어 -> 던전 재화 200
            Quests.OnSpecialRoomCleared(q, "Room_AncientSlime");
            Assert.That(q.RunState.ActiveQuest.Progress, Is.EqualTo(0));
            Quests.OnSpecialRoomCleared(q, "Room_AcidShrine");
            Assert.That(q.RunState.ActiveQuest.bCompleted, Is.True);
            Assert.That(q.RunState.Currency, Is.EqualTo(200));
        }

        [Test]
        public void ExpireForNewTheme_ClearsQuest()
        {
            var p = NewPlayer();
            var notices = new List<string>();
            p.Notice += notices.Add;
            Quests.Accept(p, "Q_SLI_001", 1);
            Quests.ExpireForNewTheme(p);
            Assert.That(p.RunState.ActiveQuest.Quest, Is.Empty);
            Assert.That(notices[notices.Count - 1], Does.StartWith("[퀘스트 실패] 슬라임 학살자"));
            Assert.That(Quests.DescribeActive(p.RunState), Is.Empty);
        }

        static List<RoomNode> MakeMap(int rows, int cols)
        {
            var rooms = new List<RoomNode>();
            int id = 0;
            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    var type = r == 0 ? RoomType.QUEST : (r == rows - 1 ? RoomType.BOSS : RoomType.MONSTER);
                    rooms.Add(new RoomNode { RoomId = id++, Row = r, Col = c, Type = type });
                }
            }
            return rooms;
        }

        [Test]
        public void InjectSpecialRooms_PutsRoomOnDepthRowOfMatchingFloor()
        {
            var p = NewPlayer();
            int injectCalls = 0;
            Quests.Accept(p, "Q_SLI_002", 1, () => ++injectCalls);   // Room_AncientSlime: 테마 2층, Depth 5
            Assert.That(injectCalls, Is.EqualTo(1));

            // 1층에선 안 맞음
            var floor1 = MakeMap(12, 3);
            Assert.That(Quests.InjectSpecialRooms(floor1, 1, SlimeTheme, new[] { p.RunState }, new Random(1)), Is.False);

            // 2층 (퀘스트 줄 없음): Depth 5 - 2 = 3줄
            var floor2 = MakeMap(11, 3);
            Assert.That(Quests.InjectSpecialRooms(floor2, 2, SlimeTheme, new[] { p.RunState }, new Random(1)), Is.True);
            var special = floor2.FindAll(n => n.SpecialRoom == "Room_AncientSlime");
            Assert.That(special.Count, Is.EqualTo(1));
            Assert.That(special[0].Row, Is.EqualTo(3));
            Assert.That(special[0].Type, Is.EqualTo(RoomType.GUARDIAN));

            // 이미 넣었으면 다시 안 넣음
            Assert.That(Quests.InjectSpecialRooms(floor2, 2, SlimeTheme, new[] { p.RunState }, new Random(2)), Is.False);

            // 다른 테마
            Assert.That(Quests.InjectSpecialRooms(MakeMap(11, 3), 2, "용암 지대", new[] { p.RunState }, new Random(1)), Is.False);

            Assert.That(Quests.GetSpecialRoomMonsters("Room_AncientSlime"), Is.EqualTo(new[] { "mon_ancientslime" }));
            Assert.That(Quests.GetSpecialRoomMonsters("NoSuchRoom"), Is.Empty);
        }

        [Test]
        public void InjectSpecialRooms_Depth12_IsBossRow()
        {
            var p = NewPlayer();
            Quests.Accept(p, "Q_ELM_001", 5);   // Room_Ifrit: 정령계 6층, Depth 12
            var map = MakeMap(11, 2);
            Assert.That(Quests.InjectSpecialRooms(map, 6, "정령계", new[] { p.RunState }, new Random(1)), Is.True);
            var node = map.Find(n => n.SpecialRoom == "Room_Ifrit");
            Assert.That(node.Row, Is.EqualTo(10));
            Assert.That(node.Type, Is.EqualTo(RoomType.BOSS), "보스방은 그대로 보스");
        }

        [Test]
        public void Describe_Texts()
        {
            Assert.That(Quests.DescribeReward(QuestRewardType.GiveCurrency, "Gold", 400), Is.EqualTo("골드 400 (정산 때)"));
            Assert.That(Quests.DescribeReward(QuestRewardType.GiveCurrency, "DungeonGold", 200), Is.EqualTo("던전 재화 200"));
            Assert.That(Quests.DescribeReward(QuestRewardType.UpgradeSkill, "", 0), Is.EqualTo("픽업 스킬 1회 강화"));

            var offer = Quests.DescribeOffer("Q_S_006");
            Assert.That(offer, Does.Contain("황실 · 보상: 골드 400 (정산 때)"));
            Assert.That(offer, Does.Contain("수락하면 바로: 던전 재화 100"));
            Assert.That(Quests.DescribeOffer("NoSuchQuest"), Is.Empty);
        }
    }
}
