// 파티 소통 도구 메시지 (언리얼 ATerminusPlayerController 의 Server_SendPing / SendQuickChat / Nudge 와 Client_Receive*)
// 행동 계획 공유(SetCombatPlanReq / CombatPlanMsg)는 전투 쪽 NetMessages.Combat.cs
// 판정은 서버 (ServerSession.Comms.cs). 대상 배틀러는 CommsTarget (플레이어 번호 / 몬스터 번호)
using System;
using Terminus.Comms;

namespace Terminus.Game.Net
{
    // ================================================================ 요청

    [Serializable] public sealed class SendPingReq { public PingKind Kind; public CommsTarget Target = new CommsTarget(); }

    [Serializable] public sealed class SendQuickChatReq { public QuickChat Kind; }

    [Serializable] public sealed class NudgeReq { }

    // ================================================================ 개인 알림

    /// <summary>같은 구역 같은 편에게. SenderName / TargetName 은 채팅 줄용 (서버가 채움, 몬스터는 "적 2")</summary>
    [Serializable] public sealed class PingMsg { public PartyPing Ping = new PartyPing(); public int AreaIndex = -1; public string SenderName = ""; public string TargetName = ""; }

    /// <summary>모두에게 (채팅처럼)</summary>
    [Serializable] public sealed class QuickChatMsg { public ulong Sender; public string SenderName = ""; public QuickChat Kind; }

    /// <summary>아직 턴을 안 끝낸 같은 구역 사람에게</summary>
    [Serializable] public sealed class NudgedMsg { public ulong Sender; public string SenderName = ""; }
}
