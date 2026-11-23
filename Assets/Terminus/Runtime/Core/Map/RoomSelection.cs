// 지도에서 방 고르기 규칙 (언리얼 AMapManager::HandleSelectRoomRequest, IsValidNextRoom, IsSinglePlayerRun,
// CheckAllPlayersReadyAndStart, RefreshPlayerCount 의 판단 부분 + ATerminusPlayerState::AdvanceToRoom / BeginFloor 의 RunState 부분)
// 판단만 한다. 실제로 SelectedRoomId 를 바꾸고 / 실패 메시지를 클라에 보내고 / 방에 들여보내는 건 Game 쪽
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Map
{
    public enum RoomSelectOutcome
    {
        Ignored,     // 그런 방이 없음. 아무것도 안 함 (언리얼도 조용히 무시)
        Rejected,    // 거절. FailMessage 를 요청자에게 (Client_OnRoomSelectFailed)
        EnterNow,    // 싱글: SelectedRoomId = RoomId 로 두고 바로 입장 (EnterSelectedRooms({요청자}))
        Deselected,  // 같은 방을 다시 누름: SelectedRoomId = -1
        Selected,    // 선택 완료: SelectedRoomId = RoomId 로 두고 CheckAllPlayersReady
    }

    public sealed class RoomSelectResult
    {
        public RoomSelectOutcome Outcome = RoomSelectOutcome.Ignored;
        public string FailMessage = "";
        // 요청자의 RunState.SelectedRoomId 를 이 값으로 (Ignored / Rejected 면 그대로 둠)
        public int NewSelectedRoomId = -1;

        public bool ChangesSelection => Outcome == RoomSelectOutcome.EnterNow || Outcome == RoomSelectOutcome.Deselected || Outcome == RoomSelectOutcome.Selected;

        internal static RoomSelectResult Reject(string message) =>
            new RoomSelectResult { Outcome = RoomSelectOutcome.Rejected, FailMessage = message };
    }

    /// <summary>방 선택 요청 하나. 게임 상태 중 Core 가 모르는 것(구역 진행 / 나간 사람)은 Game 쪽이 채워 넘김</summary>
    public sealed class RoomSelectRequest
    {
        public int RoomId = -1;
        // 요청자 RunState. 요청자는 서버가 연결(PC)에서 직접 꺼낸 것이어야 함 (클라가 남의 것을 넘겨 조작하는 걸 막기 위해)
        public RunState RequesterRun;
        // 나를 뺀 나머지 플레이어들이 고른 방 (SelectedRoomId, -1 = 안 고름)
        public List<int> OtherSelectedRoomIds = new List<int>();

        public bool bFloorVoteActive = false;      // 행선지 투표 중
        public bool bHasDepartedPlayers = false;   // 이공간 (누가 나가서 돌아오길 기다리는 중) - DungeonGameMode
        public bool bAnyRoomInProgress = false;    // 구역에서 방이 진행 중 - DungeonAreaSubsystem
        public bool bSinglePlayerRun = false;      // RoomSelectionRules.IsSinglePlayerRun
    }

    public enum PartyReadyResult
    {
        NotReady,            // 아직 안 고른 사람이 있음
        WaitingForDeparted,  // 전원 골랐지만 나간 사람이 있음 -> Notice 를 채팅으로 알리고 대기
        Start,               // 각자 고른 방으로 입장 (EnterSelectedRooms(전원))
    }

    public static class RoomSelectionRules
    {
        public const string MsgStartChoicesRequired = "시작 강화 스킬과 유물을 먼저 골라야 합니다.";
        public const string MsgFloorVoteActive = "다음 행선지를 고르는 중입니다.";
        public const string MsgDepartedPlayers = "나간 플레이어가 돌아올 때까지 방을 고를 수 없습니다.";
        public const string MsgRoomInProgress = "방을 진행하는 중에는 다음 방을 고를 수 없습니다.";
        public const string MsgInvalidPath = "이동할 수 없는 경로의 방입니다.";
        public const string MsgRoomFull = "선택한 방의 정원이 가득 찼습니다!";

        public static RoomNode FindRoom(IReadOnlyList<RoomNode> rooms, int roomId)
        {
            if (rooms == null) return null;
            foreach (var node in rooms)
            {
                if (node.RoomId == roomId) return node;
            }
            return null;
        }

        /// <summary>방 선택 요청 처리 (서버). 언리얼 HandleSelectRoomRequest 의 검사 순서 그대로</summary>
        public static RoomSelectResult EvaluateSelectRoom(IReadOnlyList<RoomNode> rooms, RoomSelectRequest req)
        {
            if (req == null || req.RequesterRun == null) return new RoomSelectResult();

            var targetRoom = FindRoom(rooms, req.RoomId);
            if (targetRoom == null) return new RoomSelectResult();

            var run = req.RequesterRun;

            // 런 시작 강화 스킬을 아직 안 골랐으면 출발 불가 (고르는 화면이 지도를 덮고 있지만 서버에서도 막음)
            if (!run.bStartSkillChosen || !run.bStartRelicsChosen)
            {
                return RoomSelectResult.Reject(MsgStartChoicesRequired);
            }

            // 행선지 투표 중엔 방을 못 고름
            if (req.bFloorVoteActive)
            {
                return RoomSelectResult.Reject(MsgFloorVoteActive);
            }

            // 이공간(누가 나가서 돌아오길 기다리는 중)에선 방을 못 고름
            if (req.bHasDepartedPlayers)
            {
                return RoomSelectResult.Reject(MsgDepartedPlayers);
            }

            // 0. 구역에서 방이 진행 중이면 지도 선택 불가 (전 구역이 끝나야 다음 선택)
            if (req.bAnyRoomInProgress)
            {
                return RoomSelectResult.Reject(MsgRoomInProgress);
            }

            // 1. 유효 노드 검증
            if (!IsValidNextRoom(rooms, run, targetRoom))
            {
                return RoomSelectResult.Reject(MsgInvalidPath);
            }

            // 2. 싱글은 기획상 "방을 선택할 경우 바로 입장한다". 정원/전원 대기는 멀티 규칙이라 안 탐
            if (req.bSinglePlayerRun)
            {
                return new RoomSelectResult { Outcome = RoomSelectOutcome.EnterNow, NewSelectedRoomId = req.RoomId };
            }

            // 3. 토글 처리 (같은 방 다시 누르면 선택 취소)
            if (run.SelectedRoomId == req.RoomId)
            {
                return new RoomSelectResult { Outcome = RoomSelectOutcome.Deselected, NewSelectedRoomId = -1 };
            }

            // 4. 인원 수용 검증. 나를 뺀 나머지 중 이 방을 고른 사람 수
            //    (다른 방에서 옮겨오는 경우 내 기존 선택은 세면 안 됨)
            int currentlySelectedCount = 0;
            if (req.OtherSelectedRoomIds != null)
            {
                foreach (int id in req.OtherSelectedRoomIds)
                {
                    if (id == req.RoomId) currentlySelectedCount++;
                }
            }

            if (currentlySelectedCount >= targetRoom.MaxPlayers)
            {
                return RoomSelectResult.Reject(MsgRoomFull);
            }

            // 5. 선택 완료. 다른 플레이어 화면에도 이 선택이 표시된다(RunState 복제)
            // 6. 전원 선택이 끝났으면 각자 고른 방으로 입장 -> Game 쪽이 CheckAllPlayersReady 를 부름
            return new RoomSelectResult { Outcome = RoomSelectOutcome.Selected, NewSelectedRoomId = req.RoomId };
        }

        public static bool IsValidNextRoom(IReadOnlyList<RoomNode> rooms, RunState playerRunState, RoomNode targetRoom)
        {
            if (playerRunState == null || targetRoom == null) return false;

            // 시작 단계(1레벨)면 퀘스트방(Row == 0)만 선택 가능
            if (playerRunState.CurrentMapLevel == 0)
            {
                return targetRoom.Row == 0;
            }

            // 2레벨 이후: 현재 방의 ConnectedRoomIds에 포함된 방인가?
            var currentRoom = FindRoom(rooms, playerRunState.CurrentRoomId);

            if (currentRoom == null) return false;

            return currentRoom.ConnectedRoomIds.Contains(targetRoom.RoomId);
        }

        /// <summary>
        /// 이번 런이 1인인가. 기획의 방 선택 규칙이 싱글/멀티로 갈린다
        /// playerCount: 지금 접속한 인원. 모르면(GameState 가 아직 없음) null -> 멀티로 취급(전원 대기 쪽이 안전)
        /// </summary>
        public static bool IsSinglePlayerRun(int? playerCount, bool bHasDepartedPlayers)
        {
            // 멀티에서 누가 나가 혼자 남은 경우도 멀티 (나간 사람을 기다려야 함)
            if (bHasDepartedPlayers) return false;

            return playerCount.HasValue && playerCount.Value <= 1;
        }

        /// <summary>
        /// 멀티: 전원이 선택을 마쳤는가 (언리얼 CheckAllPlayersReadyAndStart).
        /// selectedRoomIds = 플레이어마다 SelectedRoomId. departedNames = 나간 사람 이름 (DungeonGameMode::GetDepartedNames)
        /// </summary>
        public static PartyReadyResult CheckAllPlayersReady(IReadOnlyCollection<int> selectedRoomIds, IReadOnlyCollection<string> departedNames, out string notice)
        {
            notice = "";

            int players = selectedRoomIds?.Count ?? 0;
            int readyPlayers = 0;
            if (selectedRoomIds != null)
            {
                foreach (int id in selectedRoomIds)
                {
                    if (id != -1) readyPlayers++;
                }
            }

            if (players == 0 || readyPlayers < players) return PartyReadyResult.NotReady;

            // 진행 중에 나간 사람이 돌아올 때까지는 출발하지 않음 (돌아와서 방을 고르면 그때 다시 여기로 옴)
            if (departedNames != null && departedNames.Count > 0)
            {
                notice = $"{string.Join(", ", departedNames)} 님이 돌아올 때까지 출발할 수 없습니다.";
                return PartyReadyResult.WaitingForDeparted;
            }

            // 기획: "모든 플레이어가 선택을 완료 했을 경우, 각자가 선택한 방으로 입장하게 된다"
            return PartyReadyResult.Start;
        }

        /// <summary>
        /// 지도에 쓸 인원: 주점에서 정한 인원과 지금 접속한 인원 중 큰 값 (주점을 안 거친 PIE / 이어하기 대비)
        /// 둘 다 모르면(0) fallback (설정값 CurrentPlayerCount). 1 ~ MaxPartySize
        /// </summary>
        public static int ResolvePlayerCount(int partySize, int connectedCount, int fallback)
        {
            int count = Math.Max(Math.Max(0, partySize), Math.Max(0, connectedCount));
            int value = count > 0 ? count : fallback;
            return Math.Max(1, Math.Min(FloorRules.MaxPartySize, value));
        }

        /// <summary>
        /// 방에 들어감 (언리얼 ATerminusPlayerState::AdvanceToRoom 의 RunState 부분). 구역이 없으면 바로 클리어 처리할 때도 이것
        /// </summary>
        public static void AdvanceToRoom(RunState run, int targetRoomId, int targetRow)
        {
            if (run == null) return;

            // 1. 선택했던 다음 방 ID 초기화
            run.SelectedRoomId = -1;

            // 2. 현재 방 위치 및 진행 레벨(Row + 1) 업데이트
            run.CurrentRoomId = targetRoomId;
            run.CurrentMapLevel = targetRow + 1; // 방을 클리어했으므로 다음 레벨 진입 가능하게 함
            run.VisitedRoomIds.Add(targetRoomId);   // 지도에 지나온 길 표시용
        }

        /// <summary>
        /// 새 층 시작 (언리얼 ATerminusPlayerState::BeginFloor 의 RunState 부분). 체력 회복(기획: 층이 바뀌면 전부 회복)은 Game 쪽
        /// </summary>
        public static void BeginFloor(RunState run)
        {
            if (run == null) return;

            run.CurrentRoomId = -1;
            run.CurrentMapLevel = 0;
            run.SelectedRoomId = -1;
            run.VisitedRoomIds.Clear();
            run.BossRelic = "";
            run.BossSkill = "";
            run.FloorMonsterRooms = 0;   // 퀘스트 '한 층에서 N 번'
            run.FloorGuardianRooms = 0;
        }
    }
}
