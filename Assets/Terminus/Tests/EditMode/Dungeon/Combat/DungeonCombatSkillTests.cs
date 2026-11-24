// 던전 전투: 스킬 칸 번호 (기본 0~2, 강화 3~5) / 대상 번호 규칙 / 비용
using NUnit.Framework;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    [TestFixture]
    public class DungeonCombatSkillTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        static int Hp(DungeonMonster m) => DungeonCombatMonsters.Hp(m);
        // 밸런스 수치가 바뀌어도 맞도록 데이터에서 읽음
        static int SlimeHp => GameData.FindMonsterRow("mon_slime").BaseHP;

        DungeonCombatKit StartFighter(out DungeonPlayer p, params string[] monsters)
        {
            var kit = new DungeonCombatKit(11);
            p = DungeonCombatPlayers.Make(1);
            kit.StartForced(RoomType.MONSTER, monsters.Length > 0 ? monsters : new[] { "mon_slime", "mon_slime" }, p);
            return kit;
        }

        [Test]
        public void BasicSkillIndices_AttackDefendSpecial()
        {
            var kit = StartFighter(out var p);
            var m0 = kit.Combat.Monsters[0];
            var m1 = kit.Combat.Monsters[1];

            // 0 = 공격 (3 + 공격 0) x 2타, 적 1명 = Monsters 인덱스
            kit.Combat.HandleUseSkill(p, 0, 1);
            Assert.AreEqual(SlimeHp, Hp(m0));
            Assert.AreEqual(SlimeHp - 6, Hp(m1));
            Assert.AreEqual(2, p.Battler.State.Energy);

            // 1 = 방어 (6 + 방어 0)
            kit.Combat.HandleUseSkill(p, 1, -1);
            Assert.AreEqual(6, p.Battler.State.Shield);

            // 2 = 특수 (보호막 3 + 반격)
            kit.Combat.HandleUseSkill(p, 2, -1);
            Assert.AreEqual(9, p.Battler.State.Shield);
            Assert.IsTrue(p.Battler.HasStatus(StatusEffect.Counter));
            Assert.AreEqual(0, p.Battler.State.Energy);

            // 에너지 부족 -> 무시
            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(SlimeHp, Hp(m0));
        }

        [Test]
        public void EnhanceSkillIndices_NeedSkillEnergy_EmptySlotsIgnored()
        {
            var kit = new DungeonCombatKit(11);
            var p = DungeonCombatPlayers.Make(1);
            p.Run.ModifyRunState(s => s.EnhanceSkills.Add("gale_triple_kick"));
            kit.StartForced(RoomType.MONSTER, new[] { "mon_redslime" }, p);
            var m = kit.Combat.Monsters[0];
            int hp = Hp(m);

            // 스킬 에너지 0 -> 못 씀 (에너지도 안 빠짐)
            kit.Combat.HandleUseSkill(p, 3, 0);
            Assert.AreEqual(hp, Hp(m));
            Assert.AreEqual(3, p.Battler.State.Energy);

            // 에너지 2 를 쓰면 스킬 에너지 1
            kit.Combat.HandleUseSkill(p, 1, -1);
            kit.Combat.HandleUseSkill(p, 1, -1);
            Assert.AreEqual(1, p.Battler.State.SkillEnergy);

            // 3 = 첫 강화 칸: 3 x 3타
            kit.Combat.HandleUseSkill(p, 3, 0);
            Assert.AreEqual(hp - 9, Hp(m));
            Assert.AreEqual(0, p.Battler.State.SkillEnergy);
            Assert.AreEqual(1, p.Battler.State.Energy, "강화 스킬은 에너지를 안 씀");

            // 빈 강화 칸 / 범위 밖은 무시
            int energy = p.Battler.State.Energy;
            foreach (int index in new[] { 4, 5, 6, -1 })
            {
                kit.Combat.HandleUseSkill(p, index, 0);
                Assert.AreEqual(energy, p.Battler.State.Energy, $"칸 {index}");
                Assert.AreEqual(hp - 9, Hp(m));
            }
        }

        [Test]
        public void TrainingAndPickupUpgrades_OnlyOnCopy()
        {
            var kit = new DungeonCombatKit(11);
            var p = DungeonCombatPlayers.Make(1);
            p.Run.ModifyRunState(s =>
            {
                s.BasicSkillLevels = new System.Collections.Generic.List<int> { 2, 0, 0 };
                s.EnhanceSkills.Add("gale_triple_kick");
                s.PickupSkillLevels.Add(new SkillLevelEntry { Skill = "gale_triple_kick", Level = 1 });
                s.Stats.StartSkillEnergy = 1;
            });
            var stats = p.Run.RunState.Stats.Clone();
            p.Battler.InitFrom(stats);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_redslime", "mon_redslime" }, p);
            var m0 = kit.Combat.Monsters[0];
            var m1 = kit.Combat.Monsters[1];
            int hp = Hp(m0);

            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(hp - 10, Hp(m0), "훈련소 2단계: (3 + 2) x 2타");

            kit.Combat.HandleUseSkill(p, 3, 1);
            Assert.AreEqual(hp - 12, Hp(m1), "픽업 1단계: (3 + 1) x 3타");

            Assert.AreEqual(3, GameData.FindSkillRow("fighter_attack").BaseValue, "표 원본은 그대로");
            Assert.AreEqual(3, GameData.FindSkillRow("gale_triple_kick").BaseValue);
        }

        [Test]
        public void SingleEnemyTarget_InvalidOrDead_CancelsWithoutCost()
        {
            var kit = StartFighter(out var p);
            var m0 = kit.Combat.Monsters[0];
            DungeonCombatMonsters.Kill(m0);

            foreach (int target in new[] { -1, 2, 99, 0 })
            {
                kit.Combat.HandleUseSkill(p, 0, target);
                Assert.AreEqual(3, p.Battler.State.Energy, $"대상 {target}");
            }
            kit.Combat.HandleUseSkill(p, 0, 1);
            Assert.AreEqual(2, p.Battler.State.Energy);
        }

        [Test]
        public void AllEnemies_HitsAliveMonsters()
        {
            var kit = new DungeonCombatKit(11);
            var p = DungeonCombatPlayers.Make(1, CharacterClass.Assassin);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slime", "mon_slime" }, p);
            DungeonCombatMonsters.Kill(kit.Combat.Monsters[1]);

            // 2 = 암살자 특수: 보호막 + 적 전체 중독 (대상 번호는 안 봄)
            kit.Combat.HandleUseSkill(p, 2, 99);
            Assert.IsTrue(kit.Combat.Monsters[0].Stats.HasStatus(StatusEffect.Poison));
            Assert.IsTrue(kit.Combat.Monsters[2].Stats.HasStatus(StatusEffect.Poison));
            Assert.AreEqual(2, p.Battler.State.Shield);
        }

        [Test]
        public void SingleAllyTarget_IsPlayersIndex()
        {
            var kit = new DungeonCombatKit(11);
            var paladin = DungeonCombatPlayers.Make(1, CharacterClass.Paladin);
            var fighter = DungeonCombatPlayers.Make(2);
            var third = DungeonCombatPlayers.Make(3);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime" }, paladin, fighter, third);
            fighter.Battler.ApplyDamage(10);
            third.Battler.ApplyDamage(10);

            // 2 = 성기사 특수: 아군 1명 회복 (2 + 방어 0). 대상 = Players 인덱스
            kit.Combat.HandleUseSkill(paladin, 2, 1);
            Assert.AreEqual(fighter.Battler.Stats.MaxHealth - 8, DungeonCombatPlayers.Hp(fighter));
            Assert.AreEqual(third.Battler.Stats.MaxHealth - 10, DungeonCombatPlayers.Hp(third));

            // 쓰러진 아군 / 범위 밖은 취소
            DungeonCombatPlayers.Kill(third);
            int energy = paladin.Battler.State.Energy;
            kit.Combat.HandleUseSkill(paladin, 2, 2);
            kit.Combat.HandleUseSkill(paladin, 2, 3);
            Assert.AreEqual(energy, paladin.Battler.State.Energy);
        }

        [Test]
        public void Acid_RaisesEnergyCost()
        {
            var kit = StartFighter(out var p);
            p.Battler.ApplyStatus(StatusEffect.Acid, 1, 2);
            kit.Combat.HandleUseSkill(p, 0, 0);
            Assert.AreEqual(1, p.Battler.State.Energy, "과욕: 비용 +1");
        }

        [Test]
        public void Outsider_RequestsIgnored()
        {
            var kit = StartFighter(out var p);
            var outsider = DungeonCombatPlayers.Make(9);
            kit.Combat.HandleUseSkill(outsider, 0, 0);
            kit.Combat.HandleEndTurn(outsider);
            Assert.AreEqual(SlimeHp, Hp(kit.Combat.Monsters[0]));
            Assert.IsFalse(kit.Combat.HasEndedTurn(outsider));
            Assert.AreEqual(3, outsider.Battler.State.Energy);
        }

        [Test]
        public void SkillUsed_ReportsRowAndTargets()
        {
            var kit = StartFighter(out var p);
            string row = null;
            System.Collections.Generic.List<int> monsters = null;
            kit.Combat.SkillUsed += (r, m, pl) => { row = r; monsters = m; };

            kit.Combat.HandleUseSkill(p, 0, 1);
            Assert.AreEqual(p.Run.GetCombatSkillRow(0), row);
            CollectionAssert.AreEqual(new[] { kit.Combat.Monsters[1].Id }, monsters);
        }

        [Test]
        public void MultiHit_OneHitPerInterval()
        {
            var kit = new DungeonCombatKit(11);
            kit.Settings.HitInterval = 0.5f;
            var p = DungeonCombatPlayers.Make(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slime" }, p);
            var m1 = kit.Combat.Monsters[1];
            int effects = 0;
            kit.Combat.SkillUsed += (r, m, pl) => ++effects;

            // 기본 공격 = 3 x 2타: 첫 타는 바로, 둘째 타는 0.5초 뒤 (그 사이엔 다른 요청 안 받음)
            kit.Combat.HandleUseSkill(p, 0, 1);
            Assert.AreEqual(SlimeHp - 3, Hp(m1));
            Assert.AreEqual(1, effects);
            Assert.IsTrue(kit.Combat.IsResolvingHits);
            kit.Combat.HandleEndTurn(p);
            Assert.IsFalse(kit.Combat.HasEndedTurn(p));

            kit.Combat.Tick(0.4);
            Assert.AreEqual(SlimeHp - 3, Hp(m1));
            kit.Combat.Tick(0.5);
            Assert.AreEqual(SlimeHp - 6, Hp(m1));
            Assert.AreEqual(2, effects);
            Assert.IsFalse(kit.Combat.IsResolvingHits);
        }

        [Test]
        public void MultiHit_UsesEffectLength()
        {
            var kit = new DungeonCombatKit(11);
            kit.Settings.HitInterval = 0.5f;
            var p = DungeonCombatPlayers.Make(1);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_slime" }, p);
            var m1 = kit.Combat.Monsters[1];
            kit.Combat.HitIntervalFor = row => 1.0f;   // 이펙트 1초

            kit.Combat.HandleUseSkill(p, 0, 1);
            kit.Combat.Tick(0.6);
            Assert.AreEqual(SlimeHp - 3, Hp(m1), "기본 간격(0.5)이 아니라 이펙트 길이(1.0)를 기다림");
            kit.Combat.Tick(1.0);
            Assert.AreEqual(SlimeHp - 6, Hp(m1));
        }

        [Test]
        public void DebugCycleSkill_EquipsNextClassSkillAndRefills()
        {
            var kit = StartFighter(out var p);
            kit.Combat.HandleUseSkill(p, 0, 0);   // 에너지 1 씀
            var first = kit.Combat.DebugCycleSkill(p, 1);
            var second = kit.Combat.DebugCycleSkill(p, 1);
            Assert.AreNotEqual("", first);
            Assert.AreNotEqual(first, second);
            Assert.AreEqual(second, p.Run.GetCombatSkillRow(3));
            Assert.IsTrue(Terminus.Run.ProfileService.IsEquippableSkill(second, p.Run.CharacterClass));
            Assert.AreEqual(p.Battler.Stats.MaxEnergy, p.Battler.State.Energy);
            Assert.AreEqual(first, kit.Combat.DebugCycleSkill(p, -1));
        }

        [Test]
        public void CounterHit_ReportsAttacker()
        {
            var kit = StartFighter(out var p);
            kit.Combat.Monsters[0].Stats.ApplyStatus(StatusEffect.Counter, 2, 2);
            System.Collections.Generic.List<ulong> hitPlayers = null;
            kit.Combat.CounterHit += (m, pl) => hitPlayers = pl;

            kit.Combat.HandleUseSkill(p, 0, 0);
            CollectionAssert.AreEqual(new[] { p.Id }, hitPlayers);
        }
    }
}
