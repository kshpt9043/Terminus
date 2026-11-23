// 던전 전투: 유물 발동 시점 (실제 DT_Relic 유물로)
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatRelicTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static int Hp(DungeonMonster m) => DungeonCombatMonsters.Hp(m);

        static DungeonCombatKit Start(RoomType type, string[] monsters, params DungeonPlayer[] players)
        {
            var kit = new DungeonCombatKit(21);
            kit.StartForced(type, monsters, players);
            return kit;
        }

        static readonly string[] TwoSlimes = { "mon_slime", "mon_slime" };

        // ------------------------------------------------------------ 전투 시작

        [Test]
        public void OnBattleStart_AfterEnergyRefill_AndOnlyFirstCycle()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_003", "RLC_Common_001");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);

            Assert.AreEqual(4, p.Battler.State.Energy, "보조 배터리: 3 -> 4");
            Assert.AreEqual(1, p.Battler.GetStatusValue(StatusEffect.Brave), "전사의 증표: 용기 1 (전투 끝까지)");

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(25 - 8, Hp(kit.Combat.Monsters[0]), "(3 + 용기 1) x 2타");

            kit.EndTurnAll();
            kit.RunMonsterTurn();
            Assert.AreEqual(2, kit.Combat.Cycle);
            Assert.AreEqual(3, p.Battler.State.Energy, "다음 사이클엔 다시 안 붙음");
            Assert.AreEqual(1, p.Battler.GetStatusValue(StatusEffect.Brave));
        }

        [Test]
        public void OnBattleStart_GrenadeHitsAll_CanWinImmediately()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_005");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            Assert.AreEqual(20, Hp(kit.Combat.Monsters[0]), "유탄 발사기 5");
            Assert.AreEqual(20, Hp(kit.Combat.Monsters[1]));

            var kit2 = new DungeonCombatKit(21);
            kit2.Combat.SetNextSpawnHealthScale(0.1f);   // 체력 3
            kit2.StartForced(RoomType.MONSTER, TwoSlimes, DungeonCombatPlayers.Make(2, CharacterClass.Fighter, false, "RLC_Common_005"));
            Assert.AreEqual(CombatPhase.Victory, kit2.Combat.Phase, "시작하자마자 전멸");
            Assert.AreEqual(2, kit2.HideRequested.Count);
        }

        [Test]
        public void OnGuardianAndBossBattleStart_OnlyInThoseRooms()
        {
            var guardianRelic = "RLC_Common_017";   // 가디언 전투 시작: 용기 2
            var bossRelic = "RLC_Common_018";       // 보스 전투 시작: 용기 2

            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, guardianRelic, bossRelic);
            Start(RoomType.MONSTER, TwoSlimes, p);
            Assert.AreEqual(0, p.Battler.GetStatusValue(StatusEffect.Brave));

            p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, guardianRelic, bossRelic);
            Start(RoomType.GUARDIAN, TwoSlimes, p);
            Assert.AreEqual(2, p.Battler.GetStatusValue(StatusEffect.Brave));

            p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, guardianRelic, bossRelic);
            Start(RoomType.BOSS, TwoSlimes, p);
            Assert.AreEqual(2, p.Battler.GetStatusValue(StatusEffect.Brave));
        }

        [Test]
        public void MonsterRelic_OnBattleStart_DebuffsPlayers()
        {
            var p = DungeonCombatPlayers.Make(1);
            Start(RoomType.MONSTER, new[] { "mon_lizardpowwow" }, p);
            Assert.IsTrue(p.Battler.HasStatus(StatusEffect.Fear), "저주 인형: 적 전체 공포");
        }

        // ------------------------------------------------------------ 턴

        [Test]
        public void OnTurnStart_EveryCycle_OnTurnEnd_AtEndTurn()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_203", "RLC_Common_206");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            Assert.AreEqual(4, p.Battler.State.Energy, "영구 마력 노심: 턴 시작 에너지 +1");

            p.Battler.ApplyDamage(10);
            int hp = DungeonCombatPlayers.Hp(p);
            kit.Combat.HandleEndTurn(p);
            Assert.AreEqual(hp + 3, DungeonCombatPlayers.Hp(p), "불사조의 생명 영약: 턴 끝 회복 3");

            kit.RunMonsterTurn();
            Assert.AreEqual(4, p.Battler.State.Energy);
        }

        // ------------------------------------------------------------ 맞힘 / 맞음

        [Test]
        public void OnHitEnemy_PerHit_NotForSecondaryAoe()
        {
            var fighter = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_004");
            var kit = Start(RoomType.MONSTER, TwoSlimes, fighter);
            kit.Combat.HandleUseSkill(fighter, 0, 0);
            Assert.AreEqual(4, fighter.Battler.State.Shield, "방어 보조 모듈: 2타 x 보호막 2");

            var engineer = DungeonCombatPlayers.Make(2, CharacterClass.Engineer, false, "RLC_Common_004");
            kit = Start(RoomType.MONSTER, TwoSlimes, engineer);
            kit.Combat.HandleUseSkill(engineer, 0, 0);
            Assert.AreEqual(25 - 5 - 2, Hp(kit.Combat.Monsters[0]), "주 대상: 5 + 광역 2");
            Assert.AreEqual(25 - 2, Hp(kit.Combat.Monsters[1]));
            Assert.AreEqual(2, engineer.Battler.State.Shield, "광역 보조 피해는 타격으로 안 침");
        }

        [Test]
        public void MonsterRelic_OnTakeDamage_ShieldsAfterHealthLoss()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = Start(RoomType.MONSTER, new[] { "mon_mudslime" }, p);
            var mud = kit.Combat.Monsters[0];
            int hp = Hp(mud);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(hp - 3, Hp(mud), "단단한 진흙: 첫 타에 체력이 깎이면 보호막 3 -> 둘째 타는 보호막이 받음");
            Assert.AreEqual(0, mud.Stats.State.Shield);
        }

        [Test]
        public void OnGainShield_PaladinBasicRelicHeals()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Paladin, true);
            CollectionAssert.Contains(p.Run.Relics, "RLC_Paladin_001");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            p.Battler.ApplyDamage(10);

            kit.Combat.HandleUseSkill(p, 1, -1);
            Assert.AreEqual(6, p.Battler.State.Shield);
            Assert.AreEqual(p.Battler.Stats.MaxHealth - 8, DungeonCombatPlayers.Hp(p), "빛의 석상: 보호막 얻으면 회복 2");
        }

        [Test]
        public void OnSkillUsed_MatchesSkillType()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Fighter_009", "RLC_Assassin_004");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.IsFalse(p.Battler.HasStatus(StatusEffect.Evasion), "공격 스킬엔 안 걸림");
            Assert.AreEqual(25, Hp(kit.Combat.Monsters[1]));

            kit.Combat.HandleUseSkill(p, 2, -1);
            Assert.IsTrue(p.Battler.HasStatus(StatusEffect.Evasion), "질풍의 경갑: 특수 스킬 뒤 민첩");
            Assert.AreEqual(25 - 3, Hp(kit.Combat.Monsters[1]), "비도 암기낭: 특수 스킬 뒤 적 전체 3");
        }

        [Test]
        public void OnUseEnergy_And_OnUseSkillEnergy()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_109", "RLC_Surface_Q018");
            p.Run.ModifyRunState(s =>
            {
                s.EnhanceSkills.Add("gale_breathing");
                s.Stats.StartSkillEnergy = 1;
            });
            p.Battler.InitFrom(p.Run.RunState.Stats.Clone());
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(2, p.Battler.State.Shield, "고효율 에너지 코어: 에너지 쓰면 보호막 2");

            kit.Combat.HandleUseSkill(p, 3, -1);
            Assert.AreEqual(2 + 3, p.Battler.State.Shield, "마탑의 기록 수정구: 스킬 에너지 쓰면 보호막 3");
        }

        [Test]
        public void OnHealHP_ChainStopsAtDepthLimit()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Paladin, false, "RLC_Deep_Q005");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            p.Battler.ApplyDamage(50);

            kit.Combat.HandleUseSkill(p, 2, 0);
            // 스킬 회복 2 + 유물 회복 2 가 깊이 8 까지 연쇄
            Assert.AreEqual(p.Battler.Stats.MaxHealth - 50 + 2 + 2 * DungeonCombat.MaxRelicDepth, DungeonCombatPlayers.Hp(p));
        }

        [Test]
        public void OnHitShield_FighterBasicBonusDamage_AndLavaRelic()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, true, "RLC_Fighter_005");
            CollectionAssert.Contains(p.Run.Relics, "RLC_Fighter_001");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            var m0 = kit.Combat.Monsters[0];
            m0.Stats.AddShield(10);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(25, Hp(m0));
            Assert.AreEqual(10 - 4 - 4, m0.Stats.State.Shield, "파쇄의 권갑: 보호막 대상 +20% (3 -> 4)");
            Assert.IsTrue(kit.Combat.Monsters[1].Stats.HasStatus(StatusEffect.Lava), "강철 너클: 보호막 대상을 치면 적 전체 용암");
        }

        // ------------------------------------------------------------ 끝 / 죽음

        [Test]
        public void OnBattleEnd_MoneyAndMaxHp()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_008", "RLC_Common_119");
            int maxHp = p.Battler.Stats.MaxHealth;
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);

            kit.Combat.DebugKillAllMonsters();
            Assert.AreEqual(CombatPhase.Victory, kit.Combat.Phase);
            Assert.AreEqual(10, p.Run.RunState.Currency, "희미한 행운의 부적");
            Assert.AreEqual(maxHp + 1, p.Run.RunState.Stats.MaxHealth, "미약한 신혈: 런 스텟");
            Assert.AreEqual(maxHp + 1, p.Battler.Stats.MaxHealth, "미약한 신혈: 배틀러");
        }

        [Test]
        public void OnDead_RevivalRelic_UsesLimited()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_219");
            var p2 = DungeonCombatPlayers.Make(2);
            Start(RoomType.MONSTER, TwoSlimes, p, p2);

            p.Battler.ApplyDamage(1000);
            Assert.IsFalse(p.IsDead(), "불멸자의 윤회석: 부활");
            Assert.AreEqual(p.Battler.Stats.MaxHealth, DungeonCombatPlayers.Hp(p));

            p.Battler.ApplyDamage(1000);
            Assert.IsTrue(p.IsDead(), "한 번뿐");
        }

        [Test]
        public void MonsterRevival_AncientSlimeCore_ThreeTimes()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = Start(RoomType.GUARDIAN, new[] { "mon_ancientslime" }, p);
            var m = kit.Combat.Monsters[0];
            for (int i = 0; i < 3; ++i)
            {
                m.Stats.ApplyDamage(1000);
                Assert.IsFalse(m.IsDead(), $"부활 {i + 1}");
            }
            m.Stats.ApplyDamage(1000);
            Assert.IsTrue(m.IsDead());
        }

        [Test]
        public void KingsMajesty_IndomitableUntilAllyDies()
        {
            var p = DungeonCombatPlayers.Make(1);
            var kit = Start(RoomType.BOSS, new[] { "mon_slimeking", "mon_slime" }, p);
            var king = kit.Combat.Monsters[0];
            var slime = kit.Combat.Monsters[1];
            int hp = Hp(king);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(hp - 2, Hp(king), "왕의 위엄: 한 대 1 고정");

            DungeonCombatMonsters.Kill(slime);
            Assert.IsFalse(king.Stats.HasStatus(StatusEffect.Indomitable), "아군이 쓰러지면 해제");
            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(hp - 2 - 6, Hp(king));
        }

        [Test]
        public void EvasionDamageReduction_OnlyWhileHolderBuilt()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Assassin, true);
            CollectionAssert.Contains(p.Run.Relics, "RLC_Assassin_001");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);

            int hp = DungeonCombatPlayers.Hp(p);
            p.Battler.ApplyDamage(7);
            Assert.AreEqual(hp - 3, DungeonCombatPlayers.Hp(p), "체술: 회피 4 만큼 감소");

            kit.Combat.EndCombat();
            hp = DungeonCombatPlayers.Hp(p);
            p.Battler.ApplyDamage(7);
            Assert.AreEqual(hp - 7, DungeonCombatPlayers.Hp(p), "전투가 끝나면 꺼짐");
        }

        [Test]
        public void EndCombat_UnhooksRelicEvents()
        {
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Fighter, false, "RLC_Common_109");
            var kit = Start(RoomType.MONSTER, TwoSlimes, p);
            kit.Combat.EndCombat();

            p.Battler.RefillEnergy();
            p.Battler.SpendEnergy(1);
            Assert.AreEqual(0, p.Battler.State.Shield, "전투 밖에서는 유물이 안 걸림");
            Assert.IsNull(p.Battler.PreventDeath);
        }
    }
}
