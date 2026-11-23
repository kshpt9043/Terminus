// 이공간 (언리얼 Game/DungeonGameMode 의 나간 사람 / 돌아온 사람 규칙: InitGame / PreLogin / PostLogin / Logout / RefreshRift)
// 멀티에서 진행 중에 누가 나가면 남은 사람은 전원 이공간으로 (싸우던 방은 무효 = 들어가기 전으로 롤백)
// 세션을 주점 목록에 다시 띄우고, 나간 사람이 그걸 찾아 돌아오면 전원 지도로
// 접속 / 세션 목록 / 배틀러 스폰 / 자리 배정(ChoosePlayerStart)은 Game 쪽. 여기는 누구를 기다리는지와 그때 상태만
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Dungeon
{
    // 진행 중에 나간 플레이어. 돌아오면 이 상태로 되살림 (언리얼 FDepartedPlayer)
    [Serializable]
    public sealed class DepartedPlayer
    {
        public string PlayerId = "";       // 온라인 아이디 (스팀 ID)
        public string PlayerName = "";
        public RunState RunState = new RunState();   // 나갈 때 상태. SavedHealth = 그때 체력

        public DepartedPlayer Clone()
        {
            var c = (DepartedPlayer)MemberwiseClone();
            c.RunState = RunState != null ? RunState.Clone() : new RunState();
            return c;
        }
    }

    public sealed class DungeonRift
    {
        public const string MsgAlreadyInProgress = "이미 던전이 진행 중입니다.";

        readonly DungeonAreaManager _areas;
        readonly IAreaNotifier _notifier;
        readonly Func<IReadOnlyList<DungeonPlayer>> _connectedPlayers;
        readonly List<DepartedPlayer> _departed = new List<DepartedPlayer>();

        /// <param name="areas">구역 (나가면 진행 중인 방을 정리)</param>
        /// <param name="connectedPlayers">지금 접속한 사람 전원 (이공간 화면을 보낼 대상. 언리얼 PlayerControllerIterator)</param>
        public DungeonRift(DungeonAreaManager areas, IAreaNotifier notifier, Func<IReadOnlyList<DungeonPlayer>> connectedPlayers)
        {
            _areas = areas;
            _notifier = notifier;
            _connectedPlayers = connectedPlayers;
        }

        // PIE 처럼 아이디가 매번 바뀌는 환경: 아이디가 안 맞아도 이름 -> 아무 자리 (언리얼 UseLooseRejoinMatching)
        public bool bLooseRejoinMatching = false;

        // PIE 는 심리스 트래블이 막혀서 클라가 재접속으로 넘어옴 -> 막으면 테스트 불가 (언리얼 PreLogin 의 PIE 예외)
        public bool bAllowAnyJoin = false;

        /// <summary>이공간이 열렸는가가 바뀜 -> Game 쪽: 세션을 주점 목록에 다시 띄우거나 내림 (SetRejoinListing)</summary>
        public event Action<bool> RejoinListingChanged;

        // 멀티 세이브를 불러와 모이는 중 (전원 모이면 false). 이공간 / 채팅 문구만 다름
        public bool IsGatheringFromSave { get; private set; }

        public IReadOnlyList<DepartedPlayer> DepartedPlayers => _departed;

        // 나갔다가 아직 안 돌아온 사람이 있는가 (= 이공간). 있으면 방을 고를 수 없음 (RoomSelectRequest.bHasDepartedPlayers)
        public bool HasDepartedPlayers => _departed.Count > 0;

        public List<string> GetDepartedNames()
        {
            var names = new List<string>();
            foreach (var d in _departed) names.Add(d.PlayerName);
            return names;
        }

        /// <summary>
        /// 멀티 세이브 이어하기 (언리얼 InitGame). 세이브 플레이어 전원을 '아직 안 온 사람'으로 둠 (= 이공간).
        /// 지도 / 층 / 슬롯은 RunSession.BeginLoadedRun 이 (Game 쪽). 싱글 세이브면 아무것도 안 함
        /// </summary>
        public void BeginFromSave(RunSave save)
        {
            if (save == null || save.Summary == null || !save.Summary.bMultiplayer) return;

            // 들어오는 대로 세이브 당시 상태로 (직업도 그대로)
            if (save.Players != null)
            {
                foreach (var player in save.Players)
                {
                    if (player == null) continue;
                    var waiting = new DepartedPlayer
                    {
                        PlayerId = player.PlayerId ?? "",
                        PlayerName = player.PlayerName ?? "",
                        RunState = player.RunState != null ? player.RunState.Clone() : new RunState(),
                    };
                    waiting.RunState.SelectedRoomId = -1;
                    _departed.Add(waiting);
                }
            }
            IsGatheringFromSave = _departed.Count > 0;
        }

        /// <summary>
        /// 들어와도 되나 (언리얼 PreLogin). 던전엔 주점에서 같이 넘어온 사람만 (심리스로 온 사람은 여기를 안 탐).
        /// 진행 중에 나갔던 사람 / 세이브 플레이어는 들어올 수 있음
        /// </summary>
        public bool CanJoin(string playerId, out string error)
        {
            error = "";
            if (bAllowAnyJoin) return true;
            if (FindDeparted(playerId ?? "", "") >= 0) return true;

            error = MsgAlreadyInProgress;
            return false;
        }

        // 돌아온 사람의 기록. 아이디가 같은 것 (PIE 는 아이디가 매번 바뀌어서 이름 -> 아무거나). 없으면 -1
        // bLoose = 아이디가 안 맞아도 이름 -> 아무 자리 (방장 자신 등)
        public int FindDeparted(string playerId, string playerName, bool bLoose = false)
        {
            if (!string.IsNullOrEmpty(playerId))
            {
                for (int i = 0; i < _departed.Count; ++i)
                {
                    if (_departed[i].PlayerId == playerId) return i;
                }
            }

            if (!bLoose && !bLooseRejoinMatching) return -1;

            if (!string.IsNullOrEmpty(playerName))
            {
                for (int i = 0; i < _departed.Count; ++i)
                {
                    if (_departed[i].PlayerName == playerName) return i;
                }
            }
            return _departed.Count > 0 ? 0 : -1;
        }

        /// <summary>
        /// 나갔던 사람이 돌아옴 (언리얼 PostLogin 앞부분). 기다리던 사람이면 나갈 때 상태(사본, 고른 방 비움)를 돌려주고 목록에서 뺌.
        /// Game 쪽: 배틀러가 생기기 전에 RunPlayer.SetRunState(돌려받은 것) -> 스폰 -> AnnounceReturn
        /// bLocal = 방장 자신 (세이브를 만든 사람 -> 아이디가 달라도 자리를 줌: 계정을 바꿨거나 PIE)
        /// </summary>
        public RunState ClaimDeparted(string playerId, string playerName, bool bLocal)
        {
            if (_departed.Count == 0) return null;

            int index = FindDeparted(playerId ?? "", playerName ?? "", bLocal);
            if (index < 0) return null;

            var restored = _departed[index].RunState.Clone();
            restored.SelectedRoomId = -1;
            _departed.RemoveAt(index);
            return restored;
        }

        /// <summary>돌아온 사람 알림 + 이공간 갱신 (언리얼 PostLogin 뒷부분). 배틀러가 생긴 뒤 (접속 목록에 들어간 뒤) 부를 것</summary>
        public void AnnounceReturn(string playerName)
        {
            string verb = IsGatheringFromSave ? "합류했습니다" : "돌아왔습니다";
            _notifier?.BroadcastSystemChat(_departed.Count == 0
                ? $"{playerName} 님이 {verb}. 이공간을 빠져나와 지도로 갑니다."
                : $"{playerName} 님이 {verb}. 아직 기다리는 중: {string.Join(", ", GetDepartedNames())}");

            if (_departed.Count == 0)
            {
                IsGatheringFromSave = false;
            }

            // 다 돌아왔으면 전원 지도로. 아직 남았으면 돌아온 사람도 이공간에
            RefreshRift(null);
        }

        /// <summary>
        /// 누가 나감 (언리얼 Logout).
        /// bGuestLeft = 손님이 나감 (방장 자신이 아니고, 월드를 닫는 중이 아니고, 멀티) -> 방장이 나가면 세션이 끝나니 기다릴 것 없음.
        /// bRunEnded = 런이 이미 끝남 (FloorProgression.IsRunEnded). onlineId = 돌아올 때 맞춰 볼 온라인 아이디
        /// Game 쪽: 이 뒤에 그 사람 자리(PlayerStart)를 비워 둠 (돌아오면 다시 받게)
        /// </summary>
        public void HandlePlayerLeft(DungeonPlayer leaver, string onlineId, bool bGuestLeft, bool bRunEnded)
        {
            // 배신 전투 중이거나 런이 이미 끝났으면(정산으로 넘어감) 이공간으로 가지 않음. 배신 전투는 나간 사람이 진 걸로
            if (bGuestLeft && (bRunEnded || (_areas != null && _areas.IsBetrayalInProgress)))
            {
                _areas?.HandlePlayerLeft(leaver);
                return;
            }

            if (bGuestLeft && leaver != null)
            {
                // 1. 진행 중인 방 정리. 싸우는 중이면 이번 방은 무효 (전원 들어가기 전으로 롤백)
                _areas?.HandlePlayerLeft(leaver);

                // 2. 돌아오면 되살릴 상태 (롤백 / 진행이 반영된 뒤)
                var departed = new DepartedPlayer
                {
                    PlayerId = onlineId ?? "",
                    PlayerName = leaver.Name,
                    RunState = leaver.Run != null ? leaver.Run.RunState.Clone() : new RunState(),
                };
                departed.RunState.SelectedRoomId = -1;
                if (leaver.Battler != null)
                {
                    departed.RunState.SavedHealth = Math.Max(1, leaver.Battler.State.Health);
                }
                _departed.Add(departed);

                // 4. 남은 사람은 이공간으로, 세션은 주점 목록에 다시 띄움 (나간 사람이 찾아 돌아오게)
                RefreshRift(leaver);

                _notifier?.BroadcastSystemChat($"{leaver.Name} 님이 나갔습니다. 돌아올 때까지 이공간에서 기다립니다.");
            }
            else if (leaver != null)
            {
                _notifier?.BroadcastSystemChat($"{leaver.Name} 님이 나갔습니다.");
            }
        }

        // 이공간 상태를 맞춤: 기다리는 사람이 있으면 세션을 목록에 띄우고 전원 이공간 화면, 없으면 닫고 전원 지도로
        // skip = 지금 나가는 사람 (그쪽엔 안 보냄)
        void RefreshRift(DungeonPlayer skip)
        {
            bool bOpen = _departed.Count > 0;
            RejoinListingChanged?.Invoke(bOpen);

            var waiting = GetDepartedNames();
            var players = _connectedPlayers != null ? _connectedPlayers() : null;
            if (players == null || _notifier == null) return;

            foreach (var p in players)
            {
                if (p == null || p == skip) continue;

                if (bOpen) _notifier.EnterRift(p, waiting, IsGatheringFromSave);
                else _notifier.LeaveRift(p);
            }
        }
    }
}
