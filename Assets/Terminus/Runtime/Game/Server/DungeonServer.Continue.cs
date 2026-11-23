// [서버] 멀티 세이브 이어하기의 던전 쪽 (언리얼 ADungeonGameMode::InitGame 의 LoadedSave + 방장 PostLogin)
//  - 지도 / 층은 ServerSession.Continue 가 RunSession.BeginLoadedRun 으로 이미 넣어 둠 -> StartFirstFloor 가 그 지도를 되살림
//  - 방장 RunState 도 이미 세이브 당시로 (BuildPlayers 가 그 스텟 / 체력으로 배틀러를 만듦)
//  - 여기서: 세이브 플레이어 전원을 이공간 '아직 안 온 사람'으로 두고 방장 자리를 빼고, 방장 합류를 알림
//    -> 남은 사람이 있으면 이공간 화면 + 방 목록에 재합류 대기로 뜸 (DungeonServer.Rift 의 RejoinListingChanged)
//    -> 들어오는 대로 OnMemberJoinedMidRun 이 세이브 당시 상태로 되살림
using System.Linq;
using Terminus.Game.Net;
using Unity.Netcode;
using UnityEngine;

namespace Terminus.Game
{
    public sealed partial class DungeonServer
    {
        // Awake(DungeonServer.cs) 다음 = 이공간(BuildRift)이 생긴 뒤
        void Start()
        {
            if (_session == null || _rift == null || NetworkRoot.IsSinglePlayer) return;
            var save = _session.TakeContinueSave();
            if (save == null || save.Summary == null || !save.Summary.bMultiplayer) return;

            _rift.BeginFromSave(save);

            // 지금 세이브는 플레이어 아이디 자리에 접속 번호가 들어 있을 수 있음 (온라인 아이디가 아님)
            // -> 그럴 땐 이름으로 맞춰 봄 (언리얼 UseLooseRejoinMatching). 이름도 안 맞으면 빈 자리 아무거나
            bool hasOnlineIds = save.Players.All(p => p != null && !string.IsNullOrEmpty(p.PlayerId) && p.PlayerId.Contains(":"));
            if (!hasOnlineIds) _rift.bLooseRejoinMatching = true;

            // 방장은 ServerSession 이 이미 자기 자리로 되살림 -> 이공간 목록에서 같은 규칙으로 뺌
            var host = _session.Find(NetworkManager.ServerClientId);
            if (host != null && _players.ContainsKey(host.ClientId))
            {
                _rift.ClaimDeparted(host.OnlineId, host.Run.PlayerName, true);
                _rift.AnnounceReturn(host.Run.PlayerName);
            }
            Debug.Log($"[Continue] 멀티 세이브 {save.Summary.SlotName}: 기다리는 사람 {_rift.DepartedPlayers.Count}명 ({string.Join(", ", _rift.GetDepartedNames())})");
            SyncState(true);
        }
    }
}
