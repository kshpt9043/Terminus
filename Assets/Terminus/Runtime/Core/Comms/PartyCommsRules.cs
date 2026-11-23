// [서버] 소통 도구 판정 (언리얼 ATerminusPlayerController 의 Server_SendPing / Server_SendQuickChat / Server_Nudge
// 와 익명 namespace 도우미 GetAreaRecipients / IsValidCommsTarget / IsCommsAlive)
// (행동 계획 Server_SetCombatPlan 은 전투 쪽 DungeonServer.CombatPlan.cs)
// 구역은 (Occupants, DungeonCombat) 로 받음 -> 게임 쪽은 DungeonArea.Occupants 와 DungeonArea.Combat 를 넘기면 됨
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Dungeon;

namespace Terminus.Comms
{
    public static class PartyCommsRules
    {
        // ---- [서버] 너무 잦은 요청 막기
        public const double PingCooldown = 0.4;
        public const double QuickChatCooldown = 1.0;
        public const double NudgeCooldown = 8.0;

        public static bool IsAlive(DungeonPlayer p) => p != null && p.Battler != null && !p.Battler.IsDead();

        public static bool IsAlive(DungeonMonster m) => m != null && m.Stats != null && !m.Stats.IsDead() && !m.bHideRequested;

        static bool IsFighting(DungeonCombat combat) =>
            combat != null && (combat.Phase == CombatPhase.PlayerTurn || combat.Phase == CombatPhase.MonsterTurn);

        /// <summary>같은 구역에 있는 사람 (보낸 사람 포함). sameSideOnly 면 배신 전투에서 같은 편만</summary>
        public static List<DungeonPlayer> GetAreaRecipients(IReadOnlyList<DungeonPlayer> occupants, DungeonCombat combat, DungeonPlayer sender, bool sameSideOnly)
        {
            var output = new List<DungeonPlayer>();
            if (occupants == null || sender == null) return output;
            foreach (var p in occupants)
            {
                if (p == null || !p.bConnected) continue;
                if (sameSideOnly && combat != null && !combat.IsSameSide(sender, p)) continue;
                output.Add(p);
            }
            return output;
        }

        /// <summary>대상이 보낸 사람 구역의 살아 있는 배틀러인가. wantEnemy = 적(몬스터 / 배신 전투 상대 편) / 아군(나 포함)</summary>
        public static bool IsValidTarget(IReadOnlyList<DungeonPlayer> occupants, DungeonCombat combat, DungeonPlayer sender, CommsTarget target, bool wantEnemy)
        {
            if (combat == null || target == null || target.IsNone || sender == null) return false;

            if (target.IsMonster)
            {
                if (!wantEnemy || combat.IsBetrayal()) return false;
                foreach (var m in combat.Monsters)
                {
                    if (m != null && m.Id == target.MonsterId) return IsAlive(m);
                }
                return false;
            }

            if (occupants == null) return false;
            foreach (var p in occupants)
            {
                if (p != null && p.Id == target.PlayerId)
                {
                    return IsAlive(p) && wantEnemy != combat.IsSameSide(sender, p);
                }
            }
            return false;
        }

        /// <summary>핑을 보낼 수 있나 (언리얼 Server_SendPing 의 판정. 간격은 CommsThrottle)</summary>
        public static bool CanPing(IReadOnlyList<DungeonPlayer> occupants, DungeonCombat combat, DungeonPlayer sender, PingKind kind, CommsTarget target)
        {
            if (!IsFighting(combat) || !PartyComms.IsValidPingKind(kind)) return false;

            // 적 핑은 적에게, 아군 핑은 아군(나 포함)에게만
            return IsValidTarget(occupants, combat, sender, target, PartyComms.IsEnemyPing(kind));
        }

        /// <summary>
        /// 재촉 받을 사람 (언리얼 Server_Nudge). 재촉은 할 게 없는 사람만 (턴을 끝냈거나, 상대 편 차례이거나, 쓰러짐)
        /// 받는 사람 = 같은 구역에서 아직 턴을 안 끝낸 살아 있는 사람. 보낼 수 없으면 빈 목록
        /// </summary>
        public static List<DungeonPlayer> GetNudgeTargets(IReadOnlyList<DungeonPlayer> occupants, DungeonCombat combat, DungeonPlayer sender)
        {
            var output = new List<DungeonPlayer>();
            if (combat == null || sender == null || occupants == null || combat.Phase != CombatPhase.PlayerTurn) return output;
            if (!IsWaiting(combat, sender)) return output;

            foreach (var p in occupants)
            {
                if (p == null || p == sender || !p.bConnected) continue;
                if (!combat.IsTurnOf(p) || combat.HasEndedTurn(p) || !IsAlive(p)) continue;
                output.Add(p);
            }
            return output;
        }

        /// <summary>이 사람은 이번 플레이어 턴에 할 게 없나 (끝냈거나, 상대 편 차례이거나, 쓰러짐)</summary>
        public static bool IsWaiting(DungeonCombat combat, DungeonPlayer p) =>
            combat != null && p != null && (combat.HasEndedTurn(p) || !combat.IsTurnOf(p) || !IsAlive(p));
    }

    /// <summary>[서버] 한 사람의 소통 요청 간격 (언리얼 PC 의 LastPingTime / LastQuickChatTime / LastNudgeTime)</summary>
    public sealed class CommsThrottle
    {
        double _lastPing = -1;
        double _lastQuickChat = -1;
        double _lastNudge = -1;

        public bool TryPing(double now)
        {
            if (_lastPing >= 0 && now - _lastPing < PartyCommsRules.PingCooldown) return false;
            _lastPing = now;
            return true;
        }

        public bool TryQuickChat(double now)
        {
            if (_lastQuickChat >= 0 && now - _lastQuickChat < PartyCommsRules.QuickChatCooldown) return false;
            _lastQuickChat = now;
            return true;
        }

        // 재촉은 실제로 누군가에게 보냈을 때만 시간을 잼
        public bool CanNudge(double now) => _lastNudge < 0 || now - _lastNudge >= PartyCommsRules.NudgeCooldown;

        public void NudgeSent(double now) => _lastNudge = now;
    }
}
