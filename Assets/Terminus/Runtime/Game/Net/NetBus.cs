// 클라 <-> 서버 메시지 버스. 언리얼 PlayerController 의 Server_* / Client_* RPC 54개 자리
//  - RPC 하나하나를 NGO 메서드로 만들지 않고, 플레이어 오브젝트의 범용 채널 3개(요청 / 개인 알림 / 전체 알림)로
//    "메시지 타입 이름 + JSON" 을 보낸다. 메시지 타입은 NetMessages.cs 의 [Serializable] 클래스
//  - 서버는 요청을 보낸 연결(SenderClientId)로 플레이어를 정함 -> 클라가 남의 행동을 대신 못 함 (언리얼 Server RPC 와 같음)
using System;
using System.Collections.Generic;
using Unity.Netcode;
using UnityEngine;

namespace Terminus.Game.Net
{
    public static class NetBus
    {
        static readonly Dictionary<string, Action<ulong, string>> ServerHandlers = new Dictionary<string, Action<ulong, string>>();
        static readonly Dictionary<string, Action<string>> ClientHandlers = new Dictionary<string, Action<string>>();

        static string TypeKey<T>() => typeof(T).Name;

        // ------------------------------------------------------------ 서버

        /// <summary>[서버] 요청 처리기 등록. 같은 타입은 하나만 (나중 것이 이김)</summary>
        public static void OnRequest<T>(Action<ulong, T> handler) where T : class
        {
            ServerHandlers[TypeKey<T>()] = (sender, json) =>
            {
                var msg = Decode<T>(json);
                if (msg != null) handler(sender, msg);
            };
        }

        public static void RemoveRequest<T>() => ServerHandlers.Remove(TypeKey<T>());

        /// <summary>[서버] 한 사람에게 (언리얼 Client_ RPC)</summary>
        public static void Notify<T>(ulong clientId, T msg) where T : class
        {
            var player = TerminusPlayer.FindByClient(clientId);
            if (player == null) return;
            player.SendNotify(TypeKey<T>(), JsonUtility.ToJson(msg));
        }

        /// <summary>[서버] 모두에게 (언리얼 Multicast / 복제 상태)</summary>
        public static void Broadcast<T>(T msg) where T : class
        {
            var any = TerminusPlayer.AnyServerSide();
            if (any == null) return;
            any.SendBroadcast(TypeKey<T>(), JsonUtility.ToJson(msg));
        }

        internal static void DispatchRequest(ulong sender, string type, string json)
        {
            if (ServerHandlers.TryGetValue(type, out var handler))
            {
                try { handler(sender, json); }
                catch (Exception e) { Debug.LogException(e); }
            }
            else
            {
                Debug.LogWarning($"[NetBus] 처리기 없는 요청 {type} (보낸 사람 {sender})");
            }
        }

        // ------------------------------------------------------------ 클라

        /// <summary>[클라] 서버에 요청 (언리얼 Server_ RPC). 접속 전이면 버림</summary>
        public static void Request<T>(T msg) where T : class
        {
            var local = TerminusPlayer.Local;
            if (local == null)
            {
                Debug.LogWarning($"[NetBus] 내 플레이어가 아직 없어 {TypeKey<T>()} 요청을 버림");
                return;
            }
            local.SendRequest(TypeKey<T>(), JsonUtility.ToJson(msg));
        }

        /// <summary>[클라] 알림 받기 (개인 / 전체 공통). 같은 타입에 여러 개 붙일 수 있음</summary>
        public static void On<T>(Action<T> handler) where T : class
        {
            var key = TypeKey<T>();
            ClientHandlers.TryGetValue(key, out var existing);
            ClientHandlers[key] = existing + (json =>
            {
                var msg = Decode<T>(json);
                if (msg != null) handler(msg);
            });
        }

        public static void ClearClientHandlers() => ClientHandlers.Clear();

        internal static void DispatchClient(string type, string json)
        {
            if (!ClientHandlers.TryGetValue(type, out var handler) || handler == null) return;
            foreach (var d in handler.GetInvocationList())
            {
                try { ((Action<string>)d)(json); }
                catch (Exception e) { Debug.LogException(e); }
            }
        }

        static T Decode<T>(string json) where T : class
        {
            try { return JsonUtility.FromJson<T>(json); }
            catch (Exception e)
            {
                Debug.LogWarning($"[NetBus] {typeof(T).Name} 해석 실패: {e.Message}");
                return null;
            }
        }

        public static bool IsServer => NetworkManager.Singleton != null && NetworkManager.Singleton.IsServer;
        public static bool IsConnected => NetworkManager.Singleton != null && (NetworkManager.Singleton.IsClient || NetworkManager.Singleton.IsServer);
        public static ulong LocalClientId => NetworkManager.Singleton != null ? NetworkManager.Singleton.LocalClientId : 0;
    }
}
