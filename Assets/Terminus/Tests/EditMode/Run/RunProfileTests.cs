// ProfileService: 골드 / 거점 강화 구매 / 창고 / 보유 스킬 / 세이브 복구
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunProfileTests
    {
        MemorySaveStorage _storage;
        RunXmlTestSerializer _serializer;

        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        [SetUp]
        public void NewStorage()
        {
            _storage = new MemorySaveStorage();
            _serializer = new RunXmlTestSerializer();
        }

        ProfileService NewProfile(int seed = 1)
        {
            var p = new ProfileService(_storage, _serializer, new Random(seed), UpgradeSettings.Default);
            p.Load();
            return p;
        }

        [Test]
        public void Gold_AddSpend_AndEvents()
        {
            var p = NewProfile();
            var events = new List<(int, int)>();
            p.GoldChanged += (gold, delta) => events.Add((gold, delta));

            p.AddGold(500);
            p.AddGold(0);       // 무시
            p.AddGold(-100);    // 무시
            Assert.That(p.Gold, Is.EqualTo(500));

            Assert.That(p.TrySpendGold(600), Is.False);
            Assert.That(p.Gold, Is.EqualTo(500));
            Assert.That(p.TrySpendGold(-1), Is.False);
            Assert.That(p.TrySpendGold(0), Is.True);
            Assert.That(p.TrySpendGold(200), Is.True);
            Assert.That(p.Gold, Is.EqualTo(300));

            Assert.That(events, Is.EqualTo(new[] { (500, 500), (300, -200) }));

            // int 넘침 방지
            p.AddGold(int.MaxValue);
            Assert.That(p.Gold, Is.EqualTo(int.MaxValue));

            p.SetGoldForDebug(-5);
            Assert.That(p.Gold, Is.EqualTo(0));
        }

        [Test]
        public void Gold_PersistsAcrossLoad_WithBackup()
        {
            var p = NewProfile();
            p.AddGold(777);
            Assert.That(_storage.Exists(ProfileService.ProfileSlotName), Is.True);
            Assert.That(_storage.Exists(ProfileService.ProfileBackupSlotName), Is.True);

            var again = NewProfile();
            Assert.That(again.Gold, Is.EqualTo(777));
            Assert.That(again.ConsumeLoadNotice(), Is.Empty);
        }

        [Test]
        public void TryUpgradeStat_UsesSettingsCostsAndLimit()
        {
            var settings = UpgradeSettings.Default;
            var p = NewProfile();
            p.AddGold(100000);

            // 공격: 최대 5단계, 1000 + 500 x 단계
            int spent = 0;
            for (int level = 0; level < 5; ++level)
            {
                int cost = settings.GetStatCost(StatUpgrade.Attack, level);
                Assert.That(cost, Is.EqualTo(1000 + 500 * level));
                Assert.That(p.TryUpgradeStat(CharacterClass.Fighter, StatUpgrade.Attack), Is.True, $"level {level}");
                spent += cost;
            }
            Assert.That(p.Gold, Is.EqualTo(100000 - spent));
            Assert.That(p.GetClassUpgrades(CharacterClass.Fighter).GetStatLevel(StatUpgrade.Attack), Is.EqualTo(5));

            // 최대면 실패하고 골드는 그대로
            int before = p.Gold;
            Assert.That(p.TryUpgradeStat(CharacterClass.Fighter, StatUpgrade.Attack), Is.False);
            Assert.That(p.Gold, Is.EqualTo(before));

            // 직업마다 따로
            Assert.That(p.GetClassUpgrades(CharacterClass.Paladin).GetStatLevel(StatUpgrade.Attack), Is.EqualTo(0));

            // 단계 목록은 항목 수만큼 채워짐
            Assert.That(p.GetClassUpgrades(CharacterClass.Fighter).StatLevels.Count, Is.EqualTo(ClassUpgrades.StatCount));

            // 잘못된 항목
            Assert.That(p.TryUpgradeStat(CharacterClass.Fighter, (StatUpgrade)99), Is.False);
        }

        [Test]
        public void TryUpgradeStat_FailsWithoutGold()
        {
            var p = NewProfile();
            p.AddGold(999);
            int changed = 0;
            p.UpgradesChanged += () => ++changed;

            Assert.That(p.TryUpgradeStat(CharacterClass.Fighter, StatUpgrade.Health), Is.False);
            Assert.That(p.Gold, Is.EqualTo(999));
            Assert.That(changed, Is.EqualTo(0));

            p.AddGold(1);
            Assert.That(p.TryUpgradeStat(CharacterClass.Fighter, StatUpgrade.Health), Is.True);
            Assert.That(p.Gold, Is.EqualTo(0));
            Assert.That(changed, Is.EqualTo(1));
        }

        [Test]
        public void TryUpgradeSkill_CostAndMax()
        {
            var p = NewProfile();
            p.AddGold(100000);

            for (int i = 0; i < UpgradeSettings.Default.SkillMaxLevel; ++i)
            {
                Assert.That(p.TryUpgradeSkill(CharacterClass.Paladin, 1), Is.True);
            }
            Assert.That(p.TryUpgradeSkill(CharacterClass.Paladin, 1), Is.False, "최대 3단계");
            Assert.That(p.Gold, Is.EqualTo(100000 - 3 * 5000));

            var up = p.GetClassUpgrades(CharacterClass.Paladin);
            Assert.That(up.SkillLevels, Is.EqualTo(new[] { 0, 3, 0 }));

            Assert.That(p.TryUpgradeSkill(CharacterClass.Paladin, 3), Is.False, "칸은 0~2");
            Assert.That(p.TryUpgradeSkill(CharacterClass.Paladin, -1), Is.False);

            // 저장되어 다시 읽힘
            var again = NewProfile();
            Assert.That(again.GetClassUpgrades(CharacterClass.Paladin).SkillLevels, Is.EqualTo(new[] { 0, 3, 0 }));

            again.ResetUpgrades();
            Assert.That(again.GetAllUpgrades(), Is.Empty);
        }

        [Test]
        public void RelicCapacityUpgrade_LimitsToMax()
        {
            var settings = UpgradeSettings.Default;
            var rule = settings.FindStatRule(StatUpgrade.RelicCapacity);
            Assert.That(rule.MaxLevel, Is.EqualTo(RelicCapacity.Max - RelicCapacity.Base));

            var p = NewProfile();
            p.AddGold(int.MaxValue);
            int bought = 0;
            while (p.TryUpgradeStat(CharacterClass.Engineer, StatUpgrade.RelicCapacity)) ++bought;
            Assert.That(bought, Is.EqualTo(9));
        }

        [Test]
        public void StoredRelics_Rules()
        {
            Assert.That(ProfileService.IsStorableRelic("RLC_Common_001"), Is.True);
            Assert.That(ProfileService.IsStorableRelic("RLC_Fighter_001"), Is.False, "직업 기본 유물");
            Assert.That(ProfileService.IsStorableRelic("RLC_Monster_ELM06"), Is.False, "몬스터 유물");
            Assert.That(ProfileService.IsStorableRelic("RLC_Fighter_002"), Is.True, "업그레이드 유물");
            Assert.That(ProfileService.IsStorableRelic("NoSuchRelic"), Is.False);
            Assert.That(ProfileService.IsStorableRelic(""), Is.False);

            var p = NewProfile();
            int changed = 0;
            p.StorageChanged += () => ++changed;
            Assert.That(p.AddStoredRelic("RLC_Common_001"), Is.True);
            Assert.That(p.AddStoredRelic("RLC_Common_001"), Is.False, "도감: 하나씩");
            Assert.That(p.AddStoredRelic("RLC_Fighter_001"), Is.False);
            Assert.That(p.StoredRelics, Is.EqualTo(new[] { "RLC_Common_001" }));
            Assert.That(p.RemoveStoredRelic("RLC_Common_001"), Is.True);
            Assert.That(p.RemoveStoredRelic("RLC_Common_001"), Is.False);
            Assert.That(changed, Is.EqualTo(2));
        }

        [Test]
        public void OwnedSkills_Rules()
        {
            Assert.That(ProfileService.IsOwnableSkill("fighter_attack"), Is.False, "기본 스킬");
            Assert.That(ProfileService.IsOwnableSkill("slime_strike"), Is.False, "몬스터 스킬");
            Assert.That(ProfileService.IsOwnableSkill("gale_step"), Is.True);

            Assert.That(ProfileService.IsEquippableSkill("gale_step", CharacterClass.Fighter), Is.True);
            Assert.That(ProfileService.IsEquippableSkill("gale_step", CharacterClass.Paladin), Is.False, "다른 직업 Personal");
            Assert.That(ProfileService.IsEquippableSkill("common_adrenaline", CharacterClass.Paladin), Is.True, "Common 은 누구나");
            Assert.That(ProfileService.IsEquippableSkill("evt_blood_pact_strike", CharacterClass.Assassin), Is.True, "Event 는 누구나");

            var p = NewProfile();
            Assert.That(p.AddOwnedSkill("gale_step"), Is.True);
            Assert.That(p.AddOwnedSkill("gale_step"), Is.False);
            Assert.That(p.AddOwnedSkill("aura_of_valor"), Is.True);
            Assert.That(p.AddOwnedSkill("fighter_attack"), Is.False);
            Assert.That(p.GetUsableOwnedSkills(CharacterClass.Fighter), Is.EqualTo(new[] { "gale_step" }));
            Assert.That(p.GetUsableOwnedSkills(CharacterClass.Paladin), Is.EqualTo(new[] { "aura_of_valor" }));
        }

        [Test]
        public void Load_TamperedProfile_RestoresFromBackup()
        {
            var p = NewProfile();
            p.AddGold(1000);

            // 원본만 망가뜨림
            _storage.TryRead(ProfileService.ProfileSlotName, out var data);
            data[data.Length - 1] ^= 0xFF;
            _storage.Write(ProfileService.ProfileSlotName, data);

            var again = NewProfile();
            Assert.That(again.Gold, Is.EqualTo(1000));
            Assert.That(again.ConsumeLoadNotice(), Is.EqualTo("세이브 파일이 손상되어 백업에서 복구했습니다."));
            Assert.That(again.ConsumeLoadNotice(), Is.Empty, "꺼내면 비워짐");
            Assert.That(_storage.TryRead(ProfileService.ProfileTamperedSlotName, out var kept), Is.True);
            Assert.That(kept, Is.EqualTo(data), "조작된 원본 보관");

            // 다시 저장되어 원본이 고쳐짐
            Assert.That(SaveCrypto.LoadFromSlot<ProfileSave>(_storage, _serializer, ProfileService.ProfileSlotName, out var result).Gold, Is.EqualTo(1000));
            Assert.That(result, Is.EqualTo(SaveLoadResult.Ok));
        }

        [Test]
        public void Load_TamperedWithoutBackup_StartsFresh()
        {
            var p = NewProfile();
            p.AddGold(1000);
            _storage.Delete(ProfileService.ProfileBackupSlotName);
            _storage.Write(ProfileService.ProfileSlotName, new byte[] { (byte)'T', (byte)'M', (byte)'S', (byte)'1', 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });

            var again = NewProfile();
            Assert.That(again.Gold, Is.EqualTo(0));
            Assert.That(again.ConsumeLoadNotice(), Is.EqualTo("세이브 파일이 손상되었거나 변조되어 새로 시작합니다."));
        }

        [Test]
        public void Load_LegacyPlainProfile_IsReencrypted_AndDeduped()
        {
            var legacy = new ProfileSave { Gold = 50 };
            legacy.StoredRelics.AddRange(new[] { "RLC_Common_001", "RLC_Common_002", "RLC_Common_001" });
            _storage.Write(ProfileService.ProfileSlotName, _serializer.Serialize(legacy));

            var p = NewProfile();
            Assert.That(p.Gold, Is.EqualTo(50));
            Assert.That(p.StoredRelics, Is.EqualTo(new[] { "RLC_Common_001", "RLC_Common_002" }));

            _storage.TryRead(ProfileService.ProfileSlotName, out var data);
            Assert.That(SaveCrypto.HasMagic(data), Is.True, "다시 암호화해서 씀");
        }
    }
}
