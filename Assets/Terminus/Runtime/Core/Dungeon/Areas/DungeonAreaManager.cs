// 던전 구역들을 관리 (언리얼 Dungeon/DungeonAreaSubsystem 이식 + AMapManager::EnterSelectedRooms 의 대체 처리)
//
// - 레벨의 구역들을 Game 쪽이 RegisterArea 로 등록 (언리얼은 ADungeonArea::BeginPlay 에서 스스로 등록)
// - 전원 선택이 끝나면 같은 방을 고른 사람끼리 구역 하나에 배정 (StartSelectedRooms)
// - 모든 구역이 끝나야 지도로 돌아가서 다음 선택 (NotifyAreaCleared)
//
// 배정 / 진행은 서버에서만. 복제 / 저장 / 층 진행 / 런 끝은 이벤트를 받은 Game 쪽이 (MapManager -> FloorProgression, SaveSubsystem)
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public sealed class DungeonAreaManager
    {
        public const string MsgPartyWiped = "파티가 전멸했습니다.";
        public const string MsgSoloDied = "쓰러졌습니다.";
        public const string MsgAutoSaved = "진행 상황을 자동 저장했습니다.";

        readonly List<DungeonArea> _areas = new List<DungeonArea>();
        readonly Dictionary<string, DungeonThemeData> _themes = new Dictionary<string, DungeonThemeData>(StringComparer.Ordinal);

        // 플레이어가 지금 들어가 있는 구역 (언리얼 ATerminusPlayerState::CurrentArea. 복제는 PlayerAreaChanged 를 받은 Game 쪽)
        readonly Dictionary<DungeonPlayer, DungeonArea> _currentAreas = new Dictionary<DungeonPlayer, DungeonArea>();

        public DungeonAreaManager(DungeonAreaSettings settings, Random rng, DungeonTimers timers, IAreaNotifier notifier)
        {
            Settings = settings ?? new DungeonAreaSettings();
            Rng = rng ?? throw new ArgumentNullException(nameof(rng));
            Timers = timers ?? new DungeonTimers();
            Notifier = notifier;
        }

        public DungeonAreaSettings Settings { get; }
        public Random Rng { get; }
        public DungeonTimers Timers { get; }
        public IAreaNotifier Notifier { get; }

        // 이 런의 서버 데이터 (이벤트 시드 / 나온 이벤트). 없으면 이벤트는 고정 시드 없이 굴림
        public RunSession Session { get; set; }

        // 지금 층 (언리얼은 레벨의 AMapManager::CurrentFloor 를 찾아 읽음, 없으면 1). Game 쪽: () => FloorProgression.CurrentFloor
        public Func<int> CurrentFloorProvider { get; set; }

        public int CurrentFloor => CurrentFloorProvider != null ? Math.Max(1, CurrentFloorProvider()) : 1;

        // 로그 (언리얼 UE_LOG). 비우면 안 남김
        public Action<string> LogSink { get; set; }

        internal void Log(string text) => LogSink?.Invoke(text);

        // ------------------------------------------------------------ Game 쪽이 받을 것

        /// <summary>플레이어의 지금 구역이 바뀜 (null = 지도로). Game 쪽: 복제 -> 각 클라가 시점 전환 + 배틀러를 자리(AreaSlot)로 / 원래 자리로</summary>
        public event Action<DungeonPlayer, DungeonArea> PlayerAreaChanged;

        /// <summary>구역의 복제할 값(IsInUse / IsCleared / Room / Occupants / CurrentSet)이 바뀜 (등록한 구역 전부의 DungeonArea.Changed)</summary>
        public event Action<DungeonArea> AreaChanged;

        void ForwardAreaChanged(DungeonArea area) => AreaChanged?.Invoke(area);

        /// <summary>
        /// 모든 구역이 끝나고 전원 지도로 돌아옴 (각자 들어갔던 방으로 진행 끝).
        /// true = 보스방이었음 -> FloorProgression.HandleBossCleared (저장은 새 층에 도착했을 때. 투표 중엔 저장 안 함 -> 끄면 보스 직전부터)
        /// false = 자동 저장 (SaveCurrentRun). 저장했으면 MsgAutoSaved 를 채팅으로
        /// </summary>
        public event Action<bool> RoomsFinished;

        /// <summary>다른 방에 살아남은 사람이 없음 -> 런 끝 (FloorProgression.EndRunByDeath(사유): 세이브 삭제 + 사망 정산)</summary>
        public event Action<string> PartyWiped;

        /// <summary>배신 전투 끝 (FloorProgression.HandleBetrayalResult). (배신자, 배신자가 이겼나)</summary>
        public event Action<DungeonPlayer, bool> BetrayalFinished;

        /// <summary>퀘스트 방에서 특수 방을 넣는 퀘스트를 수락함 -> Quests.InjectSpecialRooms 로 지금 지도에 넣고 런 사본 / 복제 갱신</summary>
        public event Action<DungeonPlayer> SpecialRoomInjectionRequested;

        internal void RequestSpecialRoomInjection(DungeonPlayer player) => SpecialRoomInjectionRequested?.Invoke(player);

        // =====================================================================
        // 테마 (언리얼 UDungeonThemeData 에셋 참조 자리)
        // =====================================================================

        public void RegisterTheme(DungeonThemeData data)
        {
            if (data == null || string.IsNullOrEmpty(data.Id)) return;
            _themes[data.Id] = data;
        }

        public DungeonThemeData FindThemeData(DungeonTheme theme)
        {
            if (theme == null || string.IsNullOrEmpty(theme.Id)) return null;
            return _themes.TryGetValue(theme.Id, out var data) ? data : null;
        }

        // =====================================================================
        // 구역 등록 / 조회
        // =====================================================================

        public void RegisterArea(DungeonArea area)
        {
            if (area == null || _areas.Contains(area)) return;

            // 같은 번호가 둘이면 배정 순서가 뒤죽박죽이 됨 -> 레벨 배치 실수 알림
            foreach (var other in _areas)
            {
                if (other.AreaIndex == area.AreaIndex)
                {
                    Log($"[AreaManager] AreaIndex {area.AreaIndex} 가 겹침");
                }
            }

            _areas.Add(area);
            area.Manager = this;
            area.Changed += ForwardAreaChanged;
        }

        public void UnregisterArea(DungeonArea area)
        {
            if (area == null || !_areas.Remove(area)) return;
            area.Changed -= ForwardAreaChanged;
            if (area.Manager == this) area.Manager = null;
        }

        // 레벨에 구역이 하나라도 있는가
        public bool HasAreas => _areas.Count > 0;

        // 구역에서 방이 진행 중인가. 진행 중엔 지도에서 새 방을 고를 수 없음 (RoomSelectRequest.bAnyRoomInProgress)
        public bool IsAnyRoomInProgress
        {
            get
            {
                foreach (var area in _areas)
                {
                    if (area.IsInUse) return true;
                }
                return false;
            }
        }

        // 번호순 정렬된 구역 목록 (안정 정렬: 번호가 같으면 등록 순)
        public List<DungeonArea> GetSortedAreas()
        {
            var result = new List<DungeonArea>(_areas);
            var order = new Dictionary<DungeonArea, int>();
            for (int i = 0; i < _areas.Count; ++i) order[_areas[i]] = i;
            result.Sort((a, b) => a.AreaIndex != b.AreaIndex ? a.AreaIndex.CompareTo(b.AreaIndex) : order[a].CompareTo(order[b]));
            return result;
        }

        // 지금 방이 열려 있는 구역들 (번호순). 관전 바
        public List<DungeonArea> GetActiveAreas()
        {
            var result = GetSortedAreas();
            result.RemoveAll(a => !a.IsInUse);
            return result;
        }

        public DungeonArea GetCurrentArea(DungeonPlayer player)
        {
            return player != null && _currentAreas.TryGetValue(player, out var area) ? area : null;
        }

        internal void SetCurrentArea(DungeonPlayer player, DungeonArea area)
        {
            if (player == null) return;
            if (GetCurrentArea(player) == area) return;

            _currentAreas[player] = area;
            PlayerAreaChanged?.Invoke(player, area);
        }

        // 구역을 비울 때: 이 사람이 아직 그 구역에 있으면 지도로
        internal void ClearCurrentArea(DungeonPlayer player, DungeonArea area)
        {
            if (player == null || GetCurrentArea(player) != area) return;

            _currentAreas.Remove(player);
            PlayerAreaChanged?.Invoke(player, null);
        }

        // =====================================================================
        // 방 시작
        // =====================================================================

        /// <summary>
        /// [서버] 각 플레이어가 고른 방(SelectedRoomId)으로 입장시킨다. 같은 방 = 같은 구역.
        /// theme 은 이 층의 테마. 구역마다 이 테마의 무대를 띄움.
        /// 구역이 없거나 모자라면 아무것도 안 하고 false -> 부른 쪽이 대체 처리 (EnterSelectedRooms)
        /// </summary>
        public bool StartSelectedRooms(IReadOnlyList<DungeonPlayer> players, IReadOnlyList<RoomNode> mapRooms, DungeonTheme theme)
        {
            if (IsAnyRoomInProgress)
            {
                Log("[AreaManager] 이미 진행 중인 방이 있어서 새로 시작 안 함");
                return false;
            }
            if (players == null) return false;

            // 같은 방을 고른 사람끼리 묶기. 순서는 PlayerArray 순 그대로 -> 슬롯도 그 순서로 채워짐
            var roomOrder = new List<int>();
            var playersByRoom = new Dictionary<int, List<DungeonPlayer>>();

            foreach (var p in players)
            {
                if (p?.Run == null || p.Run.RunState.SelectedRoomId == -1) continue;

                int roomId = p.Run.RunState.SelectedRoomId;
                if (!playersByRoom.TryGetValue(roomId, out var list))
                {
                    list = new List<DungeonPlayer>();
                    playersByRoom.Add(roomId, list);
                    roomOrder.Add(roomId);
                }
                list.Add(p);
            }

            if (roomOrder.Count == 0) return false;

            var sortedAreas = GetSortedAreas();
            if (sortedAreas.Count < roomOrder.Count)
            {
                // 일부만 배정하면 나머지 사람이 붕 뜸 -> 전부 아니면 아예 안 함
                Log($"[AreaManager] 구역 부족. 방 {roomOrder.Count}개 필요, 레벨에 구역 {sortedAreas.Count}개");
                return false;
            }

            // 누가 나가서 이번 방이 무효가 되면 여기로 되돌림
            TakeRoomStartSnapshots(players);

            for (int i = 0; i < roomOrder.Count; ++i)
            {
                int roomId = roomOrder[i];
                var room = RoomSelectionRules.FindRoom(mapRooms, roomId);
                if (room != null)
                {
                    sortedAreas[i].BeginRoom(room, playersByRoom[roomId], theme);
                }
            }

            return true;
        }

        /// <summary>
        /// [서버] 고른 방으로 입장 (언리얼 AMapManager::EnterSelectedRooms). 구역에 배정하고,
        /// 구역이 없는(또는 모자란) 레벨이면 콘텐츠 없이 바로 클리어 처리 (구역 배치 전에도 지도 진행은 되게)
        /// </summary>
        public void EnterSelectedRooms(IReadOnlyList<DungeonPlayer> players, IReadOnlyList<RoomNode> mapRooms, DungeonTheme theme)
        {
            // 플레이어마다 다른 방일 수 있어서 서버 전체 이동 대신 한 레벨 안의 구역을 방마다 하나씩 배정한다
            if (HasAreas && StartSelectedRooms(players, mapRooms, theme)) return;
            if (players == null) return;

            foreach (var p in players)
            {
                if (p?.Run == null) continue;

                var room = RoomSelectionRules.FindRoom(mapRooms, p.Run.RunState.SelectedRoomId);
                if (room == null) continue;

                Log($"[Map] 구역 없음 -> {p.Name} 즉시 {room.RoomId}번 방(Row {room.Row}, {room.Type}) 클리어 처리");
                p.Run.AdvanceToRoom(room.RoomId, room.Row);
            }
        }

        // =====================================================================
        // 배신 전투
        // =====================================================================

        /// <summary>[서버] 배신 전투를 첫 번째 구역에서 시작. 구역이 없거나 방이 진행 중이면 false (-> EndRunByEscape(MsgNoOpponent))</summary>
        public bool StartBetrayal(IReadOnlyList<DungeonPlayer> players, DungeonPlayer betrayer, DungeonTheme theme, float victimHealthCut, int statPerOpponent)
        {
            if (IsAnyRoomInProgress) return false;

            var sorted = GetSortedAreas();
            if (sorted.Count == 0 || betrayer == null) return false;

            sorted[0].BeginBetrayal(players, betrayer, theme, victimHealthCut, statPerOpponent);
            return true;
        }

        // 배신 전투가 진행 중인가 (누가 나가도 이공간으로 가지 않음: 끝나면 런이 끝남)
        public bool IsBetrayalInProgress
        {
            get
            {
                foreach (var area in GetActiveAreas())
                {
                    if (area.Combat != null && area.Combat.IsBetrayal) return true;
                }
                return false;
            }
        }

        internal void NotifyBetrayalFinished(DungeonArea area, DungeonPlayer betrayer, bool bBetrayerWon)
        {
            BetrayalFinished?.Invoke(betrayer, bBetrayerWon);
        }

        // =====================================================================
        // 구역 끝 / 전멸
        // =====================================================================

        // [서버] 구역 하나가 끝났을 때 구역이 부름. 전 구역이 끝났으면 전원 진행 + 지도로 복귀
        internal void NotifyAreaCleared(DungeonArea area) => EvaluateAreas();

        // [서버] 구역 하나가 전멸했을 때 구역이 부름. 기획 사망 순서도 (사용자 결정 10-08: 다이어그램대로)
        //  - 다른 구역이 아직 싸우는 중이면 기다림
        //  - 다른 구역에 살아남은 사람이 있으면: 그 사람들이 구출 / 난입 투표
        //      구출: 구출하는 사람 체력 -RescueHealthCost(현재 체력 비율), 전멸한 사람은 체력 1 (보상 없음)
        //      난입: 전멸한 방에 들어가 남은 몬스터와 이어서 싸움 (몬스터 체력 유지, 이기면 보상 + 쓰러진 사람 체력 1)
        //      둘 다 살리러 간 방에 같이 있는 걸로 침 -> 전원 지도 위치가 전멸한 방, 다음 선택도 그 방에서 (사용자 결정 10-08)
        //  - 아무도 없으면 (싱글 사망 / 멀티 전멸): 런 끝 -> 사망 정산 (유물만 골드로, 강화 스킬 / 던전 재화 소멸)
        internal void NotifyAreaWiped(DungeonArea area) => EvaluateAreas();

        void SplitAreas(List<DungeonArea> outSurvivors, List<DungeonArea> outWiped, out bool bOutStillFighting)
        {
            bOutStillFighting = false;
            foreach (var area in GetSortedAreas())
            {
                if (!area.IsInUse) continue;

                if (area.IsWiped) outWiped.Add(area);
                else if (area.IsCleared) outSurvivors.Add(area);
                else bOutStillFighting = true;   // 싸우는 중 / 보상 / 휴식 / 이벤트 고르는 중
            }
        }

        void EvaluateAreas()
        {
            if (_bRescueVoting) return;

            var survivors = new List<DungeonArea>();
            var wiped = new List<DungeonArea>();
            SplitAreas(survivors, wiped, out bool bStillFighting);

            // 아직 진행 중인 구역이 있으면 대기 (그 구역도 끝나야 구출 / 난입을 고를 수 있음)
            // 먼저 끝난 구역 사람은 관전 (CanSpectate)
            if (bStillFighting) return;

            if (wiped.Count == 0)
            {
                FinishAllRooms();
                return;
            }

            if (survivors.Count > 0)
            {
                StartRescueVote(survivors, wiped);
                return;
            }

            // 다른 방에 아무도 없음 -> 전멸. 런 끝 (세이브 삭제 + 사망 정산)
            int playerCount = 0;
            foreach (var area in wiped) playerCount += area.Occupants.Count;

            Log("[AreaManager] 전멸. 런 끝");
            PartyWiped?.Invoke(playerCount > 1 ? MsgPartyWiped : MsgSoloDied);
        }

        // 모든 구역을 닫고 각자 들어갔던 방으로 지도상 위치를 진행시킴
        void FinishAllRooms()
        {
            Log("[AreaManager] 모든 구역 종료. 지도로 복귀");

            bool bBossCleared = false;
            foreach (var area in GetSortedAreas())
            {
                if (!area.IsInUse) continue;

                // 지도상 위치를 들어갔던 방으로 진행 (선택도 여기서 비워짐)
                var room = area.Room;
                bBossCleared |= room.Type == RoomType.BOSS;
                foreach (var p in area.Occupants)
                {
                    p.Run?.AdvanceToRoom(room.RoomId, room.Row);
                }

                area.Release();
            }

            // 방이 정상으로 끝났으니 롤백할 일 없음
            _roomStartSnapshots.Clear();

            // 보스방이면 층 진행 / 행선지 투표. 아니면 자동 저장: 방이 끝나고 전원이 지도로 돌아온 지금 (보상까지 받은 뒤)
            RoomsFinished?.Invoke(bBossCleared);
        }

        // =====================================================================
        // 구출 / 난입 투표
        // =====================================================================

        bool _bRescueVoting;
        bool _bRescueCanIntervene;
        readonly List<DungeonPlayer> _rescueVoters = new List<DungeonPlayer>();
        readonly Dictionary<DungeonPlayer, bool> _rescueVotes = new Dictionary<DungeonPlayer, bool>();
        readonly TimerHandle _rescueTimer = new TimerHandle();

        public bool IsRescueVoting => _bRescueVoting;
        public bool RescueCanIntervene => _bRescueCanIntervene;
        public IReadOnlyList<DungeonPlayer> RescueVoters => _rescueVoters;

        void StartRescueVote(List<DungeonArea> survivorAreas, List<DungeonArea> wipedAreas)
        {
            _bRescueVoting = true;
            _bRescueCanIntervene = wipedAreas.Count == 1;   // 전멸한 방이 둘 이상이면 구출만 (한 번에 한 방에만 들어갈 수 있어서)
            _rescueVoters.Clear();
            _rescueVotes.Clear();

            var wipedNames = new List<string>();
            foreach (var area in wipedAreas)
            {
                foreach (var p in area.Occupants) wipedNames.Add(p.Name);
            }

            float cost = Settings.RescueHealthCost;
            float seconds = Settings.RescueVoteSeconds;
            foreach (var area in survivorAreas)
            {
                foreach (var p in area.Occupants)
                {
                    _rescueVoters.Add(p);
                    Notifier?.ShowRescue(p, true, wipedNames, _bRescueCanIntervene, cost, seconds);
                }
            }

            foreach (var area in wipedAreas)
            {
                foreach (var p in area.Occupants)
                {
                    Notifier?.ShowRescue(p, false, wipedNames, _bRescueCanIntervene, cost, seconds);
                }
            }

            Notifier?.BroadcastSystemChat($"{string.Join(", ", wipedNames)} 님이 쓰러졌습니다. 다른 방의 동료가 구출 / 난입을 고릅니다.");

            Timers.Set(_rescueTimer, seconds, ResolveRescueVote);
        }

        /// <summary>[서버] 구출 / 난입 투표 (PC 의 Server_ChooseRescue). 살아남은 사람만, 한 번</summary>
        public void HandleRescueChoice(DungeonPlayer voter, bool bIntervene)
        {
            if (!_bRescueVoting || voter == null || !_rescueVoters.Contains(voter) || _rescueVotes.ContainsKey(voter)) return;
            if (bIntervene && !_bRescueCanIntervene) return;

            _rescueVotes.Add(voter, bIntervene);

            foreach (var p in _rescueVoters)
            {
                if (!_rescueVotes.ContainsKey(p)) return;
            }
            ResolveRescueVote();
        }

        void CancelRescueVote()
        {
            if (!_bRescueVoting) return;

            _bRescueVoting = false;
            Timers.Clear(_rescueTimer);
            _rescueVoters.Clear();
            _rescueVotes.Clear();

            foreach (var area in GetSortedAreas())
            {
                foreach (var p in area.Occupants) Notifier?.CloseRescue(p);
            }
        }

        void ResolveRescueVote()
        {
            if (!_bRescueVoting) return;

            // 다수결, 동점이면 랜덤, 아무도 안 골랐으면 구출
            int rescue = 0, intervene = 0;
            foreach (var pair in _rescueVotes)
            {
                if (pair.Value) ++intervene;
                else ++rescue;
            }
            bool bIntervene = _bRescueCanIntervene && (intervene > rescue || (intervene == rescue && intervene > 0 && Rng.Next(0, 2) == 1));

            CancelRescueVote();   // 투표 화면 닫기

            var survivors = new List<DungeonArea>();
            var wiped = new List<DungeonArea>();
            SplitAreas(survivors, wiped, out _);
            if (survivors.Count == 0 || wiped.Count == 0)
            {
                EvaluateAreas();
                return;
            }

            Notifier?.BroadcastSystemChat(bIntervene
                ? $"난입! 전멸한 방에 들어가 남은 몬스터와 싸웁니다. (구출 {rescue} / 난입 {intervene})"
                : $"구출! 쓰러진 동료를 일으켰습니다. 다음 방은 동료가 쓰러진 방에서 고릅니다. (구출 {rescue} / 난입 {intervene})");

            if (bIntervene) ApplyIntervention(survivors, wiped[0]);
            else ApplyRescue(survivors, wiped);
        }

        void ApplyRescue(List<DungeonArea> survivorAreas, List<DungeonArea> wipedAreas)
        {
            // 살리러 간 방 (전멸한 방). 구출한 사람도 그 방에 같이 있는 걸로 침 -> 다음 선택은 그 방에서
            var targetRoom = wipedAreas[0].Room.Clone();

            // 구출하는 사람: 현재 체력의 RescueHealthCost 만큼 잃음 (1 아래로는 안 내려감)
            foreach (var area in survivorAreas)
            {
                foreach (var p in area.Occupants)
                {
                    var stats = p.Battler;
                    if (stats != null && !stats.IsDead())
                    {
                        int maxHealth = Math.Max(1, stats.Stats.MaxHealth);
                        int health = stats.State.Health;
                        int newHealth = Math.Max(1, health - CombatMath.RoundToInt(health * Settings.RescueHealthCost));
                        stats.Revive((float)newHealth / maxHealth);
                    }
                    p.Run?.AdvanceToRoom(targetRoom.RoomId, targetRoom.Row);
                }
                area.Release();
            }

            // 전멸한 사람: 체력 1. 보상 없음
            foreach (var area in wipedAreas)
            {
                foreach (var p in area.Occupants)
                {
                    p.Battler?.Revive(0f);   // 최소 1
                    p.Run?.AdvanceToRoom(targetRoom.RoomId, targetRoom.Row);
                }
                area.Release();
            }

            // 열린 구역이 없으니 지도 복귀 + 자동 저장만
            FinishAllRooms();
        }

        void ApplyIntervention(List<DungeonArea> survivorAreas, DungeonArea wipedArea)
        {
            // 살아남은 사람들을 자기 구역에서 빼서 (지도 자리로 돌아온 뒤) 전멸한 구역으로
            var joiners = new List<DungeonPlayer>();
            foreach (var area in survivorAreas)
            {
                joiners.AddRange(area.Occupants);
                area.Release();
            }

            // 이기면 그 구역이 클리어 -> 전원 그 방(살리러 간 방)으로 진행. 지면 다시 전멸 -> 아무도 없으니 런 끝
            wipedArea.BeginIntervention(joiners);
        }

        // =====================================================================
        // 나간 사람 / 강제 종료
        // =====================================================================

        /// <summary>
        /// [서버] 누가 게임을 나갔을 때 (멀티). 끝나면 모든 구역이 닫혀 있음 (전원 이공간으로 가기 전 정리)
        ///  - 아직 싸우는 구역이 있으면: 이번 방은 무효 (롤백). 전원 방에 들어가기 직전 상태로 되돌림
        ///  - 모든 구역이 이미 이겼으면(보상 중): 결과는 인정하고 평소처럼 진행. 아직 안 고른 보상은 건너뜀
        /// </summary>
        public void HandlePlayerLeft(DungeonPlayer leaver)
        {
            // 배신 전투 중: 나간 사람은 진 걸로 (배신자면 팀 승리). 방 롤백은 안 함
            foreach (var area in GetActiveAreas())
            {
                if (area.Combat != null && area.Combat.IsBetrayal)
                {
                    area.Combat.HandleBetrayalLeaver(leaver);
                    return;
                }
            }

            bool bAnyInUse = false;
            bool bAllFightsOver = true;
            foreach (var area in GetSortedAreas())
            {
                if (!area.IsInUse) continue;
                bAnyInUse = true;
                bAllFightsOver &= area.IsFightOver();
            }

            if (!bAnyInUse) return;

            // 구출 / 난입 투표 중이었으면 닫음 (아래에서 방을 무효로 돌림)
            CancelRescueVote();

            if (bAllFightsOver)
            {
                // 이미 이긴 방: 결과 인정. 나간 사람도 같이 진행 (보상 화면은 닫히고 안 고른 스킬은 건너뜀)
                Log($"[AreaManager] {leaver?.Name ?? "?"} 나감. 모든 방이 끝난 뒤라 결과를 인정하고 마무리");
                FinishAllRooms();
                return;
            }

            // 싸우는 중: 이번 방은 무효. 모든 구역을 닫고 방에 들어가기 전으로 (나간 사람 포함)
            Log($"[AreaManager] {leaver?.Name ?? "?"} 나감. 진행 중인 방을 무효로 하고 들어가기 전으로 되돌림");

            foreach (var area in GetSortedAreas())
            {
                if (area.IsInUse) area.Release();
            }

            RollbackToRoomStart();
        }

        /// <summary>[서버 / 테스트] 진행 중인 방을 전부 닫음. 지도 위치 그대로, 고른 방만 비움 (보상 / 진행 없음)</summary>
        public void AbortAllRooms()
        {
            CancelRescueVote();

            foreach (var area in GetSortedAreas())
            {
                if (!area.IsInUse) continue;

                foreach (var p in area.Occupants)
                {
                    p.Run?.SetSelectedRoomId(-1);
                }
                area.Release();
            }
            _roomStartSnapshots.Clear();
        }

        // ------------------------------------------------------------ 롤백

        // 방에 들어가기 직전 상태 (롤백용). 방이 정상으로 끝나면 비움
        sealed class RoomStartSnapshot
        {
            public RunState RunState;
            public CharacterStats BattlerStats;   // 배틀러가 실제로 쓰던 스텟 (배틀러가 없으면 런 스텟)
            public int Health = -1;
        }

        readonly Dictionary<DungeonPlayer, RoomStartSnapshot> _roomStartSnapshots = new Dictionary<DungeonPlayer, RoomStartSnapshot>();

        public bool HasRoomStartSnapshot(DungeonPlayer p) => p != null && _roomStartSnapshots.ContainsKey(p);

        void TakeRoomStartSnapshots(IReadOnlyList<DungeonPlayer> players)
        {
            _roomStartSnapshots.Clear();

            foreach (var p in players)
            {
                if (p?.Run == null) continue;

                var snapshot = new RoomStartSnapshot { RunState = p.Run.RunState.Clone() };
                if (p.Battler != null)
                {
                    snapshot.BattlerStats = p.Battler.Stats.Clone();
                    snapshot.Health = p.Battler.State.Health;
                }
                else
                {
                    snapshot.BattlerStats = snapshot.RunState.Stats.Clone();
                }
                _roomStartSnapshots[p] = snapshot;
            }
        }

        void RollbackToRoomStart()
        {
            foreach (var pair in _roomStartSnapshots)
            {
                var p = pair.Key;
                var snapshot = pair.Value;
                if (p?.Run == null) continue;

                // 런 상태 (재화 / 유물 / 강화 스킬 / 최대 체력 / 지도 위치). 고른 방은 비움
                var restored = snapshot.RunState.Clone();
                restored.SelectedRoomId = -1;
                p.Run.SetRunState(restored);

                // 배틀러: 스텟 / 체력 / 에너지 / 상태 효과 모두 들어가기 전으로
                var stats = p.Battler;
                if (stats != null)
                {
                    stats.InitFrom(snapshot.BattlerStats);
                    int maxHealth = snapshot.BattlerStats.MaxHealth;
                    if (snapshot.Health > 0 && maxHealth > 0 && snapshot.Health < maxHealth)
                    {
                        stats.Revive((float)snapshot.Health / maxHealth);
                    }
                }

                Log($"[AreaManager] {p.Name} 롤백 (체력 {snapshot.Health})");
            }

            _roomStartSnapshots.Clear();
        }

        // =====================================================================
        // 관전 (언리얼 USpectateWidget::Refresh 의 판단 부분)
        // =====================================================================

        /// <summary>
        /// 이 사람이 볼 수 있는 다른 구역 (진행 중인 구역 중 내 것 빼고). 내가 아직 싸우는 중이면 빈 목록
        /// (싸우는 중 = 내 구역이 안 끝났고, 내가 살아 있고, 턴이 도는 전투)
        /// </summary>
        public List<DungeonArea> GetSpectatableAreas(DungeonPlayer player)
        {
            var myArea = GetCurrentArea(player);
            bool bMeAlive = player?.Battler != null && !player.Battler.IsDead();
            bool bFighting = myArea != null && !myArea.IsCleared && bMeAlive
                && myArea.Combat != null && myArea.Combat.IsTurnInProgress;

            var others = new List<DungeonArea>();
            if (bFighting) return others;

            foreach (var area in GetActiveAreas())
            {
                if (area != myArea) others.Add(area);
            }
            return others;
        }

        public bool CanSpectate(DungeonPlayer player) => GetSpectatableAreas(player).Count > 0;

        // 관전 버튼 글: "이름, 이름 (몬스터)"
        public static string GetAreaLabel(DungeonArea area)
        {
            if (area == null) return "";
            var names = new List<string>();
            foreach (var p in area.Occupants) names.Add(p.Name);
            return $"{string.Join(", ", names)} ({RoomTypeLabel(area.Room.Type)})";
        }

        // 언리얼 TerminusUI::RoomTypeLabel
        public static string RoomTypeLabel(RoomType type)
        {
            switch (type)
            {
                case RoomType.MONSTER: return "몬스터";
                case RoomType.GUARDIAN: return "가디언";
                case RoomType.BOSS: return "보스";
                case RoomType.BREAK: return "휴식터";
                case RoomType.EVENT: return "이벤트";
                case RoomType.STORE: return "상점";
                case RoomType.QUEST: return "퀘스트";
                default: return "방";
            }
        }

        // =====================================================================
        // 요청 전달 (언리얼 PC 의 Server_* 가 PS->GetCurrentArea() 로 넘기던 것). 요청자는 서버가 연결에서 꺼낸 사람이어야 함
        // =====================================================================

        public void HandleRestChoice(DungeonPlayer chooser, DungeonPlayer target) => GetCurrentArea(chooser)?.HandleRestChoice(chooser, target);
        public void HandleRestExplore(DungeonPlayer chooser) => GetCurrentArea(chooser)?.HandleRestExplore(chooser);
        public void HandleEventChoice(DungeonPlayer chooser, int index, int replaceSlot, string relicRow) => GetCurrentArea(chooser)?.HandleEventChoice(chooser, index, replaceSlot, relicRow);
        public void HandleShopBuyRelic(DungeonPlayer p, int index) => GetCurrentArea(p)?.HandleShopBuyRelic(p, index);
        public void HandleShopBuySkill(DungeonPlayer p, int index, int replaceSlot) => GetCurrentArea(p)?.HandleShopBuySkill(p, index, replaceSlot);
        public void HandleShopSellRelic(DungeonPlayer p, string relicRow) => GetCurrentArea(p)?.HandleShopSellRelic(p, relicRow);
        public void HandleShopUpgradeSkill(DungeonPlayer p, string skillRow) => GetCurrentArea(p)?.HandleShopUpgradeSkill(p, skillRow);
        public void HandleShopLeave(DungeonPlayer p) => GetCurrentArea(p)?.HandleShopLeave(p);
    }
}
