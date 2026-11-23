// 층 진행 (언리얼 AMapManager 의 상태와 규칙: BeginPlay 의 지도 만들기 / 이어하기, HandleBossCleared, AdvanceFloor,
// PickThemeForFloor, 투표 결과 적용, HandleBetrayalResult, EndRun*). 엔진 없음
// 복제 / RPC / 저장 / 채팅 / 층 도착 화면 / 플레이어 체력 회복은 이벤트를 받은 Game 쪽 (NetworkBehaviour) 이 함
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Map
{
    /// <summary>새 층 도착 (AdvanceFloor). 층 도착 화면 / 채팅 / 플레이어 BeginFloor / 퀘스트 처리에 씀</summary>
    public sealed class FloorArrival
    {
        public int Floor = 1;
        public DungeonTheme Theme;
        // 층 도착 화면 (Client_ShowFloorTitle) 제목 "3층" / 부제 "중층 · 리자드의 늪"
        public string Title = "";
        public string Subtitle = "";
        // 시스템 채팅
        public string ChatText = "";
        // 새 테마로 들어감 (퀘스트 방이 있는 층) -> 못 깬 퀘스트는 사라짐 (TerminusQuests::ExpireForNewTheme)
        public bool bNewTheme = false;
    }

    /// <summary>런 끝 (EndRun). Game 쪽: 세이브 삭제 + 사람마다 Client_BeginSettlement(MessageFor, bDeath, AdjustFor, 방 이름)</summary>
    public sealed class RunEndInfo
    {
        public string Reason = "";
        public bool bDeath = false;
        // 사람마다 안내 / 정산 조정 (배신 결과). 없는 사람은 Reason + 빈 조정
        public Dictionary<ulong, RunEndResult> Results = new Dictionary<ulong, RunEndResult>();

        public string MessageFor(ulong playerId) =>
            Results.TryGetValue(playerId, out var r) && !string.IsNullOrEmpty(r.Message) ? r.Message : Reason;

        public SettlementAdjust AdjustFor(ulong playerId) =>
            Results.TryGetValue(playerId, out var r) && r.Adjust != null ? r.Adjust : new SettlementAdjust();
    }

    public sealed class FloorProgression
    {
        public const string MsgEscaped = "던전을 탈출했습니다.";

        public MapSettings Settings { get; }
        public MapGenerator Generator { get; }
        public FloorVote Vote { get; }

        readonly Random _rng;

        // 지도 (복제). 퀘스트 특수 방 넣기(SpecialRoomPlacement)는 이 리스트를 직접 고침 -> 고친 뒤 Game 쪽이 복제 / 런 사본 갱신
        public List<RoomNode> Rooms { get; private set; } = new List<RoomNode>();

        // 지금 층 (1~). 기획: 1~2층 표층 / 3~4층 중층 / 5~6층 심층. 보스방을 깨면 올라감 (복제)
        public int CurrentFloor = 1;

        // 이 층(지도)의 테마. 방에 들어가면 구역에 이 테마의 무대가 뜬다. 계층이 바뀌면 TierThemes 에서 새로 고름
        // 모두에게 복제 (상단바 테마 이름). TierThemes 가 비어 있으면 이 값을 계속 씀 (처음 값 = 레벨에 지정한 것)
        public DungeonTheme FloorTheme;

        // 지도에 쓸 인원 (RefreshPlayerCount 로 갱신)
        public int CurrentPlayerCount;

        // 런이 끝났는가 (정산으로 넘어감). 끝난 뒤 나가는 사람은 이공간으로 안 감
        public bool IsRunEnded { get; private set; }

        /// <summary>
        /// 지도를 새로 만들기 직전에 부름 (언리얼 BeginPlay / AdvanceFloor 안의 RefreshPlayerCount 자리).
        /// Game 쪽이 () => RefreshPlayerCount(주점 인원, 접속 인원) 을 걸어 둠
        /// </summary>
        public Action RequestPlayerCountRefresh;

        /// <summary>지도가 새로 생기거나 다시 불러와짐 (언리얼 OnMapGenerated). Game 쪽: 복제 + 런 사본(SetRooms / SetFloor / SetThemePath)</summary>
        public event Action<List<RoomNode>> MapGenerated;

        /// <summary>
        /// 다음 층 도착 (AdvanceFloor). MapGenerated 보다 먼저 옴. 언리얼 순서대로 Game 쪽이:
        /// 런 사본 저장 -> 전원 BeginFloor(지도 처음 + 체력 회복) -> (bNewTheme) 퀘스트 만료 -> OnFloorReached -> 특수 방 넣기
        /// -> (MapGenerated) -> 층 도착 화면 / 채팅 -> 세이브 (SaveCurrentRun)
        /// </summary>
        public event Action<FloorArrival> FloorAdvanced;

        /// <summary>투표 상태가 바뀜 (복제할 사본)</summary>
        public event Action<FloorVoteState> VoteChanged;

        /// <summary>전원에게 보낼 시스템 채팅 (언리얼 BroadcastChat)</summary>
        public event Action<string> SystemNotice;

        /// <summary>
        /// 투표에서 배신이 나옴 (언리얼 StartBetrayalBattle). Game 쪽:
        /// order = BetrayalRules.BuildBattleOrder(전원, 배신자) -> CanStartBattle 이고 구역이 전투를 열면 채팅(StartNotice),
        /// 아니면 EndRunByEscape(BetrayalRules.MsgNoOpponent). 보정값은 Settings.BetrayalVictimHealthCut / BetrayerStatPerOpponent
        /// </summary>
        public event Action<ulong?> BetrayalRequested;

        /// <summary>런 끝 (탈출 / 전멸 / 배신 결과)</summary>
        public event Action<RunEndInfo> RunEnded;

        public FloorProgression(MapSettings settings, Random rng)
        {
            Settings = settings ?? new MapSettings();
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
            Generator = new MapGenerator(Settings, _rng);
            Vote = new FloorVote(Settings, _rng);
            CurrentPlayerCount = Math.Max(1, Math.Min(FloorRules.MaxPartySize, Settings.CurrentPlayerCount));

            Vote.Changed += s => VoteChanged?.Invoke(s);
            Vote.Applied += ApplyFloorChoice;
        }

        public bool IsFloorVoteActive => Vote.IsActive;

        /// <summary>지도에 쓸 인원: 주점에서 정한 인원과 지금 접속한 인원 중 큰 값. 둘 다 모르면 지금 값</summary>
        public void RefreshPlayerCount(int partySize, int connectedCount)
        {
            CurrentPlayerCount = RoomSelectionRules.ResolvePlayerCount(partySize, connectedCount, CurrentPlayerCount);
        }

        /// <summary>지금 지도의 줄 수 (퀘스트 줄이 없으면 TotalLevels - 1). 지도가 아직 없으면 TotalLevels</summary>
        public int GetLevelCount() => MapGenerator.GetLevelCount(Rooms, Settings.TotalLevels);

        public RoomNode FindRoom(int roomId) => RoomSelectionRules.FindRoom(Rooms, roomId);

        // -------------------------------------------------------------
        // 지도 만들기 / 이어하기 (언리얼 BeginPlay 의 서버 부분)
        // -------------------------------------------------------------

        /// <summary>이번 런에 지도가 아직 없음: 첫 층 테마(계층 후보가 있으면 거기서) + 새 지도</summary>
        public List<RoomNode> StartNewMap()
        {
            RequestPlayerCountRefresh?.Invoke();

            var theme = PickThemeForFloor(CurrentFloor);
            if (theme != null) FloorTheme = theme;

            Rooms = Generator.GenerateMap(CurrentFloor, CurrentPlayerCount);

            // 서버 자신(Listen Server)의 UI 업데이트를 위해 알림
            MapGenerated?.Invoke(Rooms);
            return Rooms;
        }

        /// <summary>
        /// 지도 원본은 런 쪽(언리얼 RunSubsystem, GameInstance)에 있음 -> 전투 갔다 돌아와도 같은 지도.
        /// floor > 0 이면 세이브에서 이어하는 런 / 층을 넘어간 런. savedTheme 이 null 이면 지금 테마 그대로
        /// 돌아온 뒤 Game 쪽은 Rooms 를 런 사본에 다시 써야 함 (OpenSingleRoomRows 로 고쳐졌을 수 있음)
        /// </summary>
        public List<RoomNode> RestoreMap(IEnumerable<RoomNode> savedRooms, int floor, DungeonTheme savedTheme)
        {
            RequestPlayerCountRefresh?.Invoke();

            Rooms = new List<RoomNode>();
            if (savedRooms != null)
            {
                foreach (var r in savedRooms) Rooms.Add(r.Clone());
            }
            MapGenerator.OpenSingleRoomRows(Rooms);   // 예전 지도에 정원 1 인 외길이 있으면 열어 둠

            if (floor > 0) CurrentFloor = floor;
            if (savedTheme != null) FloorTheme = savedTheme;

            MapGenerated?.Invoke(Rooms);
            return Rooms;
        }

        // =====================================================================
        // 층 진행
        // =====================================================================

        /// <summary>이 층의 테마. 같은 계층이면 지금 테마 그대로, 계층이 바뀌면 그 계층 후보 중 랜덤</summary>
        public DungeonTheme PickThemeForFloor(int floor)
        {
            int tier = (Math.Max(1, floor) - 1) / 2;
            int prevTier = (Math.Max(1, floor - 1) - 1) / 2;

            // 같은 계층의 두 번째 층이면 지금 테마 그대로
            if (floor > 1 && tier == prevTier && FloorTheme != null)
            {
                return FloorTheme;
            }

            if (Settings.TierThemes != null && tier >= 0 && tier < Settings.TierThemes.Count)
            {
                var candidates = new List<DungeonTheme>();
                var themes = Settings.TierThemes[tier]?.Themes;
                if (themes != null)
                {
                    foreach (var theme in themes)
                    {
                        if (theme != null) candidates.Add(theme);
                    }
                }
                if (candidates.Count > 0)
                {
                    return candidates[MapGenerator.RandRange(_rng, 0, candidates.Count - 1)];
                }
            }

            // 후보가 없으면 지금 테마 (레벨에 지정한 것)
            return FloorTheme;
        }

        /// <summary>
        /// [서버] 보스방이 끝났을 때 (구역 쪽이 부름)
        /// 테마가 안 바뀌는 층이면 바로 다음 층(true, 저장까지 함). 테마 끝 층이면 행선지 투표 시작(false, 결과가 나올 때까지 저장 안 함)
        /// bSinglePlayerRun = RoomSelectionRules.IsSinglePlayerRun(...), voterCount = 지금 접속한 인원, now = 서버 시각(초)
        /// </summary>
        public bool HandleBossCleared(bool bSinglePlayerRun, int voterCount, double now)
        {
            // 테마가 바뀌는 층(2 / 4 / 6층)에서만 행선지를 고름. 나머지는 바로 다음 층
            if (FloorRules.IsThemeEndFloor(CurrentFloor))
            {
                StartFloorVote(bSinglePlayerRun, voterCount, now);
                return false;
            }

            AdvanceFloor();
            return true;
        }

        /// <summary>[서버] 다음 층으로: 층 +1, (계층이 바뀌면) 새 테마, 새 지도. 전원 지도 처음 / 체력 회복은 FloorAdvanced 를 받은 Game 쪽</summary>
        public void AdvanceFloor()
        {
            ++CurrentFloor;
            var theme = PickThemeForFloor(CurrentFloor);
            if (theme != null) FloorTheme = theme;

            // 새 지도는 지금 인원 기준 (이어하기로 들어와 지도를 새로 안 만든 경우 포함)
            RequestPlayerCountRefresh?.Invoke();

            // 새 지도 (인원은 이 런 그대로)
            Rooms = Generator.GenerateMap(CurrentFloor, CurrentPlayerCount);

            // 층 도착 화면
            string sub = FloorRules.GetTierName(CurrentFloor);
            if (FloorTheme != null && !string.IsNullOrEmpty(FloorTheme.DisplayName))
            {
                sub += $" · {FloorTheme.DisplayName}";
            }

            var arrival = new FloorArrival
            {
                Floor = CurrentFloor,
                Theme = FloorTheme,
                Title = $"{CurrentFloor}층",
                Subtitle = sub,
                ChatText = $"{CurrentFloor}층({sub})에 도착했습니다. 체력이 모두 회복되었습니다.",
                bNewTheme = FloorRules.HasQuestRoom(CurrentFloor),
            };

            // 전원 지도 처음 / 퀘스트 / 특수 방 넣기는 여기서 (MapGenerated 전에)
            FloorAdvanced?.Invoke(arrival);

            // 리슨 서버 자신의 지도 화면 (손님은 Rooms 복제)
            MapGenerated?.Invoke(Rooms);
        }

        // =====================================================================
        // 테마 끝 층 선택 (투표)
        // =====================================================================

        void StartFloorVote(bool bSinglePlayerRun, int voterCount, double now)
        {
            string notice = Vote.Start(CurrentFloor, Settings.LastFloor, bSinglePlayerRun, voterCount, now);
            SystemNotice?.Invoke(notice);
        }

        /// <summary>[서버] 표 넣기 (PC 의 Server_CastFloorVote). 한 사람 한 번, 배신은 선착순 1명. 거절하면 false</summary>
        public bool CastFloorVote(ulong voter, FloorChoice choice, double now) => Vote.Cast(voter, choice, now);

        /// <summary>서버 시각으로 투표 타이머를 돌림 (Game 쪽이 매 프레임)</summary>
        public void Tick(double now) => Vote.Tick(now);

        void ApplyFloorChoice(FloorChoice result)
        {
            switch (result)
            {
                case FloorChoice.NextFloor:
                    AdvanceFloor();
                    break;

                case FloorChoice.Betray:
                    BetrayalRequested?.Invoke(Vote.BetrayerId);
                    break;

                case FloorChoice.Escape:
                default:
                    EndRunByEscape(MsgEscaped);
                    break;
            }
        }

        /// <summary>
        /// [서버] 배신 전투가 끝남 (전투가 부름). 결과대로 각자 정산 조정을 붙여 런을 끝냄 (BetrayalRules.ComputeBetrayalResult)
        /// players = 지금 접속한 전원 (배신자 포함)
        /// </summary>
        public void HandleBetrayalResult(ulong? betrayerId, bool bBetrayerWon, IReadOnlyList<BetrayalParticipant> players,
            Func<string, RelicRow> findRelic = null)
        {
            if (IsRunEnded) return;

            var results = BetrayalRules.ComputeBetrayalResult(betrayerId, bBetrayerWon, players, findRelic);
            EndRun(BetrayalRules.MsgBattleOver, false, results);
        }

        /// <summary>탈출로 런 끝: 세이브 삭제 + 각자 탈출 정산</summary>
        public void EndRunByEscape(string reason) => EndRun(reason, false, null);

        /// <summary>[서버] 전멸 (싱글 사망 / 멀티 전원 사망)로 런 끝. 세이브 삭제 + 사망 정산 (유물만 골드로, 강화 스킬 / 던전 재화 소멸)</summary>
        public void EndRunByDeath(string reason) => EndRun(reason, true, null);

        // 런 끝 공통: 세이브 삭제 + 각자 정산 대기로. results 가 있으면 사람마다 안내 / 정산 조정 (배신 결과)
        void EndRun(string reason, bool bDeath, Dictionary<ulong, RunEndResult> results)
        {
            IsRunEnded = true;

            RunEnded?.Invoke(new RunEndInfo
            {
                Reason = reason ?? "",
                bDeath = bDeath,
                Results = results ?? new Dictionary<ulong, RunEndResult>(),
            });
        }
    }
}
