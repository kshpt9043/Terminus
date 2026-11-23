// 접속한 사람마다 하나 (NGO PlayerPrefab). 언리얼 ATerminusPlayerController 의 RPC 통로 역할만 한다
// 런 상태 / 전투 상태 같은 내용은 서버(ServerSession / DungeonServer)가 들고, 스냅샷으로 뿌린다
using System.Collections.Generic;
using Unity.Netcode;
using UnityEngine;

namespace Terminus.Game.Net
{
    public sealed class TerminusPlayer : NetworkBehaviour
    {
        static readonly Dictionary<ulong, TerminusPlayer> ByClient = new Dictionary<ulong, TerminusPlayer>();

        /// <summary>이 컴퓨터의 내 플레이어 (클라 / 방장)</summary>
        public static TerminusPlayer Local { get; private set; }

        public static TerminusPlayer FindByClient(ulong clientId) => ByClient.TryGetValue(clientId, out var p) ? p : null;

        public static IEnumerable<TerminusPlayer> All => ByClient.Values;

        public static TerminusPlayer AnyServerSide()
        {
            foreach (var p in ByClient.Values)
            {
                if (p != null && p.IsServer && p.IsSpawned) return p;
            }
            return null;
        }

        public override void OnNetworkSpawn()
        {
            ByClient[OwnerClientId] = this;
            if (IsOwner) Local = this;
            DontDestroyOnLoad(gameObject);
            name = "Player_" + OwnerClientId + (IsOwner ? " (나)" : "");
        }

        public override void OnNetworkDespawn()
        {
            if (ByClient.TryGetValue(OwnerClientId, out var p) && p == this) ByClient.Remove(OwnerClientId);
            if (Local == this) Local = null;
        }

        // ------------------------------------------------------------ 통로 3개

        internal void SendRequest(string type, string json) => RequestRpc(type, json);

        internal void SendNotify(string type, string json) => NotifyRpc(type, json);

        internal void SendBroadcast(string type, string json) => BroadcastRpc(type, json);

        // 클라 -> 서버. 주인만 부를 수 있음. 보낸 사람은 연결에서 꺼냄
        [Rpc(SendTo.Server, InvokePermission = RpcInvokePermission.Owner)]
        void RequestRpc(string type, string json, RpcParams rpcParams = default)
        {
            NetBus.DispatchRequest(rpcParams.Receive.SenderClientId, type, json);
        }

        // 서버 -> 이 플레이어의 주인 한 명
        [Rpc(SendTo.Owner, InvokePermission = RpcInvokePermission.Server)]
        void NotifyRpc(string type, string json)
        {
            NetBus.DispatchClient(type, json);
        }

        // 서버 -> 모두 (방장 포함)
        [Rpc(SendTo.ClientsAndHost, InvokePermission = RpcInvokePermission.Server)]
        void BroadcastRpc(string type, string json)
        {
            NetBus.DispatchClient(type, json);
        }
    }
}
