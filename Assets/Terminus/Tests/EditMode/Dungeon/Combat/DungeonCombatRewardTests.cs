// 던전 전투: 승리 -> 방 보상 -> 보상 마침 검증 -> 구역 클리어
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatRewardTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static readonly string[] OneSlime = { "mon_slime" };

        static RoomRewardOffer OfferOf(DungeonCombatKit kit, DungeonPlayer p)
        {
            foreach (var pair in kit.Offers)
            {
                if (pair.Key == p) return pair.Value;
            }
            return null;
        }

        static DungeonCombatKit WinRoom(RoomType type, int seed, params DungeonPlayer[] players)
        {
            var kit = new DungeonCombatKit(seed);
            kit.StartForced(type, OneSlime, players);
            kit.Combat.DebugKillAllMonsters();
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.AreEqual(CombatStep.StartRoomRewards, kit.Combat.PendingStep);
            Assert.AreEqual(kit.Settings.RewardDelay, kit.Combat.PendingStepTime - kit.Now, 1e-6);
            kit.Step();
            return kit;
        }

        [Test]
        public void MonsterRoom_CurrencyAndTwoSkillOffers_ThenClearAfterAllFinish()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var p2 = DungeonCombatPlayers.Make(2, CharacterClass.Paladin);
            var kit = WinRoom(RoomType.MONSTER, 3, p1, p2);

            Assert.AreEqual(2, kit.Offers.Count);
            var o1 = OfferOf(kit, p1);
            Assert.AreEqual(RoomType.MONSTER, o1.RoomType);
            Assert.AreEqual(10, o1.Currency);
            Assert.AreEqual(10, p1.Run.RunState.Currency, "재화는 바로 지급");
            Assert.AreEqual(2, o1.SkillOffers.Count);
            Assert.AreEqual(0, o1.RelicOffers.Count);
            Assert.IsTrue(kit.Combat.IsWaitingForReward(p1));

            // 후보에 없는 스킬은 무시 (그래도 '다음으로' 는 누른 걸로)
            string notOffered = new List<string> { "gale_step", "iron_shirt_stance", "blood_flash" }.Find(s => !o1.SkillOffers.Contains(s));
            kit.Combat.HandleRewardFinished(p1, notOffered, -1, "", "");
            CollectionAssert.IsEmpty(p1.Run.RunState.EnhanceSkills);
            Assert.IsFalse(kit.Combat.IsWaitingForReward(p1));
            Assert.AreEqual(CombatStep.None, kit.Combat.PendingStep, "아직 한 명 남음");

            var o2 = OfferOf(kit, p2);
            kit.Combat.HandleRewardFinished(p2, o2.SkillOffers[1], -1, "", "");
            CollectionAssert.AreEqual(new[] { o2.SkillOffers[1] }, p2.Run.RunState.EnhanceSkills);

            // 전원 끝 -> 잠깐 뒤 구역 클리어
            Assert.AreEqual(CombatStep.ClearArea, kit.Combat.PendingStep);
            Assert.AreEqual(0, kit.Cleared);
            kit.Step();
            Assert.AreEqual(1, kit.Cleared);

            // 두 번째 요청은 무시
            kit.Combat.HandleRewardFinished(p2, o2.SkillOffers[0], -1, "", "");
            Assert.AreEqual(1, p2.Run.RunState.EnhanceSkills.Count);
        }

        [Test]
        public void SkillOffers_FollowRewardRules()
        {
            foreach (int floor in new[] { 1, 3, 5 })
            {
                for (int seed = 1; seed <= 15; ++seed)
                {
                    var p = DungeonCombatPlayers.Make(1, (CharacterClass)(seed % 4));
                    p.Run.ModifyRunState(s => s.EnhanceSkills.Add(seed % 2 == 0 ? "gale_step" : "evt_blood_pact_strike"));

                    var kit = new DungeonCombatKit(seed);
                    kit.Host.CurrentFloor = floor;
                    kit.StartForced(RoomType.MONSTER, OneSlime, p);
                    kit.Combat.DebugKillAllMonsters();
                    kit.Step();

                    var offer = OfferOf(kit, p);
                    Assert.AreEqual(2, offer.SkillOffers.Count);
                    Assert.AreNotEqual(offer.SkillOffers[0], offer.SkillOffers[1]);
                    var maxTier = floor >= 5 ? SkillTier.Deep : (floor >= 3 ? SkillTier.Mid : SkillTier.Surface);
                    foreach (var row in offer.SkillOffers)
                    {
                        var skill = GameData.FindSkillRow(row);
                        Assert.IsTrue(ProfileService.IsEquippableSkill(row, p.Run.CharacterClass), row);
                        Assert.That(skill.SkillEnergyCost, Is.GreaterThan(0), row);
                        Assert.AreNotEqual(SkillCategory.Event, skill.SkillCategory, row);
                        Assert.That((int)skill.SkillTier, Is.LessThanOrEqualTo((int)maxTier), row);
                        CollectionAssert.DoesNotContain(p.Run.RunState.EnhanceSkills, row, "장착한 건 안 나옴");
                    }
                }
            }
        }

        [Test]
        public void GuardianRoom_OneRelic_MarkedSeen_OnlyOfferedAccepted()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = WinRoom(RoomType.GUARDIAN, 5, p);

            var offer = OfferOf(kit, p);
            Assert.AreEqual(20, offer.Currency);
            Assert.AreEqual(0, offer.SkillOffers.Count);
            Assert.AreEqual(1, offer.RelicOffers.Count);
            string relic = offer.RelicOffers[0];
            var row = GameData.FindRelicRow(relic);
            Assert.AreEqual(RelicTier.Surface, row.RelicTier, "1층 = 표층");
            Assert.IsTrue(RunPlayer.CanClassHoldRelic(CharacterClass.Fighter, row));
            Assert.IsTrue(p.Run.HasSeenRelic(relic), "안 골라도 이번 런엔 다시 안 나옴");

            kit.Combat.HandleRewardFinished(p, "", -1, "RLC_Common_201", "");
            CollectionAssert.DoesNotContain(p.Run.Relics, "RLC_Common_201", "후보에 없던 유물은 무시");
            Assert.AreEqual(CombatStep.ClearArea, kit.Combat.PendingStep);

            // 같은 방을 다시: 이번엔 후보를 고름
            var p2 = DungeonCombatPlayers.Make(2);
            kit = WinRoom(RoomType.GUARDIAN, 5, p2);
            relic = OfferOf(kit, p2).RelicOffers[0];
            kit.Combat.HandleRewardFinished(p2, "", -1, relic, "");
            CollectionAssert.Contains(p2.Run.Relics, relic);
            Assert.AreEqual("", p2.Run.RunState.BossRelic, "가디언방은 보스 보상 기록 없음");
        }

        [Test]
        public void BossRoom_RelicAndSkill_RecordedForBetrayal()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = WinRoom(RoomType.BOSS, 6, p);

            var offer = OfferOf(kit, p);
            Assert.AreEqual(40, offer.Currency);
            Assert.AreEqual(1, offer.SkillOffers.Count);
            Assert.AreEqual(1, offer.RelicOffers.Count);

            kit.Combat.HandleRewardFinished(p, offer.SkillOffers[0], -1, offer.RelicOffers[0], "");
            Assert.AreEqual(offer.SkillOffers[0], p.Run.RunState.BossSkill);
            Assert.AreEqual(offer.RelicOffers[0], p.Run.RunState.BossRelic);
        }

        [Test]
        public void RelicReward_WhenFull_DiscardsChosenNonBasic()
        {
            // 칸 6 을 전투에 영향 없는 유물로 채움 (기본 유물 포함)
            string[] filler = { "RLC_Fighter_001", "RLC_Common_012", "RLC_Common_112", "RLC_Common_212", "RLC_Common_110", "RLC_Common_210" };
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, filler);
            Assert.AreEqual(p.Run.GetRelicCapacity(), p.Run.Relics.Count);
            var kit = WinRoom(RoomType.GUARDIAN, 8, p);
            string relic = OfferOf(kit, p).RelicOffers[0];

            // 기본 유물은 못 버림 -> 못 받음
            kit.Combat.HandleRewardFinished(p, "", -1, relic, "RLC_Fighter_001");
            CollectionAssert.DoesNotContain(p.Run.Relics, relic);
            CollectionAssert.Contains(p.Run.Relics, "RLC_Fighter_001");

            var p2 = DungeonCombatPlayers.Make(2, CharacterClass.Fighter, false, filler);
            kit = WinRoom(RoomType.GUARDIAN, 8, p2);
            relic = OfferOf(kit, p2).RelicOffers[0];
            kit.Combat.HandleRewardFinished(p2, "", -1, relic, "RLC_Common_012");
            CollectionAssert.Contains(p2.Run.Relics, relic);
            CollectionAssert.DoesNotContain(p2.Run.Relics, "RLC_Common_012");
        }

        [Test]
        public void PickRewardRelics_TierClassAndSeenFilters()
        {
            var kit = new DungeonCombatKit(9);
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Engineer);
            p.Run.MarkRelicsSeen(new[] { "RLC_Common_101" });
            kit.Host.CurrentFloor = 3;

            var picked = kit.Combat.PickRewardRelics(p, 100);
            Assert.That(picked.Count, Is.GreaterThan(5));
            CollectionAssert.AllItemsAreUnique(picked);
            CollectionAssert.DoesNotContain(picked, "RLC_Common_101");
            foreach (var row in picked)
            {
                var relic = GameData.FindRelicRow(row);
                Assert.AreEqual(RelicTier.Mid, relic.RelicTier, row);
                Assert.That(relic.OwnerClass == SkillOwner.Shared || relic.OwnerClass == SkillOwner.Engineer, row);
            }
            Assert.AreEqual(2, kit.Combat.PickRewardRelics(p, 2).Count);
            Assert.AreEqual(0, kit.Combat.PickRewardRelics(p, 0).Count);
        }

        [Test]
        public void Victory_RevivesDeadAfterBattleEndRelics_ConsumesTempBuffs()
        {
            var alive = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_008");
            var dead = DungeonCombatPlayers.Make(2, CharacterClass.Fighter, false, "RLC_Common_008");
            alive.Run.AddTempBuff(new TempStatBuff { Attack = 2, Defense = -1, BattlesLeft = 1 });

            var kit = new DungeonCombatKit(4);
            kit.StartForced(RoomType.MONSTER, OneSlime, alive, dead);
            Assert.AreEqual(2, alive.Battler.GetStatusValue(StatusEffect.Brave), "일시 버프: 용기");
            Assert.AreEqual(1, alive.Battler.GetStatusValue(StatusEffect.Lava), "일시 버프: 용암");

            DungeonCombatPlayers.Kill(dead);
            kit.Combat.DebugKillAllMonsters();

            Assert.AreEqual(10, alive.Run.RunState.Currency, "전투 종료 유물");
            Assert.AreEqual(0, dead.Run.RunState.Currency, "쓰러져 있던 사람은 전투 종료 유물 없음");
            Assert.AreEqual(1, DungeonCombatPlayers.Hp(dead), "이기면 체력 1 로 부활");
            Assert.AreEqual(0, alive.Battler.State.Statuses.Count, "전투 효과는 지움");
            CollectionAssert.IsEmpty(alive.Run.RunState.TempBuffs, "일시 버프 1 소모");
        }

        [Test]
        public void DisconnectedPlayer_NotOfferedNorWaited()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var p2 = DungeonCombatPlayers.Make(2);
            var kit = new DungeonCombatKit(4);
            kit.StartForced(RoomType.MONSTER, OneSlime, p1, p2);
            kit.Combat.DebugKillAllMonsters();
            p2.bConnected = false;
            kit.Step();

            Assert.AreEqual(1, kit.Offers.Count);
            kit.Combat.HandleRewardFinished(p1, "", -1, "", "");
            Assert.AreEqual(CombatStep.ClearArea, kit.Combat.PendingStep);
        }

        [Test]
        public void EndCombatDuringRewards_ClosesOpenRewardScreens()
        {
            var p1 = DungeonCombatPlayers.Make(1);
            var p2 = DungeonCombatPlayers.Make(2);
            var kit = WinRoom(RoomType.MONSTER, 4, p1, p2);
            kit.Combat.HandleRewardFinished(p1, "", -1, "", "");

            kit.Combat.EndCombat();
            CollectionAssert.AreEqual(new[] { p2 }, kit.Closed);
            Assert.AreEqual(CombatPhase.None, kit.Combat.Phase);
            Assert.AreEqual(CombatStep.None, kit.Combat.PendingStep);
            Assert.AreEqual(0, kit.Cleared);
        }

        [Test]
        public void Victory_QuestKillProgress()
        {
            var p = DungeonCombatPlayers.Make(1);
            var quest = FindKillQuest();
            if (quest == null) Assert.Ignore("DT_Quest 에 KillMonster 퀘스트가 없음");
            p.Run.ModifyRunState(s => s.ActiveQuest = new QuestProgress { Quest = quest.RowName });

            var target = FindMonsterFor(quest.ConditionTargetID);
            if (target == null) Assert.Ignore("퀘스트 대상 몬스터를 못 찾음");

            var kit = new DungeonCombatKit(4);
            kit.StartForced(RoomType.MONSTER, new[] { target }, p);
            kit.Combat.DebugKillAllMonsters();
            var progress = p.Run.RunState.ActiveQuest;
            Assert.IsTrue(progress.Progress > 0 || progress.bCompleted, "몬스터 처치 퀘스트 진행");
        }

        static QuestRow FindKillQuest()
        {
            foreach (var q in GameData.Quests.Rows)
            {
                if (q.ConditionType == QuestConditionType.KillMonster && FindMonsterFor(q.ConditionTargetID) != null) return q;
            }
            return null;
        }

        static string FindMonsterFor(string target)
        {
            foreach (var m in GameData.Monsters.Rows)
            {
                if (m.MonsterID == target || m.MonsterCategory.ToString() == target || m.RowName == target) return m.RowName;
            }
            return null;
        }
    }
}
