// 지도 데이터 (언리얼 Map/MapManager.h 의 구조체 / 열거형). 생성 / 진행 로직은 같은 폴더의 다른 파일
using System.Collections.Generic;

namespace Terminus.Map
{
    [System.Serializable]
    public sealed class RoomNode
    {
        public int RoomId = 0;
        public int Row = 0;
        public int Col = 0;
        public RoomType Type = RoomType.MONSTER;
        public int MaxPlayers = 1;
        public List<int> ConnectedRoomIds = new List<int>();
        // 퀘스트 특수 방 (DT_QuestRoom 행). 비어 있으면 일반 방. 들어가면 그 방의 SpawnMonster 가 나옴
        public string SpecialRoom = "";

        public RoomNode Clone()
        {
            var c = (RoomNode)MemberwiseClone();
            c.ConnectedRoomIds = new List<int>(ConnectedRoomIds);
            return c;
        }
    }

    // 테마 끝 층 보스 뒤 선택지
    public enum FloorChoice
    {
        None,
        NextFloor,   // 다음 층 (마지막 층엔 없음)
        Escape,      // 탈출 -> 정산
        Betray,      // 배신 (멀티만, 선착순 1명)
    }

    public enum FloorVotePhase
    {
        None,        // 투표 없음
        Voting,      // 투표 중 (VoteSeconds)
        Revealing,   // 다 골랐음 -> 공개 타이머 (RevealSeconds)
        Done,        // 공개 + 결과 (잠깐 보여주고 적용)
    }

    [System.Serializable]
    public sealed class FloorVoteEntry
    {
        // 언리얼 APlayerState 포인터 대신 플레이어 번호 (NGO ClientId 등, 게임 쪽이 정함)
        public ulong PlayerId = 0;
        public FloorChoice Choice = FloorChoice.None;

        public FloorVoteEntry Clone() => (FloorVoteEntry)MemberwiseClone();
    }

    // 모두에게 복제되는 투표 상태. 누가 골랐는지(RevealedVotes)는 공개 단계가 끝나야 채워짐
    [System.Serializable]
    public sealed class FloorVoteState
    {
        // 투표마다 다른 번호 (화면이 새 투표인지 알게)
        public int VoteId = 0;
        public FloorVotePhase Phase = FloorVotePhase.None;
        public int Floor = 0;
        public List<FloorChoice> Options = new List<FloorChoice>();
        // 선택지별 표 수 (Options 와 같은 순서)
        public List<int> Counts = new List<int>();
        public int VotedCount = 0;
        public int TotalVoters = 0;
        // 이 단계가 끝나는 서버 시각 (초)
        public double PhaseEndTime = 0;
        public List<FloorVoteEntry> RevealedVotes = new List<FloorVoteEntry>();
        public FloorChoice Result = FloorChoice.None;

        public FloorVoteState Clone()
        {
            var c = (FloorVoteState)MemberwiseClone();
            c.Options = new List<FloorChoice>(Options);
            c.Counts = new List<int>(Counts);
            c.RevealedVotes = RevealedVotes.ConvertAll(e => e.Clone());
            return c;
        }
    }

    public static class FloorRules
    {
        // 한 파티 최대 인원
        public const int MaxPartySize = 4;

        // 테마가 바뀌는 층인가 (2 / 4 / 6층). 이 층 보스 뒤에서만 다음 층 / 탈출 / 배신을 고름
        public static bool IsThemeEndFloor(int floor) => floor % 2 == 0;

        // 퀘스트 방이 있는 층 = 테마가 시작되는 층 (1 / 3 / 5층)
        public static bool HasQuestRoom(int floor) => !IsThemeEndFloor(floor);

        // 층 이름. 1~2층 표층 / 3~4층 중층 / 5~6층 심층
        public static string GetTierName(int floor)
        {
            if (floor <= 2) return "표층";
            if (floor <= 4) return "중층";
            return "심층";
        }
    }
}
