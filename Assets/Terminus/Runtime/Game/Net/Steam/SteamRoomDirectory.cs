// Steam 로비 = 방 목록 / 참가 / 초대 (언리얼 OnlineSubsystemSteam 세션 + USessionSubsystem 자리)
// 광고 키는 언리얼과 같은 이름 (TERMINUSBUILD / ROOMNAME / LOCKED / LISTED / INGAME / REJOIN)
//  - LISTED=0 은 초대 전용 (검색에서 거름), INGAME=1 은 던전 진행 중 (검색에서 거름), REJOIN=1 은 이공간 (진행 중이지만 목록에 뜸)
// 실제 접속은 Facepunch 트랜스포트의 Steam 릴레이 (방장 SteamId 로)
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Steamworks;
using Steamworks.Data;
using UnityEngine;

namespace Terminus.Game.Net.Steam
{
    public sealed class SteamRoomDirectory : IRoomDirectoryBackend
    {
        // 다른 빌드 / 다른 게임(앱 480 은 공용 테스트 앱)의 로비가 섞이지 않게
        public const string BuildTag = "Terminus-Unity-1";

        const string KeyBuild = "TERMINUSBUILD";
        const string KeyRoomName = "ROOMNAME";
        const string KeyHost = "HOSTNAME";
        const string KeyLocked = "LOCKED";
        const string KeyListed = "LISTED";
        const string KeyInGame = "INGAME";
        const string KeyRejoin = "REJOIN";
        const string KeyHard = "HARD";

        Lobby? _hosted;
        bool _creating;
        RoomListing _pendingInfo;
        bool _pendingListed;

        public bool SupportsBrowsing => SteamBootstrap.Available;
        public bool SupportsInvites => SteamBootstrap.Available;

        public SteamRoomDirectory()
        {
            SteamFriends.OnGameLobbyJoinRequested += OnInviteAccepted;
        }

        // ------------------------------------------------------------ 목록 (언리얼 FindSessions)

        public async void Refresh(Action<List<RoomListing>> done)
        {
            var result = new List<RoomListing>();
            if (!SteamBootstrap.Available)
            {
                done(result);
                return;
            }
            try
            {
                var lobbies = await SteamMatchmaking.LobbyList
                    .WithKeyValue(KeyBuild, BuildTag)
                    .WithKeyValue(KeyListed, "1")
                    .FilterDistanceWorldwide()
                    .WithMaxResults(200)
                    .RequestAsync();
                if (lobbies != null)
                {
                    foreach (var l in lobbies)
                    {
                        bool inGame = l.GetData(KeyInGame) == "1";
                        bool rejoin = l.GetData(KeyRejoin) == "1";
                        // 진행 중인 방은 이공간(나간 사람 재합류 대기)일 때만
                        if (inGame && !rejoin) continue;
                        result.Add(new RoomListing
                        {
                            Id = l.Id.Value.ToString(),
                            RoomName = l.GetData(KeyRoomName),
                            HostName = l.GetData(KeyHost),
                            Players = l.MemberCount,
                            MaxPlayers = l.MaxMembers,
                            bHasPassword = l.GetData(KeyLocked) == "1",
                            bInRun = inGame,
                            bRejoinOpen = rejoin,
                            bHardMode = l.GetData(KeyHard) == "1",
                        });
                    }
                }
            }
            catch (Exception e)
            {
                Debug.LogWarning("[Steam] 로비 검색 실패: " + e.Message);
            }
            done(result);
        }

        // ------------------------------------------------------------ 참가 (언리얼 JoinSessionByIndex)

        public async void Join(RoomListing room, string password, Action<bool, string> done)
        {
            if (!SteamBootstrap.Available || room == null || !ulong.TryParse(room.Id, out var id))
            {
                done(false, "Steam 에 연결되어 있지 않습니다.");
                return;
            }
            var (ok, error) = await JoinLobby(new SteamId { Value = id }, password);
            done(ok, error);
        }

