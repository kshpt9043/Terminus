// 이벤트 방: DT_Event / DT_EventChoice 로 선택지 만들기, 대가 / 결과 / 고를 수 있는지, 쉬는 가디언 전투 (언리얼 ADungeonArea 이벤트 부분)
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;
using static Terminus.Tests.AreaTestKit;

namespace Terminus.Tests
{
    public class DungeonAreaEventTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static Rig EventRig(string forcedEvent, out DungeonPlayer a, int floor = 1, int currency = 0)
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = forcedEvent });
            rig.Floor = floor;
            a = MakePlayer(1, "A");
            a.Run.AddCurrency(currency);
            Enter(rig, new[] { Room(7, RoomType.EVENT, row: 3) }, (a, 7));
            return rig;
        }

        // ---------------------------------------------------------------- 대가 읽기

        [Test]
        public void ParseCost_ReadsTooltipBracket_FallsBackToColumns()
        {
            var o = new EventOption();
            AreaEventRules.ParseCost(GameData.FindEventChoiceRow("CH_CMN_002_B"), o);   // [체력 8 감소 + 던전 재화 30 소모]
            Assert.That(o.CostHealth, Is.EqualTo(8));
            Assert.That(o.CostCurrency, Is.EqualTo(30), "칸(LoseHP 8)에 없는 재화 대가도 문구에서 읽음");

            o = new EventOption();
            AreaEventRules.ParseCost(GameData.FindEventChoiceRow("CH_CMN_004_B"), o);   // [최대 체력 5 감소]
            Assert.That(o.CostMaxHealth, Is.EqualTo(5));
            Assert.That(o.CostHealth, Is.EqualTo(0), "'최대 체력' 을 '체력' 으로 두 번 세지 않음");

            o = new EventOption();
            AreaEventRules.ParseCost(GameData.FindEventChoiceRow("CH_CMN_011_B"), o);   // [보유 유물 1개 무작위 소실 + 체력 9 감소]
            Assert.That(o.CostRelics, Is.EqualTo(1));
            Assert.That(o.CostHealth, Is.EqualTo(9));

            o = new EventOption();
            AreaEventRules.ParseCost(GameData.FindEventChoiceRow("CH_CMN_013_C"), o);   // 디버프 유물
            Assert.That(o.CostRelic, Is.EqualTo(AreaEventRules.DebuffRelicRow));

            // 문구가 없으면 칸 값
            var custom = new EventChoiceRow { ChoiceTooltip_KR = "대가 설명 없음", CostType = EventCostType.LoseRandomSkill, CostValue = 0 };
            o = new EventOption();
            AreaEventRules.ParseCost(custom, o);
            Assert.That(o.CostSkills, Is.EqualTo(1));
        }

        // ---------------------------------------------------------------- 이벤트 고르기

        [Test]
        public void PickEventRow_ThemeAndTierFilter_OneTimeEventsNotRepeated()
        {
            var rng = new System.Random(5);
            var seen = new List<string>();
            for (int i = 0; i < 300; ++i)
            {
                var row = AreaEventRules.PickEventRow(1, "슬라임 왕국", seen, rng);
                var ev = GameData.FindEventRow(row);
                Assert.That(ev, Is.Not.Null);
                Assert.That(ev.EventScope == "공용" || ev.EventScope == "슬라임 왕국", Is.True, row);
                Assert.That(ev.AppearTier == EventAppearTier.Any || ev.AppearTier == EventAppearTier.Surface, Is.True, row);
                Assert.That(seen.Contains(row) && ev.bOneTimePerRun, Is.False, "한 런 한 번 이벤트가 다시 나옴: " + row);
                seen.Add(row);
            }

            // 심층: 정령계 이벤트는 Deep, 슬라임 이벤트(Surface)는 안 나옴
            for (int i = 0; i < 100; ++i)
            {
                var ev = GameData.FindEventRow(AreaEventRules.PickEventRow(5, "정령계", null, rng));
                Assert.That(ev.EventScope, Is.Not.EqualTo("슬라임 왕국"));
            }

            Assert.That(AreaEventRules.PickEventRow(1, "", null, rng, "evt_statue_of_trial"), Is.EqualTo("evt_statue_of_trial"), "디버그 강제");
        }

        [Test]
        public void SameRunSeed_SameRoom_SameEventAndRolls()
        {
            string Roll(int runSeed)
            {
                var rig = MakeRig(seed: 99);
                var session = new RunSession(new System.Random(runSeed));
                session.BeginNewRun(1);
                rig.Manager.Session = session;
                var a = MakePlayer(1, "A");
                Enter(rig, new[] { Room(7, RoomType.EVENT, row: 3) }, (a, 7));
                var area = rig.Areas[0];
                Assert.That(session.SeenEvents, Does.Contain(area.CurrentEvent));
                var opts = area.GetEventOptions(a);
                return area.CurrentEvent + "|" + string.Join(",", opts.ConvertAll(o => o.SkillRow + "/" + o.RelicRow + "/" + o.Amount));
            }

            Assert.That(Roll(1234), Is.EqualTo(Roll(1234)), "나갔다가 이어하기로 다시 들어와도 같은 이벤트");
        }

        // ---------------------------------------------------------------- 선택지

        [Test]
        public void Options_BuiltFromChoiceRows_WithAvailability()
        {
            var rig = EventRig("evt_wandering_instructor", out var a);
            var area = rig.Areas[0];
            var options = area.GetEventOptions(a);

            Assert.That(area.CurrentEvent, Is.EqualTo("evt_wandering_instructor"));
            Assert.That(rig.Notifier.LastEventTitle[a], Is.EqualTo(GameData.FindEventRow("evt_wandering_instructor").EventName_KR));
            Assert.That(options.ConvertAll(o => o.ChoiceRow), Is.EqualTo(new[] { "CH_CMN_002_A", "CH_CMN_002_B", "CH_CMN_002_C" }));
            Assert.That(options[0].Title, Is.EqualTo(GameData.FindEventChoiceRow("CH_CMN_002_A").ChoiceTitle_KR));

            // A: 재화 35 -> 없음
            Assert.That(options[0].CostCurrency, Is.EqualTo(35));
            Assert.That(options[0].bAvailable, Is.False);
            Assert.That(options[0].UnavailableReason, Is.EqualTo("던전 재화가 부족합니다."));
            Assert.That(options[0].SkillRow, Is.Not.Empty, "받을 스킬은 미리 정해 둠");
            var skill = GameData.FindSkillRow(options[0].SkillRow);
            Assert.That(skill.OwnerClass, Is.EqualTo(SkillOwner.Fighter));
            Assert.That(skill.SkillTier, Is.EqualTo(SkillTier.Surface));

            // B: 한 계층 위 스킬
            Assert.That(GameData.FindSkillRow(options[1].SkillRow).SkillTier, Is.EqualTo(SkillTier.Mid));

            // C: 회복 4
            Assert.That(options[2].Outcome, Is.EqualTo(EventOutcomeType.HealHP));
            Assert.That(options[2].Amount, Is.EqualTo(4));
            Assert.That(options[2].bAvailable, Is.True);
        }

        [Test]
        public void Options_HealthCost_NeedsMoreThanCost()
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = "evt_mimic_roulette_chest" });
            var a = MakePlayer(1, "A");
            a.Battler.ApplyDamage(a.Battler.State.Health - 7);   // 체력 7 (대가 7)
            Enter(rig, new[] { Room(7, RoomType.EVENT) }, (a, 7));

            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[1].CostHealth, Is.EqualTo(7));
            Assert.That(options[1].bAvailable, Is.False);
            Assert.That(options[1].UnavailableReason, Is.EqualTo("체력이 부족합니다."));
        }

        [Test]
        public void Choose_Currency_And_HealthCost_Applied_ThenRoomClears()
        {
            var rig = EventRig("evt_mimic_roulette_chest", out var a);
            var area = rig.Areas[0];
            var options = area.GetEventOptions(a);
            int health = a.Battler.State.Health;

            Assert.That(options[1].Amount, Is.InRange(80, 140));
            rig.Manager.HandleEventChoice(a, 1, -1, "");

            Assert.That(a.Battler.State.Health, Is.EqualTo(health - 7));
            Assert.That(a.RunState.Currency, Is.EqualTo(options[1].Amount));
            Assert.That(rig.Notifier.LastResult[a], Does.Contain("체력 -7"));
            Assert.That(rig.Notifier.LastResult[a], Does.Contain($"던전 재화 +{options[1].Amount}"));
            Assert.That(area.HasChosenEvent(a), Is.True);

            // 두 번 못 고름
            rig.Manager.HandleEventChoice(a, 0, -1, "");
            Assert.That(a.RunState.Currency, Is.EqualTo(options[1].Amount));

            rig.Timers.Advance(1.5);
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(rig.Combats[0].Count("StartCombat"), Is.Zero);
        }

        [Test]
        public void Choose_DebuffRelicCost_GivesCurseRelic()
        {
            var rig = EventRig("evt_mimic_roulette_chest", out var a);
            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[2].CostRelic, Is.EqualTo("RLC_Event_Temp_006"));
            Assert.That(options[2].bAvailable, Is.True);

            rig.Manager.HandleEventChoice(a, 2, -1, "");
            Assert.That(a.RunState.Relics, Does.Contain("RLC_Event_Temp_006"));
            Assert.That(a.Run.GetTempRelicBattlesLeft("RLC_Event_Temp_006"), Is.EqualTo(3));
            Assert.That(a.RunState.Currency, Is.InRange(150, 220));
        }

        [Test]
        public void Choose_MaxHealthCost_AndPermRelic()
        {
            var rig = EventRig("evt_fallen_comrades_Echo", out var a);
            int maxBefore = a.RunState.Stats.MaxHealth;
            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[2].RelicRow, Is.EqualTo("RLC_Event_Perm_001"));
            Assert.That(a.Run.HasSeenRelic("RLC_Event_Perm_001"), Is.True, "카드에 보인 유물은 이번 런에 다시 안 나옴");

            rig.Manager.HandleEventChoice(a, 1, -1, "");   // [최대 체력 5 감소] 다른 직업 스킬
            Assert.That(a.RunState.Stats.MaxHealth, Is.EqualTo(maxBefore - 5));
            Assert.That(a.Battler.Stats.MaxHealth, Is.EqualTo(maxBefore - 5));
            Assert.That(a.RunState.EnhanceSkills, Does.Contain(options[1].SkillRow));
            Assert.That(GameData.FindSkillRow(options[1].SkillRow).OwnerClass, Is.Not.EqualTo(SkillOwner.Fighter));
        }

        [Test]
        public void Choose_Unaffordable_ResendsFreshOptions_KeepingRolls()
        {
            var rig = EventRig("evt_wandering_instructor", out var a, currency: 40);
            var area = rig.Areas[0];
            var before = area.GetEventOptions(a);
            Assert.That(before[0].bAvailable, Is.True);

            a.Run.AddCurrency(-10);   // 그새 재화가 줄어듦
            rig.Manager.HandleEventChoice(a, 0, -1, "");

            Assert.That(area.HasChosenEvent(a), Is.False, "다시 고르게 함");
            Assert.That(rig.Notifier.Count("ShowEvent", a), Is.EqualTo(2));
            var fresh = rig.Notifier.LastEvent[a];
            Assert.That(fresh[0].bAvailable, Is.False);
            Assert.That(fresh[0].SkillRow, Is.EqualTo(before[0].SkillRow), "미리 보여 준 스킬은 그대로");
            Assert.That(a.RunState.Currency, Is.EqualTo(30), "대가만 치르는 일 없음");

            // 잘못된 번호도 다시 고르게
            rig.Manager.HandleEventChoice(a, 9, -1, "");
            Assert.That(area.HasChosenEvent(a), Is.False);
        }

        [Test]
        public void Choose_RelicWhenFull_NeedsRelicToDrop()
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = "evt_alchemist_cauldron" });
            var a = MakePlayer(1, "A");
            foreach (var r in new[] { "RLC_Common_002", "RLC_Common_003", "RLC_Common_004", "RLC_Common_005", "RLC_Common_006" }) a.Run.GainRelic(r);
            Assert.That(a.RunState.Relics.Count, Is.EqualTo(a.Run.GetRelicCapacity()));
            Enter(rig, new[] { Room(7, RoomType.EVENT) }, (a, 7));

            rig.Manager.HandleEventChoice(a, 0, -1, "");
            Assert.That(rig.Areas[0].HasChosenEvent(a), Is.False, "버릴 유물을 안 골랐음");

            rig.Manager.HandleEventChoice(a, 0, -1, "RLC_Fighter_001");
            Assert.That(rig.Areas[0].HasChosenEvent(a), Is.False, "기본 유물은 버릴 수 없음");

            rig.Manager.HandleEventChoice(a, 0, -1, "RLC_Common_003");
            Assert.That(a.RunState.Relics, Does.Not.Contain("RLC_Common_003"));
            Assert.That(a.RunState.Relics, Does.Contain("RLC_Event_Temp_001"));
            Assert.That(rig.Notifier.LastResult[a], Does.Contain("(전투 3번)"));
        }

        [Test]
        public void Choose_SwapRelic_SameTier()
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = "evt_goblin_relic_broker" });
            var a = MakePlayer(1, "A");
            a.Run.GainRelic("RLC_Common_016");
            Enter(rig, new[] { Room(7, RoomType.EVENT) }, (a, 7));

            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[0].CostRelics, Is.EqualTo(0), "바뀌는 유물이 곧 대가");
            Assert.That(options[0].Amount, Is.EqualTo(20), "PlusGold");

            rig.Manager.HandleEventChoice(a, 0, -1, "");
            Assert.That(a.RunState.Relics, Does.Not.Contain("RLC_Common_016"));
            Assert.That(a.RunState.Relics.Count, Is.EqualTo(2));
            Assert.That(GameData.FindRelicRow(a.RunState.Relics[1]).RelicTier, Is.EqualTo(RelicTier.Surface));
            Assert.That(a.RunState.Currency, Is.EqualTo(20));
        }

        [Test]
        public void NoEventData_ShowsFallbackText_AndCountsAsChosen()
        {
            // 이벤트 표가 비면 후보 없음 -> 기본 문구, 고를 게 없음 -> 고른 걸로 -> 잠깐 뒤 끝
            var dir = TestData.DataDir;
            GameData.Load(name => name == GameData.EventTableName ? null
                : (System.IO.File.Exists(System.IO.Path.Combine(dir, name + ".csv")) ? System.IO.File.ReadAllText(System.IO.Path.Combine(dir, name + ".csv")) : null));
            try
            {
                var rig = MakeRig();
                var a = MakePlayer(1, "A");
                Enter(rig, new[] { Room(7, RoomType.EVENT) }, (a, 7));

                Assert.That(rig.Areas[0].CurrentEvent, Is.Empty);
                Assert.That(rig.Notifier.LastEventTitle[a], Is.EqualTo("이벤트"));
                Assert.That(rig.Notifier.LastEvent[a], Is.Empty);
                Assert.That(rig.Areas[0].HasChosenEvent(a), Is.True);
                rig.Timers.Advance(1.5);
                Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            }
            finally
            {
                TestData.LoadGameData();
            }
        }

        // ---------------------------------------------------------------- 쉬는 가디언

        [Test]
        public void WeakGuardian_AfterEveryoneChooses_StartsGuardianFight_WithHealthScale()
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = "evt_sleeping_slime_guardian" });
            var a = MakePlayer(1, "A");
            var b = MakePlayer(2, "B");
            Enter(rig, new[] { Room(7, RoomType.EVENT, row: 5) }, (a, 7), (b, 7));
            var area = rig.Areas[0];
            var combat = rig.Combats[0];

            var options = area.GetEventOptions(a);
            Assert.That(options.Count, Is.EqualTo(2));
            Assert.That(options[0].Outcome, Is.EqualTo(EventOutcomeType.StartWeakGuardianBattle));
            Assert.That(options[0].Amount, Is.EqualTo(70));

            rig.Manager.HandleEventChoice(a, 0, -1, "");
            Assert.That(area.PendingGuardianHealth, Is.EqualTo(0.7f).Within(1e-5));
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("[이벤트] A 님"));
            rig.Manager.HandleEventChoice(b, 1, -1, "");   // B 는 재화. 그래도 전원 싸움

            rig.Timers.Advance(1.5);
            Assert.That(combat.Count("StartCombat"), Is.Zero, "전투가 있으면 조금 더 길게 보여줌");
            rig.Timers.Advance(1.0);

            Assert.That(combat.Calls, Is.EqualTo(new[] { "SetNextSpawnHealthScale", "StartCombat" }));
            Assert.That(combat.StartedHealthScale, Is.EqualTo(0.7f).Within(1e-5));
            Assert.That(combat.LastRoom.Type, Is.EqualTo(RoomType.GUARDIAN));
            Assert.That(combat.LastRoom.RoomId, Is.EqualTo(7));
            Assert.That(combat.LastPlayers, Is.EqualTo(new[] { a, b }));
            Assert.That(rig.Notifier.Count("CloseEvent"), Is.EqualTo(2));
            Assert.That(area.IsInEvent, Is.False);
            Assert.That(area.IsCleared, Is.False);

            combat.Clear();
            Assert.That(rig.RoomsFinishedCount, Is.EqualTo(1));
            Assert.That(a.RunState.CurrentRoomId, Is.EqualTo(7));
        }

        [Test]
        public void WeakGuardian_WinBonus_FromTooltip_GivenOnClear()
        {
            var rig = EventRig("evt_hibernating_magma_golem", out var a);
            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[0].BonusWinCurrency, Is.EqualTo(55));
            Assert.That(options[0].bBonusWinRelic, Is.True);

            rig.Manager.HandleEventChoice(a, 0, -1, "");
            rig.Timers.Advance(2.5);
            Assert.That(rig.Combats[0].Count("StartCombat"), Is.EqualTo(1));
            int relics = a.RunState.Relics.Count;

            rig.Combats[0].Clear();
            Assert.That(a.RunState.Currency, Is.EqualTo(55));
            Assert.That(a.RunState.Relics.Count, Is.EqualTo(relics + 1), "테마(퀘스트) 유물");
            Assert.That(GameData.FindRelicRow(a.RunState.Relics[a.RunState.Relics.Count - 1]).OwnerClass, Is.EqualTo(SkillOwner.Quest));
            Assert.That(rig.Notifier.Chat, Has.Some.Contains("가디언 기습 성공"));
        }

        [Test]
        public void HealOutcome_AndBonusUpgradeFromTooltip()
        {
            var rig = MakeRig(settings: new DungeonAreaSettings { DebugForcedEvent = "evt_obsidian_forge_altar" });
            var a = MakePlayer(1, "A");
            string attack = FirstSkill(s => s.OwnerClass == SkillOwner.Fighter && s.SkillCategory == SkillCategory.Personal
                && s.SkillType == SkillType.Attack && s.SkillEnergyCost > 0);
            a.Run.EquipEnhanceSkill(attack);
            a.Battler.ApplyDamage(20);
            int health = a.Battler.State.Health;
            Enter(rig, new[] { Room(7, RoomType.EVENT) }, (a, 7));

            // CH_LAV_002_C: [대가 없음] 체력을 8 회복하고 ... +1강 강화 (결과 칸은 UpgradeRandomSkill)
            var options = rig.Areas[0].GetEventOptions(a);
            Assert.That(options[2].Outcome, Is.EqualTo(EventOutcomeType.UpgradeRandomSkill));
            Assert.That(options[2].BonusHeal, Is.EqualTo(8));
            Assert.That(options[2].BonusUpgrades, Is.EqualTo(0), "결과가 이미 강화면 추가 강화로 안 셈");

            rig.Manager.HandleEventChoice(a, 2, -1, "");
            Assert.That(a.Run.GetPickupSkillLevel(attack), Is.EqualTo(1));
            Assert.That(a.Battler.State.Health, Is.EqualTo(health + 8));
        }
    }
}
