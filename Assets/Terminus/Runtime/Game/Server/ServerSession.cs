// [서버 전용] 판 전체를 들고 있는 곳. 씬이 바뀌어도 살아 있음 (DontDestroyOnLoad)
// 언리얼: ATerminusPlayerState(런 상태, CopyProperties 로 맵 넘어 유지) + ATavernGameMode + UTerminusRunSubsystem
//  - 접속한 사람마다 RunPlayer (런 규칙 / 상태)
//  - 주점 요청 (직업 / 준비 / 하드 모드 / 시작)
//  - 파티 스냅샷을 모두에게 (언리얼 PlayerState 복제)
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Run;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Terminus.Game
{
    public sealed partial class ServerSession : MonoBehaviour
    {
        public sealed class Member
        {
            public ulong ClientId;
            public RunPlayer Run;
            public bool bConnected = true;
            // 다시 들어왔을 때 같은 사람인지 (Steam 이면 SteamId). 이공간 재합류 / 세이브 이어하기
            public string OnlineId = "";
        }

        public static ServerSession Instance { get; private set; }

        readonly Dictionary<ulong, Member> _members = new Dictionary<ulong, Member>();
        readonly List<ulong> _order = new List<ulong>();   // 들어온 순서 (언리얼 PlayerArray)
        readonly System.Random _rng = new System.Random();

        public RunSession Run { get; private set; }
        public RunSaveService Saves { get; private set; }
        public bool bHardMode { get; private set; }

        /// <summary>던전이 붙이는 추가 정보 (구역 / 전투 수치). 없으면 주점 정보만</summary>
        public Action<ulong, PlayerInfo> FillExtraInfo;

        /// <summary>던전이 나간 사람을 처리 (언리얼 Logout -> DungeonGameMode)</summary>
        public event Action<Member> MemberLeft;

        /// <summary>런 진행 중에 누가 들어옴 (이공간 재합류 / 세이브 이어하기). 던전이 상태를 되살림 (언리얼 DungeonGameMode::PostLogin)</summary>
        public event Action<Member> MemberJoinedMidRun;

        float _nextSync;
        string _lastPartyJson = "";

        public static ServerSession EnsureCreated()
        {
            if (Instance != null) return Instance;
            var go = new GameObject("ServerSession");
            DontDestroyOnLoad(go);
            return go.AddComponent<ServerSession>();
        }

        public static void DestroyIfExists()
        {
            if (Instance != null) Destroy(Instance.gameObject);
            Instance = null;
        }

        void Awake()
        {
            Instance = this;
            Run = new RunSession(new System.Random());
            Run.SetRoomName(NetworkRoot.IsSinglePlayer ? "" : NetworkRoot.RoomName);
            Saves = new RunSaveService(LocalProfile.Storage, LocalProfile.SaveSerializer, () => DateTime.Now);

            var nm = NetworkManager.Singleton;
            nm.OnClientConnectedCallback += OnClientConnected;
            nm.OnClientDisconnectCallback += OnClientDisconnected;
            foreach (var id in nm.ConnectedClientsIds) OnClientConnected(id);

            NetBus.OnRequest<SetNameReq>(OnSetName);
            NetBus.OnRequest<SelectCharacterReq>(OnSelectCharacter);
            NetBus.OnRequest<SetReadyReq>(OnSetReady);
            NetBus.OnRequest<SetHardModeReq>(OnSetHardMode);
            NetBus.OnRequest<StartGameReq>(OnStartGame);
            NetBus.OnRequest<ReportUpgradesReq>(OnReportUpgrades);
            NetBus.OnRequest<SendChatReq>(OnChat);
        }

        void OnDestroy()
        {
            if (Instance == this) Instance = null;
            var nm = NetworkManager.Singleton;
            if (nm != null)
            {
                nm.OnClientConnectedCallback -= OnClientConnected;
                nm.OnClientDisconnectCallback -= OnClientDisconnected;
            }
        }

        // ------------------------------------------------------------ 사람

        public IEnumerable<Member> Members => _order.Where(_members.ContainsKey).Select(id => _members[id]);

        public IEnumerable<Member> ConnectedMembers => Members.Where(m => m.bConnected);

        public Member Find(ulong clientId) => _members.TryGetValue(clientId, out var m) ? m : null;

        public bool IsHost(ulong clientId) => clientId == NetworkManager.ServerClientId;

        void OnClientConnected(ulong clientId)
        {
            if (_members.ContainsKey(clientId)) return;
            var payload = NetworkRoot.TakePayload(clientId);
            var name = !string.IsNullOrWhiteSpace(payload.Name) ? payload.Name.Trim()
                : clientId == NetworkManager.ServerClientId && !string.IsNullOrWhiteSpace(NetworkRoot.LocalPlayerName) ? NetworkRoot.LocalPlayerName
                : "모험가 " + (_order.Count + 1);
            if (name.Length > 16) name = name.Substring(0, 16);

            // 진행 중 재합류: 같은 사람의 예전 기록(나간 사람)은 치움. 상태는 던전이 이공간 기록에서 되살림
            if (NetworkRoot.bRunStarted && !string.IsNullOrEmpty(payload.OnlineId))
            {
                foreach (var old in _members.Values.Where(m => !m.bConnected && m.OnlineId == payload.OnlineId).ToList())
                {
                    _members.Remove(old.ClientId);
                    _order.Remove(old.ClientId);
                }
            }

            var member = new Member { ClientId = clientId, OnlineId = payload.OnlineId ?? "", Run = new RunPlayer(new System.Random(_rng.Next()), name) };
            member.Run.SetHardMode(bHardMode);
            member.Run.Notice += text => NetBus.Notify(clientId, new ChatMsg { Text = text, bSystem = true });
            _members[clientId] = member;
            _order.Add(clientId);
            if (NetworkRoot.bRunStarted) MemberJoinedMidRun?.Invoke(member);
            else SystemChat($"{name} 님이 들어왔습니다.");
            NetworkRoot.UpdateHostedRoom();
        }

        void OnClientDisconnected(ulong clientId)
        {
            if (!_members.TryGetValue(clientId, out var member)) return;
            // 진행 중에 나가면 안내는 던전(이공간)이 함
            if (!NetworkRoot.bRunStarted) SystemChat($"{member.Run.PlayerName} 님이 나갔습니다.");

            if (!NetworkRoot.bRunStarted)
            {
                _members.Remove(clientId);
                _order.Remove(clientId);
                return;
            }
            member.bConnected = false;
            MemberLeft?.Invoke(member);
        }

        // ------------------------------------------------------------ 주점 (언리얼 ATavernGameMode / Server_*)

        void OnSetName(ulong sender, SetNameReq req)
        {
            var m = Find(sender);
            if (m == null || NetworkRoot.bRunStarted) return;
            var name = (req.Name ?? "").Trim();
            if (name.Length == 0) return;
            if (name.Length > 16) name = name.Substring(0, 16);
            m.Run.PlayerName = name;
        }

        void OnSelectCharacter(ulong sender, SelectCharacterReq req)
        {
            var m = Find(sender);
            if (m == null || NetworkRoot.bRunStarted || m.Run.bClassLocked) return;
            m.Run.SetCharacterClass(req.Class);
            // 직업을 바꾸면 준비 풀기
            m.Run.bReady = false;
        }

        void OnSetReady(ulong sender, SetReadyReq req)
        {
            var m = Find(sender);
            if (m == null || NetworkRoot.bRunStarted) return;
            m.Run.bReady = req.bReady;
        }

        void OnSetHardMode(ulong sender, SetHardModeReq req)
        {
            if (!IsHost(sender) || NetworkRoot.bRunStarted) return;
            bHardMode = req.bHardMode;
            foreach (var m in Members) m.Run.SetHardMode(bHardMode);
            NetworkRoot.UpdateHostedRoom();
            SystemChat(bHardMode ? "하드 모드: 들고 간 창고 유물은 창고에서 사라집니다." : "하드 모드를 껐습니다.");
        }

        void OnReportUpgrades(ulong sender, ReportUpgradesReq req)
        {
            var m = Find(sender);
            if (m == null || NetworkRoot.bRunStarted) return;
            m.Run.SetReportedUpgradesFromClient(req.Upgrades, GameData.Upgrades);
        }

        bool AreAllPlayersReady() => ConnectedMembers.All(m => m.Run.bReady);

        // 언리얼 ATavernGameMode::TryStartGame (새 런)
        void OnStartGame(ulong sender, StartGameReq req)
        {
            if (!IsHost(sender) || NetworkRoot.bRunStarted) return;
            if (!AreAllPlayersReady())
            {
                NetBus.Notify(sender, new ChatMsg { Text = "모두 준비해야 시작할 수 있습니다.", bSystem = true });
                return;
            }

            var members = ConnectedMembers.ToList();
            Run.BeginNewRun(members.Count);
            foreach (var m in members)
            {
                m.Run.BeginRun(GameData.Upgrades);
                m.Run.SetHardMode(bHardMode);
            }

            NetworkRoot.bRunStarted = true;
            NetworkRoot.UpdateHostedRoom();
            SystemChat("던전으로 이동합니다...");
            NetworkRoot.ServerLoadScene(SceneNames.Dungeon);
        }

        void OnChat(ulong sender, SendChatReq req)
        {
            var m = Find(sender);
            var text = (req.Text ?? "").Trim();
            if (m == null || text.Length == 0) return;
            if (text.Length > 200) text = text.Substring(0, 200);
            NetBus.Broadcast(new ChatMsg { Sender = m.Run.PlayerName, Text = text });
        }

        public void SystemChat(string text)
        {
            if (string.IsNullOrEmpty(text)) return;
            NetBus.Broadcast(new ChatMsg { Text = text, bSystem = true });
        }

        // ------------------------------------------------------------ 파티 스냅샷 (언리얼 PlayerState 복제)

        void Update()
        {
            if (Time.unscaledTime < _nextSync) return;
            _nextSync = Time.unscaledTime + 0.1f;
            SyncParty(false);
        }

        public PartySnapshot BuildPartySnapshot()
        {
            var snap = new PartySnapshot
            {
                Scene = SceneManager.GetActiveScene().name,
                bHardMode = bHardMode,
                RoomName = Run.RoomName,
            };
            foreach (var m in Members)
            {
                var info = new PlayerInfo
                {
                    Id = m.ClientId,
                    Name = m.Run.PlayerName,
                    Class = m.Run.RunState.CharacterClass,
                    bReady = m.Run.bReady,
                    bHost = IsHost(m.ClientId),
                    bConnected = m.bConnected,
                    Run = m.Run.RunState,
                };
                FillExtraInfo?.Invoke(m.ClientId, info);
                snap.Players.Add(info);
            }
            return snap;
        }

        /// <summary>바뀌었으면 보냄. force 면 무조건 (새로 들어온 사람)</summary>
        public void SyncParty(bool force)
        {
            if (TerminusPlayer.AnyServerSide() == null) return;
            var snap = BuildPartySnapshot();
            var json = JsonUtility.ToJson(snap);
            if (!force && json == _lastPartyJson) return;
            _lastPartyJson = json;
            NetBus.Broadcast(snap);
        }
    }
}
