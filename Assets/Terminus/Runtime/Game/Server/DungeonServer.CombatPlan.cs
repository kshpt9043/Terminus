// [서버] 전투 행동 계획 공유 (언리얼 ATerminusPlayerController::Server_SetCombatPlan + GetAreaRecipients / IsValidCommsTarget)
// 요청자는 늘 보낸 연결에서 꺼냄. 수치는 실제 사용과 같은 계산(강화 복사본 + 스텟 보정)으로 서버가 넣어 줌
// 받는 사람: 같은 구역의 같은 편 (보낸 사람 포함). 배신 전투면 상대 편에겐 안 감
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Game.Net;
using UnityEngine;

namespace Terminus.Game
{
    public sealed partial class DungeonServer
    {
        // 계획은 1초에 이만큼까지 (클라가 0.15초 간격으로 모아 보내니 정상이면 안 걸림). 지우기는 늘 받음
        // 간격으로 막지 않는 이유: 네트워크가 밀려 두 개가 붙어 도착하면 '더 최신' 계획이 버려져 화면에 옛 계획이 남음
        const int MaxPlansPerSecond = 15;

        sealed class PlanRecvWindow
        {
            public float Start = -1f;
            public int Count;
        }

        readonly Dictionary<ulong, PlanRecvWindow> _planWindows = new Dictionary<ulong, PlanRecvWindow>();

        /// <summary>RegisterRequests 에서 한 번 부름</summary>
        void RegisterCombatPlanRequests()
        {
            NetBus.OnRequest<SetCombatPlanReq>(OnSetCombatPlan);
        }

        void OnSetCombatPlan(ulong clientId, SetCombatPlanReq r)
        {
            if (!_players.TryGetValue(clientId, out var p) || !p.bConnected) return;
            var area = _areas.GetCurrentArea(p);
            if (area == null || !(area.Combat is DungeonCombat combat) || !combat.IsInCombat()) return;

            var plan = new CombatPlanMsg { AreaIndex = area.AreaIndex, PlannerId = p.Id, Cycle = combat.Cycle };

            float now = Time.unscaledTime;
            if (!_planWindows.TryGetValue(clientId, out var window))
            {
                window = new PlanRecvWindow();
                _planWindows[clientId] = window;
            }
            if (window.Start < 0f || now - window.Start >= 1f)
            {
                window.Start = now;
                window.Count = 0;
            }
            bool tooFast = r.SkillIndex >= 0 && ++window.Count > MaxPlansPerSecond;
            if (tooFast) return;   // 너무 잦은 계획은 버림 (비정상 클라)

            var caster = p.Battler;
            var skillPtr = r.SkillIndex >= 0 && p.Run != null ? p.Run.GetCombatSkill(r.SkillIndex) : null;

            if (skillPtr != null && caster != null && !caster.IsDead() && combat.IsTurnOf(p) && !combat.HasEndedTurn(p))
            {
                // 실제 사용과 같은 수치: 훈련소 / 상점 강화(복사본) + 스텟 보정
                var skill = skillPtr.Clone();
                p.Run.ApplyBasicSkillUpgrade(r.SkillIndex, skill);

                SkillExecutor.DescribeValueUse(skill.ActionKind, out bool damage, out bool shield, out bool heal, out bool onCaster);

                plan.SkillRow = p.Run.GetCombatSkillRow(r.SkillIndex);
                plan.TargetType = skill.TargetType;
                plan.Effect = damage ? CombatPlanEffect.Damage : shield ? CombatPlanEffect.Shield : heal ? CombatPlanEffect.Heal : CombatPlanEffect.None;
                plan.Amount = SkillExecutor.PreviewAmount(skill, caster);
                plan.HitCount = damage ? Mathf.Max(1, skill.HitCount) : 1;
                plan.bDeclared = r.bDeclared;

                switch (skill.TargetType)
                {
                    case TargetType.Self:
                        plan.TargetKind = CombatPlanTargetKind.Player;
                        plan.TargetPlayerId = p.Id;
                        break;
                    case TargetType.SingleEnemy:
                        SetPlanTarget(plan, p, area, combat, r, true);
                        break;
                    case TargetType.SingleAlly:
                        SetPlanTarget(plan, p, area, combat, r, false);
                        break;
                    default:
                        break;   // 전체: 대상 없음 (클라가 편으로 나눠 표시)
                }

                // 보호막 / 회복이 시전자에게 가는 스킬(독 연막 등)은 시전자 머리 위에
                if (onCaster && (shield || heal))
                {
                    plan.TargetType = TargetType.Self;
                    plan.TargetKind = CombatPlanTargetKind.Player;
                    plan.TargetMonsterId = -1;
                    plan.TargetPlayerId = p.Id;
                }
            }
            // SkillRow 가 "" 이면 '지움' 으로 감 (못 쓰는 상태가 됐을 때도)

            foreach (var occupant in area.Occupants)
            {
                if (occupant == null || !occupant.bConnected) continue;
                if (!combat.IsSameSide(p, occupant)) continue;
                NetBus.Notify(occupant.Id, plan);
            }
        }

        // 대상이 이 구역의 살아 있는 배틀러인가 (언리얼 IsValidCommsTarget). wantEnemy = 적(몬스터 / 배신 전투 상대 편) / 아군(나 포함)
        static void SetPlanTarget(CombatPlanMsg plan, DungeonPlayer sender, DungeonArea area, DungeonCombat combat, SetCombatPlanReq r, bool wantEnemy)
        {
            if (r.TargetKind == CombatPlanTargetKind.Monster)
            {
                if (!wantEnemy || combat.IsBetrayal()) return;
                foreach (var m in combat.Monsters)
                {
                    if (m == null || m.Id != r.TargetMonsterId || m.Stats == null || m.Stats.IsDead()) continue;
                    plan.TargetKind = CombatPlanTargetKind.Monster;
                    plan.TargetMonsterId = m.Id;
                    return;
                }
                return;
            }

            if (r.TargetKind == CombatPlanTargetKind.Player)
            {
                foreach (var occupant in area.Occupants)
                {
                    if (occupant == null || occupant.Id != r.TargetPlayerId || occupant.IsDead()) continue;
                    if (wantEnemy == combat.IsSameSide(sender, occupant)) return;
                    plan.TargetKind = CombatPlanTargetKind.Player;
                    plan.TargetPlayerId = occupant.Id;
                    return;
                }
            }
        }
    }
}
