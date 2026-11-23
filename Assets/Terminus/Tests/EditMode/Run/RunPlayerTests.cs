// RunPlayer: BeginRun 스텟 / 유물 칸 / 유물 얻기 / 강화 스킬 규칙 (언리얼 PlayerState 규칙)
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunPlayerTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static RunPlayer NewPlayer(CharacterClass c, ClassUpgrades upgrades = null)
        {
            var p = new RunPlayer(new Random(3), "Tester");
            p.SetCharacterClass(c);
            if (upgrades != null) p.SetReportedUpgrades(new[] { upgrades });
            p.BeginRun(UpgradeSettings.Default);
            return p;
        }

        // SkillRow 에 Clone 이 없어서 (Data 쪽) 필요한 칸만 복사
        static SkillRow CopySkill(SkillRow r) => new SkillRow
        {
            RowName = r.RowName, SkillType = r.SkillType, BaseValue = r.BaseValue, StatusValue = r.StatusValue,
        };

        sealed class FakeBattler : IRunBattler
        {
            public int MaxHealthDelta, Healed, AttackDelta, DefenseDelta, Revived;
            public void ModifyMaxHealth(int delta) => MaxHealthDelta += delta;
            public void Heal(int amount) => Healed += amount;
            public void ModifyAttackDefense(int attackDelta, int defenseDelta) { AttackDelta += attackDelta; DefenseDelta += defenseDelta; }
            public void ReviveFull() => ++Revived;
        }

        [Test]
        public void BeginRun_CopiesClassStats_AndBasicRelic()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            var row = GameData.FindCharacterClassRow(CharacterClass.Fighter);

            Assert.That(p.RunState.bStatsInitialized, Is.True);
            Assert.That(p.RunState.Stats.MaxHealth, Is.EqualTo(row.BaseStats.MaxHealth));
            Assert.That(p.RunState.Stats.EnhanceSlots, Is.EqualTo(row.BaseStats.EnhanceSlots));
            Assert.That(p.RunState.Stats, Is.Not.SameAs(row.BaseStats), "표 원본을 공유하면 안 됨");
            Assert.That(p.RunState.Relics, Is.EqualTo(new[] { "RLC_Fighter_001" }));
            Assert.That(p.RunState.BasicSkillLevels, Is.Empty, "강화 보고가 없으면 비어 있음");
            Assert.That(p.GetRelicCapacity(), Is.EqualTo(6));
            Assert.That(p.GetEnhanceSlotCount(), Is.EqualTo(3));
        }

        [Test]
        public void BeginRun_AppliesReportedUpgrades()
        {
            var up = new ClassUpgrades { Class = CharacterClass.Paladin };
            for (int i = 0; i < ClassUpgrades.StatCount; ++i) up.StatLevels.Add(0);
            up.StatLevels[(int)StatUpgrade.Health] = 2;         // +5 x 2
            up.StatLevels[(int)StatUpgrade.Attack] = 1;
            up.StatLevels[(int)StatUpgrade.SkillEnergy] = 3;
            up.StatLevels[(int)StatUpgrade.RelicCapacity] = 4;  // 6 + 4
            up.StatLevels[(int)StatUpgrade.PickupSlots] = 2;    // 3 + 2
            up.SkillLevels.AddRange(new[] { 1, 0, 2 });

            // 다른 직업 강화는 안 섞임
            var other = new ClassUpgrades { Class = CharacterClass.Fighter };
            other.StatLevels.AddRange(new[] { 10, 5, 5, 3, 2, 3, 9, 2 });

            var p = new RunPlayer(new Random(1));
            p.SetCharacterClass(CharacterClass.Paladin);
            p.SetReportedUpgrades(new[] { other, up });
            p.BeginRun(UpgradeSettings.Default);

            var s = p.RunState.Stats;
            Assert.That(s.MaxHealth, Is.EqualTo(70 + 10));
            Assert.That(s.Attack, Is.EqualTo(1));
            Assert.That(s.Defense, Is.EqualTo(0));
            Assert.That(s.StartSkillEnergy, Is.EqualTo(3));
            Assert.That(s.MaxRelics, Is.EqualTo(10));
            Assert.That(s.EnhanceSlots, Is.EqualTo(5));
            Assert.That(p.GetRelicCapacity(), Is.EqualTo(10));
            Assert.That(p.GetEnhanceSlotCount(), Is.EqualTo(5));
            Assert.That(p.RunState.BasicSkillLevels, Is.EqualTo(new[] { 1, 0, 2 }));
            Assert.That(p.RunState.Relics, Is.EqualTo(new[] { "RLC_Paladin_001" }));
        }

        [Test]
        public void BeginRun_ResetsPreviousRun()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            p.AddCurrency(300);
            p.GainRelic("RLC_Common_001");
            p.ChooseStartSkill("gale_step");
            p.AdvanceToRoom(4, 2);
            p.ModifyRunState(s => { s.QuestGold = 50; s.ActiveQuest.Quest = "Q_SLI_001"; s.SavedHealth = 12; });

            p.BeginRun(UpgradeSettings.Default);
            var st = p.RunState;
            Assert.That(st.Currency, Is.EqualTo(0));
            Assert.That(st.Relics, Is.EqualTo(new[] { "RLC_Fighter_001" }));
            Assert.That(st.EnhanceSkills, Is.Empty);
            Assert.That(st.bStartSkillChosen, Is.False);
            Assert.That(st.CurrentRoomId, Is.EqualTo(-1));
            Assert.That(st.VisitedRoomIds, Is.Empty);
            Assert.That(st.QuestGold, Is.EqualTo(0));
            Assert.That(st.ActiveQuest.Quest, Is.Empty);
            Assert.That(st.SavedHealth, Is.EqualTo(-1));
            Assert.That(st.SeenRelics, Is.Empty);
        }

        [Test]
        public void ReportedUpgradesFromClient_AreSanitized()
        {
            var cheat = new ClassUpgrades { Class = CharacterClass.Fighter };
            cheat.StatLevels.AddRange(new[] { 99, -3 });
            cheat.SkillLevels.AddRange(new[] { 50 });

            var p = new RunPlayer(new Random(1));
            p.SetReportedUpgradesFromClient(new List<ClassUpgrades> { cheat }, UpgradeSettings.Default);
            p.BeginRun(UpgradeSettings.Default);

            Assert.That(p.RunState.Stats.MaxHealth, Is.EqualTo(70 + 10 * 5), "Health 최대 10단계");
            Assert.That(p.RunState.Stats.Attack, Is.EqualTo(0));
            Assert.That(p.RunState.BasicSkillLevels, Is.EqualTo(new[] { 3, 0, 0 }));

            // 16개 넘게 보내면 전부 버림
            var many = new List<ClassUpgrades>();
            for (int i = 0; i < 17; ++i) many.Add(cheat);
            p.SetReportedUpgradesFromClient(many, UpgradeSettings.Default);
            p.BeginRun(UpgradeSettings.Default);
            Assert.That(p.RunState.Stats.MaxHealth, Is.EqualTo(70));
        }

        [Test]
        public void GainRelic_ClassAndCapacityRules()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            int quests = 0;
            p.RunStateChanged += _ => ++quests;

            Assert.That(p.GainRelic("RLC_Paladin_002"), Is.False, "다른 직업 유물");
            Assert.That(p.GainRelic("RLC_Monster_ELM06"), Is.False, "몬스터 유물");
            Assert.That(p.GainRelic("NoSuchRelic"), Is.False);
            Assert.That(p.GainRelic("RLC_Surface_Q008"), Is.True, "퀘스트 유물은 누구나");
            Assert.That(p.GainRelic("RLC_Surface_Q008"), Is.False, "이미 있음");
            Assert.That(p.HasSeenRelic("RLC_Surface_Q008"), Is.True);

            // 6칸 채우기 (기본 유물 + 퀘스트 유물 + 4)
            foreach (var r in new[] { "RLC_Common_001", "RLC_Common_002", "RLC_Common_003", "RLC_Common_004" })
            {
                Assert.That(p.GainRelic(r), Is.True, r);
            }
            Assert.That(p.RunState.Relics.Count, Is.EqualTo(6));
            Assert.That(p.GainRelic("RLC_Common_005"), Is.False, "칸 가득");

            // 업그레이드 유물은 기본 유물을 대체하니 꽉 차 있어도 됨
            Assert.That(p.GainRelic("RLC_Fighter_002"), Is.True);
            Assert.That(p.RunState.Relics, Does.Not.Contain("RLC_Fighter_001"));
            Assert.That(p.RunState.Relics, Does.Contain("RLC_Fighter_002"));
            Assert.That(p.RunState.Relics.Count, Is.EqualTo(6));
            Assert.That(quests, Is.GreaterThan(0));
        }

        [Test]
        public void GainRelic_OnGainEffects()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            var battler = new FakeBattler();
            p.Battler = battler;

            Assert.That(p.GainRelic("RLC_Common_011"), Is.True);   // GainMoney 333
            Assert.That(p.RunState.Currency, Is.EqualTo(333));

            Assert.That(p.GainRelic("RLC_Common_015"), Is.True);   // MaxHPUp 8
            Assert.That(p.RunState.Stats.MaxHealth, Is.EqualTo(78));
            Assert.That(battler.MaxHealthDelta, Is.EqualTo(8));

            // RandomUpgrade: 장착 픽업 스킬 중 공격 / 방어만
            p.EquipEnhanceSkill("gale_breathing");   // 특수 -> 강화 안 됨
            p.EquipEnhanceSkill("gale_triple_kick"); // 공격
            Assert.That(p.GainRelic("RLC_Common_010"), Is.True);   // RandomUpgrade 1
            Assert.That(p.GetPickupSkillLevel("gale_triple_kick"), Is.EqualTo(1));
            Assert.That(p.GetPickupSkillLevel("gale_breathing"), Is.EqualTo(0));
        }

        [Test]
        public void TempRelic_DisappearsAfterBattles()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            Assert.That(p.GainRelic("RLC_Event_LAV_Temp01"), Is.True);
            Assert.That(p.GetTempRelicBattlesLeft("RLC_Event_LAV_Temp01"), Is.EqualTo(3));
            p.AddTempBuff(new TempStatBuff { Attack = 2, BattlesLeft = 1 });
            p.AddTempBuff(new TempStatBuff { Attack = 2, BattlesLeft = 0 });   // 무시
            Assert.That(p.RunState.TempBuffs.Count, Is.EqualTo(1));

            p.ConsumeTempBuffBattle();
            Assert.That(p.RunState.TempBuffs, Is.Empty);
            Assert.That(p.GetTempRelicBattlesLeft("RLC_Event_LAV_Temp01"), Is.EqualTo(2));
            p.ConsumeTempBuffBattle();
            p.ConsumeTempBuffBattle();
            Assert.That(p.GetTempRelicBattlesLeft("RLC_Event_LAV_Temp01"), Is.EqualTo(-1));
            Assert.That(p.RunState.Relics, Does.Not.Contain("RLC_Event_LAV_Temp01"));
        }

        [Test]
        public void SwapAndRemoveRelic_KeepsPosition()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            p.GainRelic("RLC_Common_001");
            p.GainRelic("RLC_Common_002");

            Assert.That(p.SwapRelic("RLC_Common_001", "RLC_Common_003"), Is.True);
            Assert.That(p.RunState.Relics, Is.EqualTo(new[] { "RLC_Fighter_001", "RLC_Common_003", "RLC_Common_002" }));
            Assert.That(p.SwapRelic("RLC_Common_003", "RLC_Paladin_002"), Is.False, "못 얻으면 그대로");
            Assert.That(p.RunState.Relics, Is.EqualTo(new[] { "RLC_Fighter_001", "RLC_Common_003", "RLC_Common_002" }));

            Assert.That(p.RemoveRelic("RLC_Common_003"), Is.True);
            Assert.That(p.RemoveRelic("RLC_Common_003"), Is.False);
        }

        [Test]
        public void StartChoices_OncePerRun()
        {
            var p = NewPlayer(CharacterClass.Fighter);

            p.ChooseStartSkill("aura_of_valor");   // 성기사 전용 -> 장착 없이 넘어감
            Assert.That(p.RunState.EnhanceSkills, Is.Empty);
            Assert.That(p.HasChosenStartSkill, Is.True);
            p.ChooseStartSkill("gale_step");       // 이미 골랐음
            Assert.That(p.RunState.EnhanceSkills, Is.Empty);

            p.ResetStartSkill();
            p.ChooseStartSkill("gale_step");
            Assert.That(p.RunState.EnhanceSkills, Is.EqualTo(new[] { "gale_step" }));

            p.ChooseStartRelics(new[] { "RLC_Fighter_001", "RLC_Common_001", "RLC_Common_002" });
            Assert.That(p.RunState.StartRelics, Is.EqualTo(new[] { "RLC_Common_001" }), "최대 1개, 기본 유물은 보관 불가");
            Assert.That(p.RunState.Relics, Is.EqualTo(new[] { "RLC_Fighter_001", "RLC_Common_001" }));
            p.ChooseStartRelics(new[] { "RLC_Common_002" });
            Assert.That(p.RunState.Relics.Count, Is.EqualTo(2), "한 런에 한 번");
        }

        [Test]
        public void EnhanceSkills_SlotsAndPickupUpgrade()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            Assert.That(p.EquipEnhanceSkill("gale_triple_kick"), Is.True);
            Assert.That(p.EquipEnhanceSkill("iron_shirt_stance"), Is.True);
            Assert.That(p.EquipEnhanceSkill("gale_step"), Is.True);
            Assert.That(p.EquipEnhanceSkill("armor_breaker_fist"), Is.False, "3칸 가득");
            Assert.That(p.EquipEnhanceSkill("armor_breaker_fist", 1), Is.True, "바꿔 끼우기");
            Assert.That(p.RunState.EnhanceSkills, Is.EqualTo(new[] { "gale_triple_kick", "armor_breaker_fist", "gale_step" }));
            Assert.That(p.EquipEnhanceSkill("NoSuchSkill"), Is.False);

            Assert.That(p.GetCombatSkillRow(0), Is.EqualTo("fighter_attack"));
            Assert.That(p.GetCombatSkillRow(1), Is.EqualTo("fighter_defend"));
            Assert.That(p.GetCombatSkillRow(2), Is.EqualTo("fighter_special"));
            Assert.That(p.GetCombatSkillRow(3), Is.EqualTo("gale_triple_kick"));
            Assert.That(p.GetCombatSkillRow(6), Is.Empty);

            for (int i = 0; i < RunPlayer.MaxPickupSkillLevel; ++i) Assert.That(p.UpgradePickupSkill("gale_triple_kick"), Is.True);
            Assert.That(p.UpgradePickupSkill("gale_triple_kick"), Is.False, "최대 5");
            Assert.That(p.UpgradePickupSkill("gale_step"), Is.False, "특수는 강화 없음");

            // 픽업 칸: 공격 / 방어만 BaseValue + 단계
            var skill = CopySkill(GameData.FindSkillRow("gale_triple_kick"));
            int baseValue = skill.BaseValue;
            p.ApplyBasicSkillUpgrade(3, skill);
            Assert.That(skill.BaseValue, Is.EqualTo(baseValue + 5));

            Assert.That(p.UnequipEnhanceSkill("armor_breaker_fist"), Is.True);
            Assert.That(p.RunState.EnhanceSkills, Is.EqualTo(new[] { "gale_triple_kick", "gale_step" }));
        }

        [Test]
        public void ApplyBasicSkillUpgrade_TrainingLevels()
        {
            var up = new ClassUpgrades { Class = CharacterClass.Fighter };
            up.SkillLevels.AddRange(new[] { 2, 1, 3 });
            var p = NewPlayer(CharacterClass.Fighter, up);

            for (int slot = 0; slot < 3; ++slot)
            {
                var original = GameData.FindSkillRow(p.GetCombatSkillRow(slot));
                var skill = CopySkill(original);
                p.ApplyBasicSkillUpgrade(slot, skill);
                int level = up.SkillLevels[slot];
                if (original.BaseValue != 0)
                {
                    Assert.That(skill.BaseValue, Is.EqualTo(original.BaseValue + level), $"slot {slot}");
                    Assert.That(skill.StatusValue, Is.EqualTo(original.StatusValue));
                }
                else
                {
                    Assert.That(skill.StatusValue, Is.EqualTo(original.StatusValue + level), $"slot {slot}");
                }
            }
        }

        [Test]
        public void Floor_AndPermanentStats_TouchBattler()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            var battler = new FakeBattler();
            p.Battler = battler;

            p.AdvanceToRoom(7, 3);
            Assert.That(p.RunState.CurrentRoomId, Is.EqualTo(7));
            Assert.That(p.RunState.CurrentMapLevel, Is.EqualTo(4));
            Assert.That(p.RunState.VisitedRoomIds, Is.EqualTo(new[] { 7 }));

            p.ApplyPermanentStat(0, -100);
            Assert.That(p.RunState.Stats.MaxHealth, Is.EqualTo(1), "최소 1");
            p.ApplyPermanentStat(1, 2);
            p.ApplyPermanentStat(2, 3);
            Assert.That(p.RunState.Stats.Attack, Is.EqualTo(2));
            Assert.That(p.RunState.Stats.Defense, Is.EqualTo(3));
            Assert.That(battler.AttackDelta, Is.EqualTo(2));
            Assert.That(battler.DefenseDelta, Is.EqualTo(3));

            p.ModifyRunState(s => { s.FloorMonsterRooms = 3; s.BossRelic = "RLC_Common_001"; });
            p.BeginFloor();
            Assert.That(p.RunState.VisitedRoomIds, Is.Empty);
            Assert.That(p.RunState.FloorMonsterRooms, Is.EqualTo(0));
            Assert.That(p.RunState.BossRelic, Is.Empty);
            Assert.That(battler.Revived, Is.EqualTo(1));
        }

        [Test]
        public void Currency_NeverBelowZero_AndSavedHealthConsumedOnce()
        {
            var p = NewPlayer(CharacterClass.Fighter);
            p.AddCurrency(50);
            p.AddCurrency(-80);
            Assert.That(p.RunState.Currency, Is.EqualTo(0));

            p.ModifyRunState(s => s.SavedHealth = 33);
            Assert.That(p.ConsumeSavedHealth(), Is.EqualTo(33));
            Assert.That(p.ConsumeSavedHealth(), Is.EqualTo(-1));

            p.LockClassFromSave(CharacterClass.Assassin);
            Assert.That(p.bClassLocked, Is.True);
            Assert.That(p.CharacterClass, Is.EqualTo(CharacterClass.Assassin));
        }
    }
}
