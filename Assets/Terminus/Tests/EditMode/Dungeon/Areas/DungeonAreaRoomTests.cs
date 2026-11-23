// 방 타입별 진행: 휴식터 / 상점 / 퀘스트 방 / 퀘스트 특수 방 (언리얼 ADungeonArea)
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
    public class DungeonAreaRoomTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        // ================================================================ 휴식터

        [Test]
        public void Rest_HealTarget_ByRatio_OncePerPerson_ThenClearsAfterDelay()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            b.Battler.ApplyDamage(40);
            int bBefore = b.Battler.State.Health;

            Enter(rig, new[] { Room(10, RoomType.BREAK, row: 4) }, (a, 10), (b, 10));
            var area = rig.Areas[0];
            Assert.That(area.IsResting, Is.True);
            Assert.That(rig.Notifier.Count("ShowRest"), Is.EqualTo(2));
            Assert.That(rig.Combats[0].Count("StartCombat"), Is.Zero, "휴식터는 전투 없음");
            Assert.That(area.CurrentSet.Id, Is.EqualTo("Set_Slime_A"));

            rig.Manager.HandleRestChoice(a, b);
            int heal = CombatMath.RoundToInt(b.Battler.Stats.MaxHealth * 0.2f);
            Assert.That(b.Battler.State.Health, Is.EqualTo(bBefore + heal));
            Assert.That(rig.Notifier.LastResult[a], Does.Contain("B 님의 체력을"));
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("A 님이 B 님의 체력을"));

            // 한 사람 한 번
            rig.Manager.HandleRestChoice(a, b);
            Assert.That(b.Battler.State.Health, Is.EqualTo(bBefore + heal));

            // 다른 구역 사람은 대상으로 못 고름
            var outsider = MakePlayer(9, "X");
            area.HandleRestChoice(b, outsider);
            Assert.That(area.HasChosenRest(b), Is.False);

            rig.Manager.HandleRestExplore(b);
            Assert.That(b.RunState.Currency, Is.InRange(5, 10));
            Assert.That(area.IsCleared, Is.False, "결과를 잠깐 보여주고 끝");

            rig.Timers.Advance(1.4);
            Assert.That(area.IsInUse && !area.IsCleared, Is.True);
            rig.Timers.Advance(0.2);
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(10));
            Assert.That(rig.Notifier.Count("CloseRest"), Is.EqualTo(2), "구역을 비울 때 화면 닫기");
        }

        [Test]
        public void RestExplore_RelicChance_GivesRewardRelic()
        {
            var settings = new DungeonAreaSettings { RestExploreRelicChance = 1f };
            var rig = MakeRig(settings: settings);
            rig.Combats[0].RewardRelics.Add("RLC_Common_001");
            var a = MakePlayer(1, "A");

            Enter(rig, new[] { Room(10, RoomType.BREAK) }, (a, 10));
            rig.Manager.HandleRestExplore(a);

            Assert.That(a.RunState.Relics, Does.Contain("RLC_Common_001"));
            Assert.That(a.Run.HasSeenRelic("RLC_Common_001"), Is.True);
            Assert.That(rig.Notifier.LastResult[a], Does.Contain("발견했습니다"));
            Assert.That(rig.Notifier.Chat, Is.Empty, "혼자면 채팅 안 함");
        }

        // ================================================================ 상점

        static (ShopRig, DungeonPlayer) OpenShop(int floor = 1, int currency = 0)
        {
            var rig = MakeRig();
            rig.Floor = floor;
            var a = MakePlayer(1, "A");
            a.Run.AddCurrency(currency);
            Enter(rig, new[] { Room(10, RoomType.STORE) }, (a, 10));
            return (new ShopRig { Rig = rig }, a);
        }

        sealed class ShopRig
        {
            public Rig Rig;
            public DungeonArea Area => Rig.Areas[0];
        }

        [Test]
        public void Shop_Offers_CurrentTier_ClassRelics_AndPickupSkills_MarkedSeen()
        {
            var (shop, a) = OpenShop();
            var state = shop.Area.GetShopState(a);

            Assert.That(shop.Area.CurrentSet.Id, Is.EqualTo("Set_Slime_Shop"), "상점 전용 무대");
            Assert.That(state.RelicOffers.Count, Is.EqualTo(3));
            Assert.That(state.SkillOffers.Count, Is.EqualTo(3));
            Assert.That(state.UpgradeCostPerLevel, Is.EqualTo(50));
            for (int i = 0; i < state.RelicOffers.Count; ++i)
            {
                var relic = GameData.FindRelicRow(state.RelicOffers[i]);
                Assert.That(relic.RelicTier, Is.EqualTo(RelicTier.Surface));
                Assert.That(relic.CanBuyInShop(), Is.True);
                Assert.That(relic.OwnerClass == SkillOwner.Shared || relic.OwnerClass == SkillOwner.Fighter, Is.True);
                Assert.That(state.RelicPrices[i], Is.EqualTo(relic.BuyPrice_Dungeon));
                Assert.That(a.Run.HasSeenRelic(state.RelicOffers[i]), Is.True, "진열된 유물은 안 사도 이번 런엔 다시 안 나옴");
            }
            for (int i = 0; i < state.SkillOffers.Count; ++i)
            {
                var skill = GameData.FindSkillRow(state.SkillOffers[i]);
                Assert.That(skill.SkillTier, Is.EqualTo(SkillTier.Surface));
                Assert.That(skill.SkillEnergyCost, Is.GreaterThan(0));
                Assert.That(skill.SkillCategory, Is.Not.EqualTo(SkillCategory.Event));
                Assert.That(ProfileService.IsEquippableSkill(skill.RowName, CharacterClass.Fighter), Is.True);
                Assert.That(state.SkillPrices[i], Is.EqualTo(skill.BuyPrice_Dungeon > 0 ? skill.BuyPrice_Dungeon : 30));
            }
            Assert.That(shop.Rig.Notifier.Count("ShowShop", a), Is.EqualTo(1));
        }

        [Test]
        public void Shop_BuyRelic_NeedsCurrency_ThenBuysOnce()
        {
            var (shop, a) = OpenShop();
            var offer = shop.Area.GetShopState(a);
            int price = offer.RelicPrices[0];

            shop.Rig.Manager.HandleShopBuyRelic(a, 0);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("던전 재화가 부족합니다."));
            Assert.That(a.RunState.Relics, Does.Not.Contain(offer.RelicOffers[0]));

            a.Run.AddCurrency(price + 7);
            shop.Rig.Manager.HandleShopBuyRelic(a, 0);
            Assert.That(a.RunState.Relics, Does.Contain(offer.RelicOffers[0]));
            Assert.That(a.RunState.Currency, Is.EqualTo(7));
            Assert.That(shop.Area.GetShopState(a).RelicBought[0], Is.True);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Does.StartWith("구매: "));

            // 산 칸은 다시 못 삼, 잘못된 칸 무시
            a.Run.AddCurrency(1000);
            int before = a.RunState.Currency;
            shop.Rig.Manager.HandleShopBuyRelic(a, 0);
            shop.Rig.Manager.HandleShopBuyRelic(a, 99);
            Assert.That(a.RunState.Currency, Is.EqualTo(before));
        }

        [Test]
        public void Shop_BuyRelic_FullSlots_Refused()
        {
            var (shop, a) = OpenShop(currency: 5000);
            // 칸 채우기 (기본 유물 1 + 5)
            foreach (var r in new[] { "RLC_Common_002", "RLC_Common_003", "RLC_Common_004", "RLC_Common_005", "RLC_Common_006", "RLC_Common_007" })
            {
                if (a.RunState.Relics.Count >= a.Run.GetRelicCapacity()) break;
                if (shop.Area.GetShopState(a).RelicOffers.Contains(r)) continue;
                a.Run.GainRelic(r);
            }
            Assert.That(a.RunState.Relics.Count, Is.EqualTo(a.Run.GetRelicCapacity()));

            shop.Rig.Manager.HandleShopBuyRelic(a, 0);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("유물 칸이 가득 찼습니다. 먼저 유물을 판매하세요."));
            Assert.That(a.RunState.Currency, Is.EqualTo(5000));
        }

        [Test]
        public void Shop_BuySkill_EquipsAndCharges_FullSlotsNeedReplaceSlot()
        {
            var (shop, a) = OpenShop(currency: 1000);
            var offer = shop.Area.GetShopState(a);

            shop.Rig.Manager.HandleShopBuySkill(a, 0, -1);
            Assert.That(a.RunState.EnhanceSkills, Does.Contain(offer.SkillOffers[0]));
            Assert.That(a.RunState.Currency, Is.EqualTo(1000 - offer.SkillPrices[0]));

            // 칸을 채움 -> 바꿀 칸이 없으면 거절, 있으면 교체
            foreach (var s in GameData.Skills.Rows)
            {
                if (a.RunState.EnhanceSkills.Count >= a.Run.GetEnhanceSlotCount()) break;
                if (s.OwnerClass == SkillOwner.Fighter && s.SkillCategory == SkillCategory.Personal && !offer.SkillOffers.Contains(s.RowName)
                    && !a.RunState.EnhanceSkills.Contains(s.RowName))
                {
                    a.Run.EquipEnhanceSkill(s.RowName);
                }
            }
            int currency = a.RunState.Currency;
            shop.Rig.Manager.HandleShopBuySkill(a, 1, -1);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("강화 스킬 칸이 가득 찼습니다. 바꿀 칸을 고르세요."));
            Assert.That(a.RunState.Currency, Is.EqualTo(currency));

            shop.Rig.Manager.HandleShopBuySkill(a, 1, 2);
            Assert.That(a.RunState.EnhanceSkills[2], Is.EqualTo(offer.SkillOffers[1]));
            Assert.That(a.RunState.Currency, Is.EqualTo(currency - offer.SkillPrices[1]));
        }

        [Test]
        public void Shop_SellRelic_PaysSellPrice_BasicRelicRefused()
        {
            var (shop, a) = OpenShop();
            a.Run.GainRelic("RLC_Common_016");
            var relic = GameData.FindRelicRow("RLC_Common_016");

            shop.Rig.Manager.HandleShopSellRelic(a, "RLC_Common_016");
            Assert.That(a.RunState.Relics, Does.Not.Contain("RLC_Common_016"));
            Assert.That(a.RunState.Currency, Is.EqualTo(relic.SellPrice_Dungeon));
            Assert.That(a.Run.HasSeenRelic("RLC_Common_016"), Is.True);

            shop.Rig.Manager.HandleShopSellRelic(a, "RLC_Fighter_001");
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("이 유물은 팔 수 없습니다."));
            Assert.That(a.RunState.Relics, Does.Contain("RLC_Fighter_001"));

            // 안 가진 유물은 무시
            int currency = a.RunState.Currency;
            shop.Rig.Manager.HandleShopSellRelic(a, "RLC_Common_016");
            Assert.That(a.RunState.Currency, Is.EqualTo(currency));
        }

        [Test]
        public void Shop_UpgradeSkill_CostRises_TierAndTypeRules()
        {
            var (shop, a) = OpenShop(currency: 1000);
            string attack = FirstSkill(s => s.OwnerClass == SkillOwner.Fighter && s.SkillCategory == SkillCategory.Personal
                && s.SkillType == SkillType.Attack && s.SkillTier == SkillTier.Surface && s.SkillEnergyCost > 0);
            string special = FirstSkill(s => s.OwnerClass == SkillOwner.Fighter && s.SkillCategory == SkillCategory.Personal
                && s.SkillType == SkillType.Special && s.SkillTier == SkillTier.Surface);
            string mid = FirstSkill(s => s.OwnerClass == SkillOwner.Fighter && s.SkillCategory == SkillCategory.Personal
                && s.SkillType == SkillType.Attack && s.SkillTier == SkillTier.Mid);
            a.Run.EquipEnhanceSkill(attack);
            a.Run.EquipEnhanceSkill(special);
            a.Run.EquipEnhanceSkill(mid);

            shop.Rig.Manager.HandleShopUpgradeSkill(a, attack);
            Assert.That(a.Run.GetPickupSkillLevel(attack), Is.EqualTo(1));
            Assert.That(a.RunState.Currency, Is.EqualTo(1000 - 50));
            shop.Rig.Manager.HandleShopUpgradeSkill(a, attack);
            Assert.That(a.Run.GetPickupSkillLevel(attack), Is.EqualTo(2));
            Assert.That(a.RunState.Currency, Is.EqualTo(1000 - 50 - 100));
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Does.EndWith("+2"));

            shop.Rig.Manager.HandleShopUpgradeSkill(a, special);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("특수 스킬은 강화할 수 없습니다."));

            shop.Rig.Manager.HandleShopUpgradeSkill(a, mid);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("이 상점에서는 지금 계층의 내 직업 / 공용 스킬만 강화할 수 있습니다."));

            // 돈이 모자라면 안 됨 (다음 단계 3 -> 150)
            a.Run.AddCurrency(-a.RunState.Currency + 149);
            shop.Rig.Manager.HandleShopUpgradeSkill(a, attack);
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("던전 재화가 부족합니다."));
            Assert.That(a.Run.GetPickupSkillLevel(attack), Is.EqualTo(2));

            // 최대 단계
            a.Run.AddCurrency(10000);
            for (int i = 0; i < 5; ++i) shop.Rig.Manager.HandleShopUpgradeSkill(a, attack);
            Assert.That(a.Run.GetPickupSkillLevel(attack), Is.EqualTo(RunPlayer.MaxPickupSkillLevel));
            Assert.That(shop.Rig.Notifier.LastShop[a].Message, Is.EqualTo("이미 최대로 강화했습니다."));
        }

        [Test]
        public void Shop_EveryoneLeaves_ThenRoomClears()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Enter(rig, new[] { Room(10, RoomType.STORE) }, (a, 10), (b, 10));
            var area = rig.Areas[0];

            // 진열은 사람마다 따로
            Assert.That(area.GetShopState(a), Is.Not.Null);
            Assert.That(area.GetShopState(b), Is.Not.Null);

            rig.Manager.HandleShopLeave(a);
            Assert.That(rig.Notifier.Count("CloseShop", a), Is.EqualTo(1));
            a.Run.AddCurrency(1000);
            rig.Manager.HandleShopBuyRelic(a, 0);
            Assert.That(a.RunState.Currency, Is.EqualTo(1000), "나간 사람은 못 삼");

            rig.Timers.Advance(5);
            Assert.That(area.IsCleared, Is.False);

            rig.Manager.HandleShopLeave(b);
            rig.Timers.Advance(0.49);
            Assert.That(rig.RoomsFinishedCount, Is.Zero);
            rig.Timers.Advance(0.02);
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
        }

        // ================================================================ 퀘스트 방 / 특수 방

        [Test]
        public void QuestRoom_OffersPerPlayer_PlusDecline_AcceptOrDecline_ThenClears()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B", CharacterClass.Paladin, seed: 2);
            Enter(rig, new[] { Room(0, RoomType.QUEST, row: 0) }, (a, 0), (b, 0));
            var area = rig.Areas[0];

            Assert.That(area.IsInQuest, Is.True);
            var offers = area.GetQuestOffers(a);
            Assert.That(offers.Count, Is.EqualTo(3));
            var cards = rig.Notifier.LastEvent[a];
            Assert.That(cards.Count, Is.EqualTo(4), "퀘스트 3 + 거절");
            Assert.That(cards[3].Title, Is.EqualTo("의뢰를 받지 않는다"));
            Assert.That(rig.Notifier.LastEventTitle[a], Is.EqualTo("세력의 의뢰"));

            rig.Manager.HandleEventChoice(a, 0, -1, "");
            Assert.That(a.RunState.ActiveQuest.Quest, Is.EqualTo(offers[0]));
            Assert.That(rig.Notifier.LastResult[a], Does.Contain("의뢰를 수락했습니다"));

            rig.Manager.HandleEventChoice(b, 3, -1, "");
            Assert.That(Ids.IsNone(b.RunState.ActiveQuest.Quest), Is.True);
            Assert.That(rig.Notifier.LastResult[b], Is.EqualTo("의뢰를 받지 않고 길을 나섭니다."));

            rig.Timers.Advance(1.5);
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(0));
        }

        [Test]
        public void QuestRoom_AcceptingSpecialRoomQuest_RequestsInjection()
        {
            // 퀘스트 후보는 무작위라 특수 방 퀘스트가 나오는 시드를 찾음
            for (int seed = 0; seed < 200; ++seed)
            {
                var rig = MakeRig(seed: seed);
                var a = MakePlayer(1, "A");
                DungeonPlayer requested = null;
                rig.Manager.SpecialRoomInjectionRequested += p => requested = p;
                Enter(rig, new[] { Room(0, RoomType.QUEST, row: 0) }, (a, 0));

                var offers = rig.Areas[0].GetQuestOffers(a);
                int index = offers.FindIndex(q => GameData.FindQuestRow(q).bInjectSpecialRoom);
                if (index < 0) continue;

                rig.Manager.HandleEventChoice(a, index, -1, "");
                Assert.That(requested, Is.SameAs(a));
                return;
            }
            Assert.Fail("특수 방 퀘스트가 후보에 안 나옴");
        }

        [Test]
        public void SpecialRoom_ForcesQuestMonsters_AndCountsClear()
        {
            var rig = MakeRig();
            var a = MakePlayer(1, "A");
            Quests.Accept(a.Run, "Q_SLI_016");   // 특수 방 Room_AcidShrine 클리어
            Assert.That(a.RunState.ActiveQuest.Quest, Is.EqualTo("Q_SLI_016"));

            Enter(rig, new[] { Room(5, RoomType.GUARDIAN, row: 3, special: "Room_AcidShrine") }, (a, 5));
            var combat = rig.Combats[0];
            var expected = Quests.GetSpecialRoomMonsters("Room_AcidShrine");
            Assert.That(expected, Is.Not.Empty);
            Assert.That(combat.Calls, Is.EqualTo(new[] { "SetNextForcedMonsters", "StartCombat" }));
            Assert.That(combat.StartedForced, Is.EqualTo(expected));
            Assert.That(a.RunState.FloorGuardianRooms, Is.EqualTo(1), "퀘스트 '한 층에서 가디언 방 N 번'");

            combat.Clear();
            Assert.That(a.RunState.ActiveQuest.bCompleted, Is.True);
        }
    }
}
