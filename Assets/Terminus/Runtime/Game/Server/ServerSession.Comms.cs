// [서버] 파티 소통 도구 요청 (언리얼 ATerminusPlayerController 의 Server_SendPing / Server_SendQuickChat / Server_Nudge)
// 판정은 Core PartyCommsRules. 핑은 같은 구역 + 같은 편에게만, 퀵챗은 모두에게, 재촉은 아직 턴을 안 끝낸 사람에게만
//
// 등록: ServerSession.Awake 를 고치지 않으려고 게임 시작 때 한 번 NetBus 에 붙여 둠 (요청이 올 때 Instance 를 찾음)
// (행동 계획은 전투 쪽 DungeonServer.CombatPlan.cs)
// 구역 / 전투가 필요한 핑 / 재촉은 DungeonServer 의 상태를 봐야 해서 아래에 DungeonServer 부분 클래스를 둠
using System.Collections.Generic;
using Terminus.Comms;
using Terminus.Dungeon;
using Terminus.Game.Net;
using UnityEngine;

namespace Terminus.Game
{
    public sealed partial class ServerSession
    {
        readonly Dictionary<ulong, CommsThrottle> _commsThrottles = new Dictionary<ulong, CommsThrottle>();

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void RegisterCommsRequests()
        {
            NetBus.OnRequest<SendQuickChatReq>((id, r) => { if (Instance != null) Instance.OnQuickChat(id, r); });
            NetBus.OnRequest<SendPingReq>((id, r) => { if (DungeonServer.Instance != null) DungeonServer.Instance.OnCommsPing(id, r); });
            NetBus.OnRequest<NudgeReq>((id, r) => { if (DungeonServer.Instance != null) DungeonServer.Instance.OnCommsNudge(id); });
        }

        internal static double CommsNow => Time.realtimeSinceStartupAsDouble;

        internal CommsThrottle CommsThrottleOf(ulong clientId)
        {
            if (!_commsThrottles.TryGetValue(clientId, out var t)) _commsThrottles[clientId] = t = new CommsThrottle();
            return t;
        }

        // 언리얼 Server_SendQuickChat: 채팅처럼 모두에게 (1초 간격)
        void OnQuickChat(ulong sender, SendQuickChatReq req)
        {
            var m = Find(sender);
            if (m == null || req == null || !PartyComms.IsValidQuickChat(req.Kind)) return;
            if (!CommsThrottleOf(sender).TryQuickChat(CommsNow)) return;
            NetBus.Broadcast(new QuickChatMsg { Sender = sender, SenderName = m.Run.PlayerName, Kind = req.Kind });
        }
    }

    public sealed partial class DungeonServer
    {
        bool TryCommsContext(ulong clientId, out DungeonPlayer p, out DungeonArea area, out DungeonCombat combat)
        {
            area = null;
            combat = null;
            if (!_players.TryGetValue(clientId, out p) || !p.bConnected) return false;
            area = _areas.GetCurrentArea(p);
            combat = area != null ? area.Combat as DungeonCombat : null;
            return combat != null;
        }

        // 언리얼 Server_SendPing
        internal void OnCommsPing(ulong clientId, SendPingReq req)
        {
            if (req == null || !TryCommsContext(clientId, out var p, out var area, out var combat)) return;
            var target = req.Target ?? CommsTarget.None;
            if (!PartyCommsRules.CanPing(area.Occupants, combat, p, req.Kind, target)) return;
            if (!_session.CommsThrottleOf(clientId).TryPing(ServerSession.CommsNow)) return;

            var msg = new PingMsg
            {
                Ping = new PartyPing { Sender = p.Id, Kind = req.Kind, Target = target.Clone() },
                AreaIndex = area.AreaIndex,
                SenderName = p.Name,
                TargetName = CommsTargetName(combat, target),
            };
            foreach (var r in PartyCommsRules.GetAreaRecipients(area.Occupants, combat, p, true))
            {
                NetBus.Notify(r.Id, msg);
            }
        }

        // 핑 / 채팅 줄에 쓰는 이름 ("적 2", 플레이어 이름). 몬스터 번호 = 그 구역 몬스터 목록 순서 (언리얼 DescribeTarget)
        string CommsTargetName(DungeonCombat combat, CommsTarget target)
        {
            if (target.bPlayer) return _players.TryGetValue(target.PlayerId, out var tp) ? tp.Name : "?";
            for (int i = 0; i < combat.Monsters.Count; ++i)
            {
                if (combat.Monsters[i] != null && combat.Monsters[i].Id == target.MonsterId) return "적 " + (i + 1);
            }
            return "적";
        }

        // 언리얼 Server_Nudge: 할 게 없는 사람이 아직 턴을 안 끝낸 사람에게 (8초 간격, 실제로 보냈을 때만 잼)
        internal void OnCommsNudge(ulong clientId)
        {
            if (!TryCommsContext(clientId, out var p, out var area, out var combat)) return;
            var throttle = _session.CommsThrottleOf(clientId);
            double now = ServerSession.CommsNow;
            if (!throttle.CanNudge(now)) return;

            var targets = PartyCommsRules.GetNudgeTargets(area.Occupants, combat, p);
            foreach (var t in targets)
            {
                NetBus.Notify(t.Id, new NudgedMsg { Sender = p.Id, SenderName = p.Name });
            }
            if (targets.Count > 0) throttle.NudgeSent(now);
        }
    }
}
