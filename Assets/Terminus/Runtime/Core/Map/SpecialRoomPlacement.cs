// 퀘스트 특수 방을 지도에 넣는 지도 쪽 부분 (언리얼 TerminusQuests::InjectSpecialRooms 의 줄 / 방 고르기)
// 어느 퀘스트가 이 층 / 테마에 맞는지(Quest.bInjectSpecialRoom, QuestRoom.Theme == FloorTheme.DisplayName, MatchesFloor)는 퀘스트 쪽이 거르고
// 맞는 것만 TryPlace 로 넘긴다. 바뀌면 퀘스트 쪽이 런 지도 사본(RunSubsystem.SetRooms)도 갱신
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Map
{
    public static class SpecialRoomPlacement
    {
        /// <summary>
        /// Depth = 12칸 지도 기준 레벨 (1 = 퀘스트 줄, 12 = 보스). 퀘스트 줄이 없는 층은 한 칸 당겨짐
        /// lastRow = 지금 지도의 마지막 줄 (보스)
        /// </summary>
        public static int RowForDepth(int depth, int floor, int lastRow)
        {
            int firstRow = FloorRules.HasQuestRoom(floor) ? 1 : 0;
            if (depth >= 12) return lastRow;

            int row = depth - (FloorRules.HasQuestRoom(floor) ? 1 : 2);
            return Math.Max(firstRow, Math.Min(lastRow, row));
        }

        public static bool ContainsSpecialRoom(IReadOnlyList<RoomNode> rooms, string specialRoomId)
        {
            if (rooms == null || Ids.IsNone(specialRoomId)) return false;
            foreach (var n in rooms)
            {
                if (n.SpecialRoom == specialRoomId) return true;
            }
            return false;
        }

        /// <summary>
        /// 특수 방 하나를 넣는다. 이미 있거나 넣을 방이 없으면 null.
        /// 그 줄의 (퀘스트 방이 아니고 아직 특수 방이 아닌) 방 중 하나를 골라 가디언으로 바꿈 (보스방이면 보스 그대로)
        /// </summary>
        public static RoomNode TryPlace(List<RoomNode> rooms, int floor, string specialRoomId, int depth, Random rng)
        {
            if (rooms == null || rng == null || Ids.IsNone(specialRoomId)) return null;
            if (ContainsSpecialRoom(rooms, specialRoomId)) return null;

            int lastRow = 0;
            foreach (var node in rooms) lastRow = Math.Max(lastRow, node.Row);

            int row = RowForDepth(depth, floor, lastRow);

            var candidates = new List<int>();
            for (int i = 0; i < rooms.Count; ++i)
            {
                var n = rooms[i];
                if (n.Row == row && n.Type != RoomType.QUEST && Ids.IsNone(n.SpecialRoom)) candidates.Add(i);
            }
            if (candidates.Count == 0) return null;

            var picked = rooms[candidates[MapGenerator.RandRange(rng, 0, candidates.Count - 1)]];
            picked.SpecialRoom = specialRoomId;
            if (picked.Type != RoomType.BOSS) picked.Type = RoomType.GUARDIAN;   // 특수 방 = 지정 몬스터와 싸우는 방
            return picked;
        }
    }
}