        async Task<(bool, string)> JoinLobby(SteamId lobbyId, string password)
        {
            try
            {
                var lobby = await SteamMatchmaking.JoinLobbyAsync(lobbyId);
                if (!lobby.HasValue) return (false, "방에 들어갈 수 없습니다. (이미 없어졌거나 가득 참)");
                var owner = lobby.Value.Owner.Id;
                // 로비는 방장을 찾는 용도. 실제 게임 연결은 트랜스포트로 하고, 로비에선 바로 나옴 (방장이 인원을 관리)
                lobby.Value.Leave();
                if (!NetworkRoot.JoinSteam(owner.Value, password)) return (false, "참가할 수 없습니다.");
                return (true, "");
            }
            catch (Exception e)
            {
                Debug.LogWarning("[Steam] 참가 실패: " + e.Message);
                return (false, "참가할 수 없습니다.");
            }
        }

        // 친구 초대를 수락함 (Steam 오버레이) -> 그 방으로. 비밀번호 방이면 초대여도 비밀번호가 필요 (언리얼과 같음)
        async void OnInviteAccepted(Lobby lobby, SteamId friend)
        {
            if (NetworkRoot.IsBusy) NetworkRoot.LeaveToMenu();
            var (ok, error) = await JoinLobby(lobby.Id, NetworkRoot.PendingInvitePassword);
            if (!ok) NetworkRoot.RaiseConnectionFailed(error);
        }

        // ------------------------------------------------------------ 초대 (언리얼 ShowInviteUI)

        public void InviteFriends()
        {
            if (_hosted.HasValue) SteamFriends.OpenGameInviteOverlay(_hosted.Value.Id);
            else Debug.LogWarning("[Steam] 초대할 방이 없습니다 (방장만 초대 가능)");
        }

        // ------------------------------------------------------------ 방장: 광고 (언리얼 HostSession / SetRejoinListing / StartRun)

        public async void PublishHostedRoom(RoomListing info, string password, bool listed)
        {
            if (!SteamBootstrap.Available || info == null) return;
            _pendingInfo = info;
            _pendingListed = listed;
            if (!_hosted.HasValue)
            {
                if (_creating) return;
                _creating = true;
                try
                {
                    _hosted = await SteamMatchmaking.CreateLobbyAsync(Math.Max(1, info.MaxPlayers));
                }
                catch (Exception e)
                {
                    Debug.LogWarning("[Steam] 로비 만들기 실패: " + e.Message);
                }
                _creating = false;
                if (!_hosted.HasValue) return;
            }
            Apply(_hosted.Value, _pendingInfo, password, _pendingListed);
        }

        static void Apply(Lobby lobby, RoomListing info, string password, bool listed)
        {
            lobby.SetPublic();
            lobby.SetData(KeyBuild, BuildTag);
            lobby.SetData(KeyRoomName, info.RoomName ?? "");
            lobby.SetData(KeyHost, SteamClient.Name ?? "");
            lobby.SetData(KeyLocked, string.IsNullOrEmpty(password) ? "0" : "1");
            lobby.SetData(KeyListed, listed ? "1" : "0");
            lobby.SetData(KeyInGame, info.bInRun ? "1" : "0");
            lobby.SetData(KeyRejoin, info.bRejoinOpen ? "1" : "0");
            lobby.SetData(KeyHard, info.bHardMode ? "1" : "0");
            // 진행 중이면 새 손님을 안 받음. 이공간이면 나간 사람이 돌아올 수 있게 열어 둠 (누가 들어올지는 방장 접속 승인이 거름)
            lobby.SetJoinable(!info.bInRun || info.bRejoinOpen);
        }

        public void CloseHostedRoom()
        {
            if (_hosted.HasValue)
            {
                _hosted.Value.Leave();
                _hosted = null;
            }
        }
    }
}
