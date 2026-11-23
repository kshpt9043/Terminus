// [서버] 이공간: 진행 중에 나간 사람을 기다렸다가 돌아오면 되살림 (언리얼 ADungeonGameMode PreLogin / PostLogin / Logout + RefreshRift)
//  - 손님이 나감: 싸우던 방은 무효(들어가기 전으로), 남은 사람은 이공간 화면, 방을 목록에 다시 띄움 (REJOIN)
//  - 나갔던 사람만 다시 들어올 수 있음 (온라인 아이디로 맞춤). 돌아오면 나갈 때 상태로 배틀러를 다시 만들고 전원 지도로
//  - 방장이 나가면 세션이 끝나니 기다리지 않음
using System.Linq;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Game.Net;
using Unity.Netcode;
using UnityEngine;

namespace Terminus.Game
{
    public sealed partial class DungeonServer
    {
        DungeonRift _rift;

        public bool HasDepartedPlayers => _rift != null && _rift.HasDepartedPlayers;

        void BuildRift()
        {
            _rift = new DungeonRift(_areas, this, () => _players.Values.Where(p => p.bConnected).ToList());
            _rift.RejoinListingChanged += open => NetworkRoot.SetRejoinOpen(open);
            NetworkRoot.CanRejoinMidRun = payload =>
            {
                if (_rift.CanJoin(payload.OnlineId, out var error)) return "";
                return string.IsNullOrEmpty(error) ? "이미 던전에 들어간 방입니다." : error;
            };
            _session.MemberJoinedMidRun += OnMemberJoinedMidRun;
        }

        // 언리얼 Logout
        void OnMemberLeft(ServerSession.Member member)
        {
            if (!_players.TryGetValue(member.ClientId, out var p)) return;
            p.bConnected = false;
            bool guestLeft = member.ClientId != NetworkManager.ServerClientId && !NetworkRoot.IsSinglePlayer;
            _rift.HandlePlayerLeft(p, member.OnlineId, guestLeft, _floor.IsRunEnded);
            // 나간 사람의 배틀러는 다시 들어오면 새로 만듦
            _players.Remove(member.ClientId);
            _floor.RequestPlayerCountRefresh?.Invoke();
            SyncState(true);
        }

        // 언리얼 PostLogin: 이공간 기록에서 상태를 되살리고 배틀러를 다시 만든 뒤 알림
        void OnMemberJoinedMidRun(ServerSession.Member member)
        {
            var restored = _rift.ClaimDeparted(member.OnlineId, member.Run.PlayerName, member.ClientId == NetworkManager.ServerClientId);
            if (restored != null)
            {
                member.Run.LockClassFromSave(restored.CharacterClass);
                member.Run.SetRunState(restored);
            }
            else if (!member.Run.RunState.bStatsInitialized)
            {
                member.Run.BeginRun(GameData.Upgrades);
            }

            var battler = new CombatStats { DebugName = member.Run.PlayerName };
            battler.InitFrom(member.Run.RunState.Stats);
            int saved = member.Run.ConsumeSavedHealth();
            if (saved > 0) battler.Revive(Mathf.Clamp01((float)saved / Mathf.Max(1, battler.Stats.MaxHealth)));
            _players[member.ClientId] = new DungeonPlayer(member.ClientId, member.Run, battler);

            _rift.AnnounceReturn(member.Run.PlayerName);
            _floor.RequestPlayerCountRefresh?.Invoke();
            SyncState(true);
        }
    }
}
