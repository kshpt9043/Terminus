// 정산: 후보 유물 / 판매 골드 / 퀘스트 골드 / 하드 모드 사망 / 완료
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunSettlementTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static ProfileService NewProfile(MemorySaveStorage storage = null)
        {
            var p = new ProfileService(storage ?? new MemorySaveStorage(), new RunXmlTestSerializer(), new Random(9), UpgradeSettings.Default);
            p.Load();
            return p;
        }

        static RunState MakeRun()
        {
            var s = new RunState();
            s.Relics.AddRange(new[] { "RLC_Fighter_001", "RLC_Common_001", "RLC_Common_002", "RLC_Monster_ELM06", "RLC_Common_003" });
            s.StartRelics.Add("RLC_Common_003");
            s.EnhanceSkills.AddRange(new[] { "gale_step", "gale_triple_kick" });
            return s;
        }

        static int SellGold(string relic) => GameData.FindRelicRow(relic).SellPrice_Gold;

        [Test]
        public void MakeSettlement_CandidatesAndPrices()
        {
            var profile = NewProfile();
            var run = MakeRun();
            var s = SettlementRules.MakeFromRun(profile, run, null, "던전을 탈출했습니다.", "테스트 방", 2, false);

            Assert.That(s.bValid, Is.True);
            Assert.That(s.Relics, Is.EqualTo(new[] { "RLC_Common_001", "RLC_Common_002" }), "기본 / 몬스터 / 시작 유물 제외");
            Assert.That(s.RelicGold, Is.EqualTo(new[] { SellGold("RLC_Common_001"), SellGold("RLC_Common_002") }));
            // 유니티 NUnit 3.5 엔 Is.AnyOf 가 없음
            Assert.That(new[] { "gale_step", "gale_triple_kick" }, Does.Contain(s.KeptSkill));
            Assert.That(s.BonusGold, Is.EqualTo(0));
            Assert.That(s.LostStoredRelics, Is.Empty);
            Assert.That(s.Floor, Is.EqualTo(2));
            Assert.That(s.RoomName, Is.EqualTo("테스트 방"));
        }

        [Test]
        public void MakeSettlement_SkipsAlreadyOwnedSkills()
        {
            var profile = NewProfile();
            profile.AddOwnedSkill("gale_step");
            profile.AddOwnedSkill("gale_triple_kick");
            var s = SettlementRules.MakeFromRun(profile, MakeRun(), null, "", "", 1, false);
            Assert.That(s.KeptSkill, Is.Empty, "전부 있으면 건너뜀");
        }

        [Test]
        public void Death_NoSkill_HardModeLosesStartRelics_QuestGoldStillPaid()
        {
            var profile = NewProfile();
            var run = MakeRun();
            run.bHardMode = true;
            run.QuestGold = 400;
            var adjust = new SettlementAdjust { BonusGold = 100, BonusReason = "배신자 유물 분배" };

            var s = SettlementRules.MakeFromRun(profile, run, adjust, "전멸", "", 3, true);
            Assert.That(s.bDeath, Is.True);
            Assert.That(s.KeptSkill, Is.Empty);
            Assert.That(s.Relics.Count, Is.EqualTo(2), "사망도 유물은 보관 / 판매");
            Assert.That(s.LostStoredRelics, Is.EqualTo(new[] { "RLC_Common_003" }));
            Assert.That(s.BonusGold, Is.EqualTo(500));
            Assert.That(s.BonusReason, Is.EqualTo("배신자 유물 분배 · 퀘스트 보상"));

            // 하드 모드가 아니면 창고 유물은 안전
            run.bHardMode = false;
            Assert.That(SettlementRules.MakeFromRun(profile, run, null, "", "", 3, true).LostStoredRelics, Is.Empty);
            Assert.That(SettlementRules.MakeFromRun(profile, run, null, "", "", 3, true).BonusReason, Is.EqualTo("퀘스트 보상"));
        }

        [Test]
        public void BetrayalAdjust_RemovesAndAddsRelics()
        {
            var profile = NewProfile();
            var adjust = new SettlementAdjust();
            adjust.LostRelics.Add("RLC_Common_001");
            adjust.LostSkills.AddRange(new[] { "gale_step", "gale_triple_kick" });
            adjust.ExtraRelics.Add("RLC_Common_005");

            var s = SettlementRules.MakeFromRun(profile, MakeRun(), adjust, "", "", 2, false);
            Assert.That(s.Relics, Is.EqualTo(new[] { "RLC_Common_002", "RLC_Common_005" }));
            Assert.That(s.KeptSkill, Is.Empty);
        }

        [Test]
        public void Confirm_KeepsChosen_SellsRest_AddsBonus()
        {
            var storage = new MemorySaveStorage();
            var profile = NewProfile(storage);
            profile.AddStoredRelic("RLC_Common_002");   // 이미 창고에 있음
            profile.AddStoredRelic("RLC_Common_003");   // 하드 모드 사망으로 잃을 것

            var run = MakeRun();
            run.Relics.Add("RLC_Common_004");
            run.QuestGold = 250;
            run.bHardMode = true;
            var pending = SettlementRules.MakeFromRun(profile, run, null, "", "", 2, true);
            profile.BeginSettlement(pending);
            Assert.That(profile.HasPendingSettlement, Is.True);

            // 다시 켜도 정산 대기가 남아 있음
            profile = NewProfile(storage);
            Assert.That(profile.HasPendingSettlement, Is.True);
            Assert.That(profile.GetPendingSettlement().Relics, Is.EqualTo(new[] { "RLC_Common_001", "RLC_Common_002", "RLC_Common_004" }));

            var kept = new List<string>();
            SettlementRules.ToggleKept(kept, "RLC_Common_001");
            SettlementRules.ToggleKept(kept, "RLC_Common_002");
            SettlementRules.ToggleKept(kept, "RLC_Common_004");
            SettlementRules.ToggleKept(kept, "RLC_Common_004");   // 다시 누르면 판매
            Assert.That(kept, Is.EqualTo(new[] { "RLC_Common_001", "RLC_Common_002" }));
            Assert.That(SettlementRules.AlreadyStored(profile, kept), Is.EqualTo(new[] { "RLC_Common_002" }));
            Assert.That(SettlementRules.SaleGold(profile.GetPendingSettlement(), kept), Is.EqualTo(SellGold("RLC_Common_004")));

            var result = SettlementRules.Confirm(profile, kept);
            Assert.That(result.RelicGold, Is.EqualTo(SellGold("RLC_Common_004")));
            Assert.That(result.BonusGold, Is.EqualTo(250));
            Assert.That(result.NewRelics, Is.EqualTo(new[] { "RLC_Common_001" }));
            Assert.That(result.NewSkill, Is.Empty, "사망");
            Assert.That(result.GoldAfter, Is.EqualTo(SellGold("RLC_Common_004") + 250));

            Assert.That(profile.Gold, Is.EqualTo(result.GoldAfter));
            Assert.That(profile.StoredRelics, Is.EquivalentTo(new[] { "RLC_Common_002", "RLC_Common_001" }));
            Assert.That(profile.HasPendingSettlement, Is.False);
            Assert.That(SettlementRules.Confirm(profile, kept), Is.Null, "두 번 받지 않음");
        }

        [Test]
        public void Confirm_EscapeGivesSkill()
        {
            var profile = NewProfile();
            var run = MakeRun();
            run.EnhanceSkills.RemoveAt(1);
            profile.BeginSettlement(SettlementRules.MakeFromRun(profile, run, null, "", "", 1, false));

            var result = SettlementRules.Confirm(profile, new List<string>());
            Assert.That(result.NewSkill, Is.EqualTo("gale_step"));
            Assert.That(profile.OwnedSkills, Is.EqualTo(new[] { "gale_step" }));
            Assert.That(result.RelicGold, Is.EqualTo(SellGold("RLC_Common_001") + SellGold("RLC_Common_002")), "하나도 안 고르면 전부 판매");
        }
    }
}
