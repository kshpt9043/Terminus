// 방 목록 / 초대 (언리얼 USessionSubsystem 의 FindSessions / JoinSessionByIndex / ShowInviteUI 자리)
// 화면(방 목록)은 여기만 씀. 실제 구현은 트랜스포트마다:
//  - UnityTransport (IP): 목록 없음 (SupportsBrowsing = false). IP 로 직접 참가
//  - Steam: Steam 로비 검색 / 참가 / 친구 초대 (SteamRoomDirectory)
using System;
using System.Collections.Generic;

namespace Terminus.Game.Net
{
    [Serializable]
    public sealed class RoomListing
    {
        public string Id = "";          // Steam 로비 Id 등 (트랜스포트가 정함)
        public string RoomName = "";
        public string HostName = "";
        public int Players;
        public int MaxPlayers = NetworkRoot.MaxPartySize;
        public bool bHasPassword;
        public bool bInRun;             // 던전 진행 중 (이공간 재접속만 가능)
        public bool bRejoinOpen;        // 진행 중이지만 나간 사람을 기다리는 중 (이공간). 목록에 뜨고 그 사람만 들어올 수 있음
        public bool bHardMode;
        public int Ping = -1;
    }

    public interface IRoomDirectoryBackend
    {
        bool SupportsBrowsing { get; }
        bool SupportsInvites { get; }
        void Refresh(Action<List<RoomListing>> done);
        void Join(RoomListing room, string password, Action<bool, string> done);
        void InviteFriends();
        // 방장이 방을 열 때 / 정보가 바뀔 때 (이름 / 인원 / 비밀번호 / 진행 중)
        void PublishHostedRoom(RoomListing info, string password, bool listed);
        void CloseHostedRoom();
    }

    public static class RoomDirectory
    {
        static IRoomDirectoryBackend _backend = new NoBrowsingBackend();

        public static void SetBackend(IRoomDirectoryBackend backend) => _backend = backend ?? new NoBrowsingBackend();

        public static bool SupportsBrowsing => _backend.SupportsBrowsing;
        public static bool SupportsInvites => _backend.SupportsInvites;

        /// <summary>방 목록 새로고침. 끝나면 done(목록). 실패하면 빈 목록</summary>
        public static void Refresh(Action<List<RoomListing>> done) => _backend.Refresh(done ?? (_ => { }));

        /// <summary>목록의 방에 참가. done(성공, 실패 이유)</summary>
        public static void Join(RoomListing room, string password, Action<bool, string> done) => _backend.Join(room, password ?? "", done ?? ((_, __) => { }));

        public static void InviteFriends() => _backend.InviteFriends();

        public static void PublishHostedRoom(RoomListing info, string password, bool listed) => _backend.PublishHostedRoom(info, password ?? "", listed);

        public static void CloseHostedRoom() => _backend.CloseHostedRoom();

        sealed class NoBrowsingBackend : IRoomDirectoryBackend
        {
            public bool SupportsBrowsing => false;
            public bool SupportsInvites => false;
            public void Refresh(Action<List<RoomListing>> done) => done(new List<RoomListing>());
            public void Join(RoomListing room, string password, Action<bool, string> done) => done(false, "IP 로 참가하세요.");
            public void InviteFriends() { }
            public void PublishHostedRoom(RoomListing info, string password, bool listed) { }
            public void CloseHostedRoom() { }
        }
    }
}
