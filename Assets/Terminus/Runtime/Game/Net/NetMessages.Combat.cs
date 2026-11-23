// 전투 행동 계획 공유 메시지 (언리얼 ATerminusPlayerController::Server_SetCombatPlan / Client_ReceiveCombatPlan + Comms/PartyCommsTypes.h 의 FCombatPlan)
//  - 클라: 대상을 고르는 중(마우스가 올라간 대상) / Shift 로 '예약' 한 계획을 0.15초 간격으로 모아 보냄 (바뀐 것만)
//  - 서버: 실제 사용과 같은 수치(훈련소 / 상점 강화 + 스텟 보정)를 계산해 같은 구역의 같은 편에게 (보낸 사람 포함)
//  - SkillRow 가 "" 이면 '계획 지움'
// 언리얼은 대상을 액터 포인터로 보냈음 -> 여기선 몬스터 Id(MonsterInfo.Id) / 플레이어 ClientId 로
using System;
using Terminus.Data;

namespace Terminus.Game.Net
{
    /// <summary>계획한 스킬이 수치(BaseValue)를 어디에 쓰는지 (언리얼 ECombatPlanEffect). 머리 위 표시 / 예상 피해 합산용</summary>
    public enum CombatPlanEffect
    {
        None,     // 상태이상만 등 (스킬 이름만 표시)
        Damage,
        Shield,
        Heal,
    }

    /// <summary>계획 대상 종류. None = 대상 없음 (전체 스킬 / 아직 고르는 중)</summary>
    public enum CombatPlanTargetKind
    {
        None,
        Monster,
        Player,
    }

    /// <summary>[클라 -> 서버] 내 행동 계획 (언리얼 Server_SetCombatPlan). SkillIndex -1 = 지움</summary>
    [Serializable]
    public sealed class SetCombatPlanReq
    {
        public int SkillIndex = -1;          // 0~2 기본, 3~ 강화 칸
        public CombatPlanTargetKind TargetKind = CombatPlanTargetKind.None;
        public int TargetMonsterId = -1;     // MonsterInfo.Id
        public ulong TargetPlayerId;         // PlayerInfo.Id
        public bool bDeclared;               // Shift 로 '예약' 한 계획 (false = 대상 고르는 중인 실시간 계획)
    }

    /// <summary>[서버 -> 같은 편] 한 사람의 이번 턴 행동 계획 (언리얼 Client_ReceiveCombatPlan(FCombatPlan))</summary>
    [Serializable]
    public sealed class CombatPlanMsg
    {
        public int AreaIndex = -1;
        public ulong PlannerId;
        public string SkillRow = "";         // "" = 계획 지움
        public TargetType TargetType = TargetType.Self;
        public CombatPlanTargetKind TargetKind = CombatPlanTargetKind.None;
        public int TargetMonsterId = -1;
        public ulong TargetPlayerId;
        public CombatPlanEffect Effect = CombatPlanEffect.None;
        public int Amount;                   // 한 대 수치 (강화 + 스텟 보정까지). 대상 상태(급소 등)는 클라가 더 계산
        public int HitCount = 1;
        public int Cycle;                    // 어느 사이클의 계획인가. 넘어가면 클라가 알아서 안 보여 줌
        public bool bDeclared;

        public bool IsValid() => !string.IsNullOrEmpty(SkillRow);
    }
}
