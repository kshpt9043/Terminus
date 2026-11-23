// NetworkManager 를 코드로 만들고 방장 / 참가 / 나가기를 맡음 (언리얼 USessionSubsystem 의 연결 부분)
// 트랜스포트 두 개를 갈아 끼움 (언리얼 DefaultEngine.ini 의 SteamSockets + IpNetDriver 폴백과 같은 생각)
//  - Steam: Facepunch 트랜스포트 (Steam 릴레이, 방장 SteamId 로 접속). 방 목록 / 초대는 SteamRoomDirectory
//  - UnityTransport: IP 직접 (개발 / Multiplayer Play Mode / -nosteam)
// 씬 흐름: MainMenu -> (방장이 방을 열면) Tavern -> (전원 준비 후 시작) Dungeon. 씬 전환은 서버가 NetworkSceneManager 로
using System;
using System.Text;
using Netcode.Transports.Facepunch;
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
        Steam,          // Steam 릴레이 (Facepunch)
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

        /// <summary>방 비밀번호 (방장만 앎). 빈 값 = 없음</summary>
        public static string HostPassword { get; private set; } = "";

        /// <summary>목록에 공개 (false = 초대 전용)</summary>
        public static bool bListed { get; private set; } = true;

        /// <summary>런이 시작돼 던전에 들어갔는가. 그 뒤엔 새 손님을 안 받음 (이공간 재접속은 따로)</summary>
        public static bool bRunStarted;

        /// <summary>내 이름 (메인 메뉴에서 정함. Steam 이면 기본값이 Steam 이름)</summary>
        public static string LocalPlayerName = "";

        /// <summary>초대로 들어갈 때 쓸 비밀번호 (비밀번호 방에 초대받았을 때 화면이 미리 넣어 둠)</summary>
        public static string PendingInvitePassword = "";

        public static event Action<string> ConnectionFailed;

        /// <summary>[서버] 런 진행 중 들어오려는 사람을 받을지 (이공간 재합류). DungeonServer 가 채움. null 이면 안 받음</summary>
        public static Func<ConnectPayload, string> CanRejoinMidRun;

        /// <summary>[서버] 접속 승인 때 받은 정보 (ServerSession 이 사람을 만들 때 꺼내 씀)</summary>
        static readonly System.Collections.Generic.Dictionary<ulong, ConnectPayload> _payloads = new System.Collections.Generic.Dictionary<ulong, ConnectPayload>();

        public static ConnectPayload TakePayload(ulong clientId)
        {
            if (_payloads.TryGetValue(clientId, out var p))
            {
                _payloads.Remove(clientId);
                return p;
            }
            return clientId == NetworkManager.ServerClientId ? PlayerIdentity.MakePayload("") : new ConnectPayload();
        }

        /// <summary>이공간이 열려 있음 (진행 중이지만 나간 사람이 돌아올 수 있음). 방 광고에 REJOIN 으로</summary>
        public static bool bRejoinOpen { get; private set; }

        public static void SetRejoinOpen(bool open)
        {
            if (bRejoinOpen == open) return;
            bRejoinOpen = open;
            UpdateHostedRoom();
        }

        public static void RaiseConnectionFailed(string reason) => ConnectionFailed?.Invoke(reason);

        /// <summary>방장 / 손님으로 이미 연결돼 있거나 연결 중</summary>
        public static bool IsBusy => NetworkManager.Singleton != null && (NetworkManager.Singleton.IsClient || NetworkManager.Singleton.IsServer || NetworkManager.Singleton.ShutdownInProgress);

        static UnityTransport _ip;
        static FacepunchTransport _steam;

        public static NetworkManager Ensure()
        {
            if (NetworkManager.Singleton != null) return NetworkManager.Singleton;

            var go = new GameObject("NetworkManager");
            UnityEngine.Object.DontDestroyOnLoad(go);
            var nm = go.AddComponent<NetworkManager>();
            _ip = go.AddComponent<UnityTransport>();
            // 상태 스냅샷(JSON)이 몇 KB 라 넉넉히. 신뢰성 메시지는 이 크기까지 쪼개서 감
            _ip.MaxPayloadSize = 256 * 1024;
            _ip.MaxPacketQueueSize = 512;
            _steam = go.AddComponent<FacepunchTransport>();

            var prefab = Resources.Load<GameObject>(PlayerPrefabPath);
            if (prefab == null) Debug.LogError($"[Net] 플레이어 프리팹 없음: Resources/{PlayerPrefabPath} (메뉴 Terminus/Setup/Build Project Assets)");

            nm.NetworkConfig = new NetworkConfig
            {
                NetworkTransport = _ip,
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

        static void UseTransport(NetworkManager nm, TransportMode mode)
        {
            nm.NetworkConfig.NetworkTransport = mode == TransportMode.Steam ? (NetworkTransport)_steam : _ip;
        }

        // 언리얼 USessionSubsystem::IsValidRoomPassword: 영문 / 숫자 1~16 (접속 옵션에 실어 보내서)
        public static bool IsValidRoomPassword(string password)
        {
            if (string.IsNullOrEmpty(password) || password.Length > 16) return false;
            foreach (var c in password)
            {
                bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                if (!ok) return false;
            }
            return true;
        }

        // 언리얼 ATavernGameMode::PreLogin + USessionSubsystem::CheckJoinRequest
        static void Approve(NetworkManager.ConnectionApprovalRequest request, NetworkManager.ConnectionApprovalResponse response)
        {
            var nm = NetworkManager.Singleton;
            bool isHostSelf = request.ClientNetworkId == NetworkManager.ServerClientId;
            int current = nm.ConnectedClientsIds.Count;
            var payload = ConnectPayload.FromBytes(request.Payload);

            string reason = null;
            if (!isHostSelf)
            {
                if (IsSinglePlayer) reason = "혼자 하는 게임입니다.";
                else if (current >= MaxPartySize) reason = "방이 가득 찼습니다.";
                else if (!string.IsNullOrEmpty(HostPassword) && payload.Password != HostPassword) reason = "비밀번호가 틀렸습니다.";
                else if (bRunStarted)
                {
                    // 진행 중: 나갔던 사람(이공간) / 세이브 플레이어만 (언리얼 ADungeonGameMode::PreLogin)
                    var err = CanRejoinMidRun != null ? CanRejoinMidRun(payload) : "이미 던전에 들어간 방입니다.";
                    if (!string.IsNullOrEmpty(err)) reason = err;
                }
            }
            if (reason == null) _payloads[request.ClientNetworkId] = payload;

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
            else if (nm.IsServer)
            {
                UpdateHostedRoom();
            }
        }

        // ------------------------------------------------------------ 시작 / 참가

        /// <summary>혼자 시작 (주점으로). 트랜스포트는 IP 루프백 (Steam 이 필요 없음)</summary>
        public static bool StartSingle()
        {
            IsSinglePlayer = true;
            RoomName = "";
            HostPassword = "";
            return StartHostInternal(TransportMode.UnityTransport, true);
        }

        /// <summary>방 만들기 (언리얼 HostSession). 주점으로. password 는 비우거나 영문 / 숫자 1~16</summary>
        public static bool StartHost(string roomName, string password = "", bool listed = true)
        {
            if (!string.IsNullOrEmpty(password) && !IsValidRoomPassword(password))
            {
                ConnectionFailed?.Invoke("비밀번호는 영문 / 숫자 1~16자입니다.");
                return false;
            }
            IsSinglePlayer = false;
            RoomName = string.IsNullOrWhiteSpace(roomName) ? "Terminus" : roomName.Trim();
            if (RoomName.Length > 24) RoomName = RoomName.Substring(0, 24);
            HostPassword = password ?? "";
            bListed = listed;
            if (!StartHostInternal(Mode, false)) return false;
            UpdateHostedRoom();
            return true;
        }

        static bool StartHostInternal(TransportMode mode, bool localOnly)
        {
            var nm = Ensure();
            bRunStarted = false;
            UseTransport(nm, mode);
            nm.NetworkConfig.ConnectionData = Array.Empty<byte>();
            if (mode == TransportMode.UnityTransport)
            {
                // 싱글은 밖에서 못 들어오게 루프백만
                _ip.SetConnectionData(localOnly ? "127.0.0.1" : "0.0.0.0", DefaultPort, localOnly ? "127.0.0.1" : "0.0.0.0");
            }
            if (!nm.StartHost())
            {
                ConnectionFailed?.Invoke(mode == TransportMode.Steam ? "Steam 방을 열 수 없습니다." : "방을 열 수 없습니다. (포트 " + DefaultPort + " 사용 중?)");
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
            HostPassword = "";
            var nm = Ensure();
            UseTransport(nm, TransportMode.UnityTransport);
            _ip.SetConnectionData("127.0.0.1", DefaultPort, "127.0.0.1");
            if (!nm.StartHost())
            {
                ConnectionFailed?.Invoke("방을 열 수 없습니다.");
                return false;
            }
            ServerSession.EnsureCreated();
            bRunStarted = inDungeon;
            return true;
        }

        /// <summary>IP 로 참가 (IP 모드)</summary>
        public static bool Join(string address, string password = "", ushort port = DefaultPort)
        {
            IsSinglePlayer = false;
            var nm = Ensure();
            UseTransport(nm, TransportMode.UnityTransport);
            _ip.SetConnectionData(string.IsNullOrWhiteSpace(address) ? "127.0.0.1" : address.Trim(), port);
            nm.NetworkConfig.ConnectionData = PlayerIdentity.MakePayload(password).ToBytes();
            if (!nm.StartClient())
            {
                ConnectionFailed?.Invoke("참가할 수 없습니다.");
                return false;
            }
            return true;
        }

        /// <summary>Steam 으로 참가 (방장 SteamId). 방 목록 / 초대가 부름</summary>
        public static bool JoinSteam(ulong hostSteamId, string password = "")
        {
            IsSinglePlayer = false;
            var nm = Ensure();
            UseTransport(nm, TransportMode.Steam);
            _steam.targetSteamId = hostSteamId;
            nm.NetworkConfig.ConnectionData = PlayerIdentity.MakePayload(password).ToBytes();
            if (!nm.StartClient())
            {
                ConnectionFailed?.Invoke("참가할 수 없습니다.");
                return false;
            }
            return true;
        }

        /// <summary>[방장] 방 광고 갱신 (인원 / 진행 중 / 하드 모드). 방 목록이 없는 모드면 아무것도 안 함</summary>
        public static void UpdateHostedRoom()
        {
            var nm = NetworkManager.Singleton;
            if (nm == null || !nm.IsServer || IsSinglePlayer) return;
            RoomDirectory.PublishHostedRoom(new RoomListing
            {
                RoomName = RoomName,
                HostName = LocalPlayerName,
                Players = nm.ConnectedClientsIds.Count,
                MaxPlayers = MaxPartySize,
                bHasPassword = !string.IsNullOrEmpty(HostPassword),
                bInRun = bRunStarted,
                bRejoinOpen = bRejoinOpen,
                bHardMode = ServerSession.Instance != null && ServerSession.Instance.bHardMode,
            }, HostPassword, bListed);
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
            if (nm != null && nm.IsServer) RoomDirectory.CloseHostedRoom();
            if (nm != null && (nm.IsClient || nm.IsServer)) nm.Shutdown();
            ServerSession.DestroyIfExists();
            bRunStarted = false;
            bRejoinOpen = false;
            IsSinglePlayer = false;
            HostPassword = "";
            CanRejoinMidRun = null;
            _payloads.Clear();
            if (SceneManager.GetActiveScene().name != SceneNames.MainMenu) SceneManager.LoadScene(SceneNames.MainMenu);
        }
    }
}
