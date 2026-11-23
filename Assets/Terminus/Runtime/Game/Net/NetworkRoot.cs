// NetworkManager 를 코드로 만들고 방장 / 참가 / 나가기를 맡음 (언리얼 USessionSubsystem 의 연결 부분)
// 트랜스포트는 지금 Unity Transport (IP). Steam 은 Facepunch 트랜스포트로 갈아 끼울 자리 (TransportMode)
// 씬 흐름: MainMenu -> (방장이 방을 열면) Tavern -> (전원 준비 후 시작) Dungeon. 씬 전환은 서버가 NetworkSceneManager 로
using System;
using Unity.Netcode;
using Unity.Netcode.Transports.UTP;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Terminus.Game.Net
{
    public static class SceneNames
    {
        public const string MainMenu = "MainMenu";
        public const string Tavern = "Tavern";
        public const string Dungeon = "Dungeon";
    }

    public enum TransportMode
    {
        UnityTransport, // IP 직접 연결 (개발 / Multiplayer Play Mode / 같은 PC 여러 개)
        Steam,          // TODO(Steam): Facepunch 트랜스포트
    }

    public static class NetworkRoot
    {
        public const ushort DefaultPort = 7777;
        public const int MaxPartySize = 4;
        const string PlayerPrefabPath = "Net/TerminusPlayer";

        public static TransportMode Mode = TransportMode.UnityTransport;

        /// <summary>싱글 (언리얼: 세션 없이 OpenLevel). 유니티는 혼자 방장으로 열고 아무도 못 들어오게</summary>
        public static bool IsSinglePlayer { get; private set; }

        /// <summary>방 이름 (언리얼 FTerminusRoomOptions.RoomName)</summary>
        public static string RoomName { get; private set; } = "";

        /// <summary>런이 시작돼 던전에 들어갔는가. 그 뒤엔 새 손님을 안 받음 (이공간 재접속은 나중에)</summary>
        public static bool bRunStarted;

        /// <summary>내 이름 (메인 메뉴에서 정함)</summary>
        public static string LocalPlayerName = "";

        public static event Action<string> ConnectionFailed;

        public static NetworkManager Ensure()
        {
            if (NetworkManager.Singleton != null) return NetworkManager.Singleton;

            var go = new GameObject("NetworkManager");
            UnityEngine.Object.DontDestroyOnLoad(go);
            var nm = go.AddComponent<NetworkManager>();
            var transport = go.AddComponent<UnityTransport>();
            // 상태 스냅샷(JSON)이 몇 KB 라 넉넉히. 신뢰성 메시지는 이 크기까지 쪼개서 감
            transport.MaxPayloadSize = 256 * 1024;
            transport.MaxPacketQueueSize = 512;

            var prefab = Resources.Load<GameObject>(PlayerPrefabPath);
            if (prefab == null) Debug.LogError($"[Net] 플레이어 프리팹 없음: Resources/{PlayerPrefabPath} (메뉴 Terminus/Setup/Build Project Assets)");

            nm.NetworkConfig = new NetworkConfig
            {
                NetworkTransport = transport,
                PlayerPrefab = prefab,
                ConnectionApproval = true,
                EnableSceneManagement = true,
                TickRate = 30,
            };
            if (prefab != null) nm.NetworkConfig.Prefabs.Add(new NetworkPrefab { Prefab = prefab });

            nm.ConnectionApprovalCallback = Approve;
            nm.OnClientDisconnectCallback += OnDisconnected;
            return nm;
        }

        // 언리얼 ATavernGameMode::PreLogin + USessionSubsystem::CheckJoinRequest
        static void Approve(NetworkManager.ConnectionApprovalRequest request, NetworkManager.ConnectionApprovalResponse response)
        {
            var nm = NetworkManager.Singleton;
            bool isHostSelf = request.ClientNetworkId == NetworkManager.ServerClientId;
            int current = nm.ConnectedClientsIds.Count;

            string reason = null;
            if (!isHostSelf)
            {
                if (IsSinglePlayer) reason = "혼자 하는 게임입니다.";
                else if (bRunStarted) reason = "이미 던전에 들어간 방입니다.";
                else if (current >= MaxPartySize) reason = "방이 가득 찼습니다.";
            }

            response.Approved = reason == null;
            response.Reason = reason ?? "";
            response.CreatePlayerObject = response.Approved;
            response.Pending = false;
        }

        static void OnDisconnected(ulong clientId)
        {
            var nm = NetworkManager.Singleton;
            if (nm == null) return;
            // 내가 끊김 (손님): 이유를 띄우고 메뉴로
            if (!nm.IsServer && clientId == nm.LocalClientId)
            {
                var reason = string.IsNullOrEmpty(nm.DisconnectReason) ? "방과 연결이 끊어졌습니다." : nm.DisconnectReason;
                ConnectionFailed?.Invoke(reason);
                LeaveToMenu();
            }
        }

        // ------------------------------------------------------------ 시작 / 참가

        /// <summary>혼자 시작 (주점으로)</summary>
        public static bool StartSingle()
        {
            IsSinglePlayer = true;
            RoomName = "";
            return StartHostInternal(DefaultPort, true);
        }

        /// <summary>방 만들기 (언리얼 HostSession). 주점으로</summary>
        public static bool StartHost(string roomName, ushort port = DefaultPort)
        {
            IsSinglePlayer = false;
            RoomName = string.IsNullOrWhiteSpace(roomName) ? "Terminus" : roomName.Trim();
            return StartHostInternal(port, false);
        }

        static bool StartHostInternal(ushort port, bool localOnly)
        {
            var nm = Ensure();
            bRunStarted = false;
            var transport = (UnityTransport)nm.NetworkConfig.NetworkTransport;
            // 싱글은 밖에서 못 들어오게 루프백만
            transport.SetConnectionData(localOnly ? "127.0.0.1" : "0.0.0.0", port, localOnly ? "127.0.0.1" : "0.0.0.0");
            if (!nm.StartHost())
            {
                ConnectionFailed?.Invoke("방을 열 수 없습니다. (포트 " + port + " 사용 중?)");
                return false;
            }
            ServerSession.EnsureCreated();
            nm.SceneManager.LoadScene(SceneNames.Tavern, LoadSceneMode.Single);
            return true;
        }

        /// <summary>
        /// 에디터에서 주점 / 던전 씬을 바로 켰을 때. 씬 전환 없이 그 자리에서 혼자 방장으로 시작
        /// 던전이면 런이 시작된 걸로 (주점 단계를 건너뜀)
        /// </summary>
        public static bool StartSingleInPlace(bool inDungeon)
        {
            IsSinglePlayer = true;
            RoomName = "";
            var nm = Ensure();
            var transport = (UnityTransport)nm.NetworkConfig.NetworkTransport;
            transport.SetConnectionData("127.0.0.1", DefaultPort, "127.0.0.1");
            if (!nm.StartHost())
            {
                ConnectionFailed?.Invoke("방을 열 수 없습니다.");
                return false;
            }
            ServerSession.EnsureCreated();
            bRunStarted = inDungeon;
            return true;
        }

        /// <summary>IP 로 참가 (언리얼 JoinSessionByIndex 자리. Steam 이면 로비로)</summary>
        public static bool Join(string address, ushort port = DefaultPort)
        {
            IsSinglePlayer = false;
            var nm = Ensure();
            var transport = (UnityTransport)nm.NetworkConfig.NetworkTransport;
            transport.SetConnectionData(string.IsNullOrWhiteSpace(address) ? "127.0.0.1" : address.Trim(), port);
            if (!nm.StartClient())
            {
                ConnectionFailed?.Invoke("참가할 수 없습니다.");
                return false;
            }
            return true;
        }

        /// <summary>[서버] 전원 다른 씬으로 (언리얼 ServerTravel)</summary>
        public static void ServerLoadScene(string sceneName)
        {
            var nm = NetworkManager.Singleton;
            if (nm == null || !nm.IsServer) return;
            var status = nm.SceneManager.LoadScene(sceneName, LoadSceneMode.Single);
            if (status != SceneEventProgressStatus.Started) Debug.LogWarning($"[Net] 씬 전환 실패 {sceneName}: {status}");
        }

        /// <summary>나가서 메인 메뉴로 (언리얼 LeaveToMenu)</summary>
        public static void LeaveToMenu()
        {
            var nm = NetworkManager.Singleton;
            if (nm != null && (nm.IsClient || nm.IsServer)) nm.Shutdown();
            ServerSession.DestroyIfExists();
            bRunStarted = false;
            IsSinglePlayer = false;
            if (SceneManager.GetActiveScene().name != SceneNames.MainMenu) SceneManager.LoadScene(SceneNames.MainMenu);
        }
    }
}
