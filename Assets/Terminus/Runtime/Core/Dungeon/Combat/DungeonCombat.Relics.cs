// 던전 전투: 유물 발동 (언리얼 DungeonCombatComponent.cpp 의 '유물')
// 전투에 참가한 쪽(플레이어 / 몬스터)마다 보유 유물을 들고, 전투 사건이 오면 발동 시점이 맞는 것을 실행
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonCombat
    {
        sealed class RelicHolder
        {
            public CombatStats Stats;
            public DungeonPlayer Player;   // 플레이어면 (던전 재화 / 최대 체력 같은 런 효과용)
            public bool bMonster = false;
            public List<string> Relics = new List<string>();
            public Dictionary<string, int> UsesLeft = new Dictionary<string, int>();   // 부활 같은 횟수 제한
        }

        readonly List<RelicHolder> _holders = new List<RelicHolder>();

        // 유물 효과가 또 유물을 부르는 깊이 (무한 반복 방지)
        int _relicDepth = 0;

        // 언리얼 약한 포인터 자리: 나간 플레이어의 홀더는 없는 셈
        static bool IsHolderValid(RelicHolder holder)
        {
            return holder.Stats != null && (holder.Player == null || holder.Player.bConnected);
        }

        int AddRelicHolder(CombatStats stats, DungeonPlayer player, bool bMonster, IEnumerable<string> relics)
        {
            if (stats == null) return -1;

            var holder = new RelicHolder { Stats = stats, Player = player, bMonster = bMonster };
            int index = _holders.Count;
            _holders.Add(holder);

            bool bEvasionReduction = false;
            int shieldBonusPercent = 0;
            if (relics != null)
            {
                foreach (var row in relics)
                {
                    var relic = GameData.FindRelicRow(row);
                    if (relic == null) continue;

                    holder.Relics.Add(row);
                    bEvasionReduction |= relic.ActionKind == ActionKind.EvasionDamageReduction;
                    if (relic.ActionKind == ActionKind.BonusDamageOnShield) shieldBonusPercent += relic.BaseValue;
                }
            }
            // 체술 / 보호막 관통은 발동형이 아니라 맞을 때마다 피해 계산에 끼어드는 거라 스텟에 켜 둠
            stats.SetEvasionReduction(bEvasionReduction);
            stats.SetShieldBonusPercent(shieldBonusPercent);

            stats.OnDamaged += HandleStatsDamaged;
            stats.OnShieldGained += HandleShieldGained;
            stats.OnHitShieldTarget += HandleHitShield;
            stats.OnHealed += HandleHealed;
            stats.OnEnergySpent += HandleEnergySpent;
            stats.OnSkillEnergySpent += HandleSkillEnergySpent;
            stats.OnDied += HandleStatsDied;
            stats.PreventDeath = HandlePreventDeath;
            return index;
        }

        void BuildRelicHolders()
        {
            ClearRelicHolders();

            // 플레이어: 런 보유 유물 (직업 기본 유물 포함)
            foreach (var p in _players)
            {
                if (IsValid(p))
                {
                    AddRelicHolder(GetStats(p), p, false, p.Run != null ? p.Run.Relics : null);
                }
            }

            // 몬스터: DT_Monster 의 PassiveID (세미콜론으로 여러 개). 유물 행 이름이 아닌 건 건너뜀
            foreach (var monster in _monsters)
            {
                AddRelicHolder(monster?.Stats, null, true, DungeonMonster.GetPassiveRelics(monster?.Row));
            }
        }

        void ClearRelicHolders()
        {
            foreach (var holder in _holders)
            {
                var stats = holder.Stats;
                if (stats == null) continue;

                stats.OnDamaged -= HandleStatsDamaged;
                stats.OnShieldGained -= HandleShieldGained;
                stats.OnHitShieldTarget -= HandleHitShield;
                stats.OnHealed -= HandleHealed;
                stats.OnEnergySpent -= HandleEnergySpent;
                stats.OnSkillEnergySpent -= HandleSkillEnergySpent;
                stats.OnDied -= HandleStatsDied;
                stats.PreventDeath = null;
                stats.SetEvasionReduction(false);
                stats.SetShieldBonusPercent(0);
            }
            _holders.Clear();
            _relicDepth = 0;
        }

        int FindHolder(CombatStats stats)
        {
            if (stats == null) return -1;
            return _holders.FindIndex(h => h.Stats == stats);
        }

        List<CombatStats> GetSideStats(bool bMonsterSide)
        {
            return bMonsterSide ? GetAliveMonsterStats() : GetAlivePlayerStats();
        }

        void FireRelicsForAll(RelicTrigger trigger, bool bPlayers, bool bMonsters)
        {
            for (int i = 0; i < _holders.Count; ++i)
            {
                if ((_holders[i].bMonster && bMonsters) || (!_holders[i].bMonster && bPlayers))
                {
                    FireRelics(i, trigger);
                }
            }
        }

        // 그 쪽의 유물 중 Trigger 가 맞는 것 실행. Amount = 사건 수치 (깎인 체력 등), UsedSkill = OnSkillUsed 판정용
        void FireRelics(int holderIndex, RelicTrigger trigger, int amount = 0, string usedSkill = "", SkillRow usedSkillRow = null)
        {
            if (holderIndex < 0 || holderIndex >= _holders.Count) return;

            // 유물 -> 사건 -> 유물 ... 이 끝없이 이어지지 않게
            if (_relicDepth >= MaxRelicDepth)
            {
                LogWarning?.Invoke($"[Relic] 연쇄 발동이 너무 깊어 멈춤 ({trigger})");
                return;
            }

            var holder = _holders[holderIndex];
            if (!IsHolderValid(holder) || holder.Stats.IsDead()) return;

            var relics = new List<string>(holder.Relics);   // 실행 중 목록이 바뀌어도 안전하게 복사
            foreach (var row in relics)
            {
                var relic = GameData.FindRelicRow(row);
                if (relic == null || relic.TriggerTiming != trigger) continue;

                // OnSkillUsed: OnSkill 칸이 쓴 스킬의 행 이름 / SkillID / 스킬 종류(Attack, Defense, Special) 와 같을 때만
                if (trigger == RelicTrigger.OnSkillUsed)
                {
                    string onSkill = relic.OnSkill;
                    bool bMatch = !Ids.IsNone(onSkill)
                        && (onSkill == usedSkill
                            || (usedSkillRow != null && (onSkill == usedSkillRow.SkillID
                                || onSkill == usedSkillRow.SkillType.ToString())));
                    if (!bMatch) continue;
                }

                LogInfo?.Invoke($"[Relic] {relic.RelicName} 발동 ({trigger})");

                ++_relicDepth;
                try
                {
                    ExecuteRelic(holderIndex, relic, amount);
                }
                finally
                {
                    --_relicDepth;
                }
            }
        }

        void ExecuteRelic(int holderIndex, RelicRow relic, int amount)
        {
            if (holderIndex < 0 || holderIndex >= _holders.Count) return;
            var holder = _holders[holderIndex];
            var owner = holder.Stats;
            var player = holder.Player;
            bool bMonster = holder.bMonster;
            if (!IsHolderValid(holder) || owner.IsDead()) return;

            switch (relic.ActionKind)
            {
                // 부활은 죽는 순간(HandlePreventDeath)에서, 체술 / 보호막 관통은 피해 계산(ApplyDamage)에서
                case ActionKind.RevivalSelfAll:
                case ActionKind.RevivalSelfHalf:
                case ActionKind.EvasionDamageReduction:
                case ActionKind.BonusDamageOnShield:
                    return;

                // 런 전체에 남는 효과 (플레이어만)
                case ActionKind.GainMoney:
                case ActionKind.MaxHPUp:
                case ActionKind.MaxHPDown:
                case ActionKind.RandomUpgrade:
                    player?.Run?.ApplyRelicMetaEffect(relic);
                    return;

                // 에너지는 최대치를 넘어도 됨 (보조 배터리: 3 -> 4)
                case ActionKind.GainEnergy:
                    owner.AddEnergy(relic.BaseValue, true);
                    return;

                // 아군이 죽으면 풀리는 불굴 (왕의 위엄). 풀어 주는 건 HandleStatsDied
                case ActionKind.IndomitableSelfAllyDead:
                    owner.ApplyStatus(StatusEffect.Indomitable, relic.StatusValue, relic.StatusDuration != 0 ? relic.StatusDuration : -1);
                    return;
            }

            // 나머지는 스킬과 같은 실행기로. 유물 칸을 스킬 행 모양으로 옮겨 담음
            var asSkill = new SkillRow
            {
                RowName = "",
                DisplayName_KR = relic.RelicName,
                ActionKind = relic.ActionKind,
                TargetType = relic.TargetType,
                BaseValue = relic.BaseValue,
                ScalingStat = relic.ScalingStat,
                ScalingRatio = relic.ScalingRatio,
                HitCount = 1,
                StatusEffect = relic.StatusEffect,
                StatusValue = relic.StatusValue,
                StatusDuration = relic.StatusDuration,
                SkillType = SkillType.Special,   // 유물 효과는 공격 스킬이 아님 (OnSkillUsed 'Attack' 유물이 다시 안 걸리게)
            };

            // 대상: 유물 주인 기준 (몬스터 유물이면 적 = 플레이어). 배신 전투면 적 = 상대 편 플레이어
            bool bBetrayalSide = IsBetrayal() && !bMonster;
            bool bMine = player == _betrayer;
            var opponents = bBetrayalSide ? GetBetrayalSideStats(!bMine) : GetSideStats(!bMonster);
            var allies = bBetrayalSide ? GetBetrayalSideStats(bMine) : GetSideStats(bMonster);
            var targets = new List<CombatStats>();
            switch (relic.TargetType)
            {
                case TargetType.Self: targets.Add(owner); break;
                case TargetType.AllEnemies: targets.AddRange(opponents); break;
                case TargetType.AllAllies: targets.AddRange(allies); break;
                case TargetType.SingleEnemy: if (opponents.Count > 0) targets.Add(opponents[RandRange(0, opponents.Count - 1)]); break;
                case TargetType.SingleAlly: if (allies.Count > 0) targets.Add(allies[RandRange(0, allies.Count - 1)]); break;
            }
            if (targets.Count == 0) return;

            SkillExecutor.Execute(asSkill, owner, targets, opponents, _rng);
        }

        // ---- 전투 사건 -> 유물

        void HandleStatsDamaged(CombatStats self, int healthLost, CombatStats instigator)
        {
            int victim = FindHolder(self);
            if (victim < 0) return;

            FireRelics(victim, RelicTrigger.OnTakeDamage, healthLost);

            // 때린 쪽 (상대편일 때만 '타격'). 광역 보조 피해는 타격으로 안 침 (엑셀 v1.3: 보호막 중복 획득 차단)
            int attacker = FindHolder(instigator);
            if (attacker >= 0 && _holders[attacker].bMonster != _holders[victim].bMonster && !SkillExecutor.IsSecondaryHit())
            {
                FireRelics(attacker, RelicTrigger.OnHitEnemy, healthLost);
            }

            // 같은 편 다른 사람
            for (int i = 0; i < _holders.Count; ++i)
            {
                if (i != victim && _holders[i].bMonster == _holders[victim].bMonster)
                {
                    FireRelics(i, RelicTrigger.OnAllyTakeDamage, healthLost);
                }
            }
        }

        void HandleShieldGained(CombatStats self, int amount) => FireRelics(FindHolder(self), RelicTrigger.OnGainShield, amount);

        void HandleHitShield(CombatStats self, int amount)
        {
            // 유물이 준 피해로 또 발동하면 "보호막 적에게 추가 피해" 가 연쇄로 이어짐 -> 스킬 / 몬스터 행동에서 맞힌 것만
            if (_relicDepth > 0) return;
            FireRelics(FindHolder(self), RelicTrigger.OnHitShield, amount);
        }

        void HandleHealed(CombatStats self, int amount) => FireRelics(FindHolder(self), RelicTrigger.OnHealHP, amount);

        void HandleEnergySpent(CombatStats self, int amount) => FireRelics(FindHolder(self), RelicTrigger.OnUseEnergy, amount);

        void HandleSkillEnergySpent(CombatStats self, int amount) => FireRelics(FindHolder(self), RelicTrigger.OnUseSkillEnergy, amount);

        void HandleStatsDied(CombatStats self)
        {
            int dead = FindHolder(self);
            if (dead < 0) return;

            // 같은 편이 죽으면 '아군 사망 시 해제' 불굴이 풀림 (왕의 위엄)
            for (int i = 0; i < _holders.Count; ++i)
            {
                if (i == dead || _holders[i].bMonster != _holders[dead].bMonster) continue;

                var ally = _holders[i].Stats;
                if (!IsHolderValid(_holders[i]) || ally.IsDead()) continue;

                foreach (var row in _holders[i].Relics)
                {
                    var relic = GameData.FindRelicRow(row);
                    if (relic != null && relic.ActionKind == ActionKind.IndomitableSelfAllyDead)
                    {
                        ally.RemoveStatus(StatusEffect.Indomitable);
                        LogInfo?.Invoke($"[Relic] 아군이 쓰러져 {relic.RelicName} 해제");
                    }
                }
            }
        }

        bool HandlePreventDeath(CombatStats self)
        {
            int index = FindHolder(self);
            if (index < 0) return false;

            var holder = _holders[index];
            foreach (var row in holder.Relics)
            {
                var relic = GameData.FindRelicRow(row);
                if (relic == null || relic.TriggerTiming != RelicTrigger.OnDead) continue;
                if (relic.ActionKind != ActionKind.RevivalSelfAll && relic.ActionKind != ActionKind.RevivalSelfHalf) continue;

                // 부활 횟수 = BaseValue (Enum_Reference: "BaseValue 만큼 부활")
                if (!holder.UsesLeft.TryGetValue(row, out int left))
                {
                    left = System.Math.Max(1, relic.BaseValue);
                }
                if (left <= 0)
                {
                    holder.UsesLeft[row] = left;
                    continue;
                }
                --left;
                holder.UsesLeft[row] = left;

                self.Revive(relic.ActionKind == ActionKind.RevivalSelfAll ? 1f : 0.5f);
                LogInfo?.Invoke($"[Relic] {relic.RelicName}: 부활 (남은 횟수 {left})");
                return true;
            }
            return false;
        }
    }
}
