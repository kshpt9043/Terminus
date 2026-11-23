// 던전 전투: 배신 전투 (언리얼 DungeonCombatComponent.cpp 의 '배신 전투')
// 배신자 1명 vs 나머지 (사용자 결정 2026-10-08)
//  - 턴: 남은 팀 전원 -> 배신자 -> 사이클 끝 (몬스터 턴 자리에 배신자 턴)
//  - 보정: 팀은 현재 체력의 VictimHealthCut 만큼 잃고, 배신자는 공격 / 방어 +(상대 수 x StatPerOpponent) (용기 / 견고)
//  - 적 = 상대 편 플레이어, 아군 = 같은 편. 대상 인덱스는 구역 Occupants(= Players) 순서
//  - 끝나면 맵 매니저에 결과를 알림 (보상 / 구역 클리어 없음)
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonCombat
    {
        public bool IsBetrayal() => _betrayer != null;
        public DungeonPlayer Betrayer => _betrayer;
        public DungeonPlayer GetBetrayer() => _betrayer;
        public bool IsBetrayerTurn() => _bBetrayerTurn;
        public bool DidBetrayerWin() => _bBetrayerWon;

        // 이 사람이 지금 행동할 차례인가 (플레이어 턴이고, 배신 전투면 그 사람 편 차례)
        public bool IsTurnOf(DungeonPlayer p)
        {
            if (_phase != CombatPhase.PlayerTurn || p == null) return false;
            return !IsBetrayal() || ((p == _betrayer) == _bBetrayerTurn);
        }

        // 같은 편인가 (보통 전투면 플레이어끼리는 늘 같은 편)
        public bool IsSameSide(DungeonPlayer a, DungeonPlayer b)
        {
            return !IsBetrayal() || ((a == _betrayer) == (b == _betrayer));
        }

        List<CombatStats> GetBetrayalSideStats(bool bBetrayerSide)
        {
            var output = new List<CombatStats>();
            foreach (var p in _players)
            {
                if ((p == _betrayer) != bBetrayerSide) continue;
                var stats = GetStats(p);
                if (stats != null && !stats.IsDead()) output.Add(stats);
            }
            return output;
        }

        /// <summary>배신 전투 시작. players = 구역 Occupants 순서 (팀 먼저, 배신자 마지막)</summary>
        public void StartBetrayal(IReadOnlyList<DungeonPlayer> players, DungeonPlayer betrayer, float victimHealthCut, int statPerOpponent)
        {
            if (betrayer == null) return;

            EndCombat();

            if (players != null)
            {
                foreach (var p in players)
                {
                    if (p != null) _players.Add(p);
                }
            }
            _betrayer = betrayer;
            _bBetrayerTurn = false;
            _bBetrayerWon = false;
            _currentRoomType = RoomType.MONSTER;   // 가디언 / 보스 시작 유물은 안 씀
            _currentRoomId = -1;
            _pendingRewards.Clear();

            BuildRelicHolders();

            // 보정: 배신당한 사람은 현재 체력의 일부를 잃고, 배신자는 상대 수만큼 공격 / 방어가 오름 (이번 전투 동안)
            int opponents = 0;
            foreach (var p in _players)
            {
                ApplyTempBuffs(p);
                if (p == _betrayer) continue;
                ++opponents;

                var stats = GetStats(p);
                if (stats == null || stats.IsDead()) continue;
                int health = stats.State.Health;
                int newHealth = Math.Max(1, health - CombatMath.RoundToInt(health * victimHealthCut));
                stats.Revive((float)newHealth / Math.Max(1, stats.Stats.MaxHealth));
            }

            var betrayerStats = GetStats(_betrayer);
            if (betrayerStats != null)
            {
                int bonus = statPerOpponent * opponents;
                if (bonus > 0)
                {
                    betrayerStats.ApplyStatus(StatusEffect.Brave, bonus, -1);   // 공격
                    betrayerStats.ApplyStatus(StatusEffect.Solid, bonus, -1);   // 방어
                }
            }

            LogInfo?.Invoke($"[Combat] 배신 전투 시작: {_betrayer.Name} vs {opponents}명");

            _cycle = 0;
            StartCycle();
        }

        void BeginBetrayerTurn()
        {
            _bBetrayerTurn = true;
            _endedTurn.Clear();

            // 배신자 턴 시작: 보호막 버림, 에너지 최대, 활력, 턴 시작 유물 (팀은 StartCycle 에서 같은 처리)
            var stats = GetStats(_betrayer);
            if (stats != null && !stats.IsDead())
            {
                stats.ClearShield();
                stats.RefillEnergy();
                stats.OnTurnStart();   // 중독
                int vitality = stats.GetStatusValue(StatusEffect.Vitality);
                if (vitality > 0)
                {
                    stats.AddEnergy(vitality, true);
                }
                FireRelics(FindHolder(stats), RelicTrigger.OnTurnStart);
            }

            LogInfo?.Invoke($"[Combat] 사이클 {_cycle} 배신자 턴");
            NotifyChanged();
            CheckCombatEnd();
        }

        void TryAdvanceBetrayalTurn()
        {
            if (!IsBetrayal() || _phase != CombatPhase.PlayerTurn) return;

            // 지금 차례인 편의 살아 있는 사람이 전원 턴 종료했으면 다음 차례
            foreach (var p in _players)
            {
                if ((p == _betrayer) != _bBetrayerTurn) continue;
                var stats = GetStats(p);
                if (stats != null && !stats.IsDead() && !_endedTurn.Contains(p)) return;
            }

            if (!_bBetrayerTurn) BeginBetrayerTurn();
            else FinishCycle();
        }

        /// <summary>배신 전투 중 누가 나감: 배신자면 팀 승리, 팀원이면 그 사람은 쓰러진 걸로</summary>
        public void HandleBetrayalLeaver(DungeonPlayer leaver)
        {
            if (!IsBetrayal() || leaver == null || _phase == CombatPhase.Victory || _phase == CombatPhase.Defeat) return;

            // 나간 사람은 쓰러진 걸로 (인덱스가 Occupants 와 어긋나지 않게 목록에서 빼진 않음)
            // 나가는 중이라 bConnected 가 이미 false 일 수 있어서 배틀러를 직접 씀 (언리얼은 Logout 중이라 아직 유효했음)
            var stats = leaver.Battler;
            if (stats != null)
            {
                stats.PreventDeath = null;
                stats.ClearCombatEffects();
                stats.ApplyDamage(stats.State.Health + stats.State.Shield);
            }
            if (leaver == _betrayer)
            {
                FinishBetrayal(false);
                return;
            }

            NotifyChanged();
            if (CheckCombatEnd()) return;
            TryAdvanceBetrayalTurn();
        }

        void FinishBetrayal(bool bInBetrayerWon)
        {
            _bBetrayerWon = bInBetrayerWon;
            _phase = CombatPhase.Victory;   // HUD 는 편을 보고 승리 / 패배를 표시
            _intents.Clear();

            ClearTimer();

            foreach (var p in _players)
            {
                GetStats(p)?.ClearCombatEffects();
            }

            LogInfo?.Invoke($"[Combat] 배신 전투 끝: {(_bBetrayerWon ? "배신자 승리" : "팀 승리")}");
            NotifyChanged();

            // 결과를 잠깐 보여 준 뒤 맵 매니저가 런을 끝냄 (정산)
            SetTimer(CombatStep.ReportBetrayalResult, _settings.CombatEndDelay);
        }

        void ReportBetrayalResult()
        {
            BetrayalFinished?.Invoke(_betrayer, _bBetrayerWon);
        }
    }
}
