// 생성된 지도가 언리얼 GenerateMap 이 보장하려는 조건을 지키는지 검사 (MapGenerator 의 검증 함수를 쓰지 않고 따로 구현)
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Map;

namespace Terminus.Tests
{
    public static class MapInvariants
    {
        /// <summary>어긴 조건들. 비어 있으면 통과</summary>
        public static List<string> Check(MapGenResult result, MapSettings settings, int floor, int players)
        {
            var errors = new List<string>();
            void Fail(string msg) => errors.Add(msg);

            var rooms = result.Rooms;
            bool quest = FloorRules.HasQuestRoom(floor);
            int maxCols = Math.Max(1, settings.MaxRoomsPerRow);
            int expectedRows = Math.Max(2, quest ? settings.TotalLevels : settings.TotalLevels - 1);
            int lastRow = expectedRows - 1;
            int firstNormal = quest ? 1 : 0;
            int restRow = lastRow - 1;
            bool hasRestRow = restRow >= firstNormal + 1;
            int center = maxCols / 2;

            if (!result.bValid) Fail("bValid false");
            if (result.bQuestRow != quest) Fail("bQuestRow 다름");
            if (result.LastRowIndex != lastRow) Fail($"LastRowIndex {result.LastRowIndex} != {lastRow}");

            // 방 번호
            var ids = rooms.Select(r => r.RoomId).ToList();
            if (ids.Distinct().Count() != ids.Count) Fail("RoomId 중복");
            var byId = rooms.ToDictionary(r => r.RoomId);

            // 줄
            var rows = rooms.GroupBy(r => r.Row).ToDictionary(g => g.Key, g => g.ToList());
            for (int r = 0; r < expectedRows; ++r)
            {
                if (!rows.ContainsKey(r)) Fail($"줄 {r} 비어 있음");
            }
            if (rows.Keys.Any(r => r < 0 || r >= expectedRows)) Fail($"줄 범위 밖 (기대 {expectedRows}줄)");
            if (errors.Count > 0) return errors;

            foreach (var kv in rows)
            {
                var cols = kv.Value.Select(r => r.Col).ToList();
                if (cols.Distinct().Count() != cols.Count) Fail($"줄 {kv.Key} 열 중복");
                if (cols.Any(c => c < 0 || c >= maxCols)) Fail($"줄 {kv.Key} 열 범위 밖");
            }

            // 퀘스트 / 보스
            int questCount = rooms.Count(r => r.Type == RoomType.QUEST);
            if (quest)
            {
                var q = rows[0];
                if (q.Count != 1 || q[0].Type != RoomType.QUEST || q[0].Col != center || q[0].MaxPlayers != FloorRules.MaxPartySize)
                    Fail("퀘스트 줄이 퀘스트방 1개(가운데, 정원 최대)가 아님");
                if (questCount != 1) Fail($"퀘스트방 {questCount}개");
            }
            else if (questCount != 0) Fail("퀘스트 없는 층에 퀘스트방");

            var boss = rows[lastRow];
            if (boss.Count != 1 || boss[0].Type != RoomType.BOSS || boss[0].Col != center || boss[0].MaxPlayers != FloorRules.MaxPartySize)
                Fail("마지막 줄이 보스방 1개(가운데, 정원 최대)가 아님");
            if (rooms.Count(r => r.Type == RoomType.BOSS) != 1) Fail("보스방이 1개가 아님");
            if (boss.Count == 1 && boss[0].ConnectedRoomIds.Count != 0) Fail("보스방에서 나가는 길이 있음");

            // 방 하나뿐인 줄 (중간 줄만, 1~2개, 연속 불가)
            var singleRows = new List<int>();
            for (int r = firstNormal + 1; r <= lastRow - 1; ++r)
            {
                if (rows[r].Count == 1) singleRows.Add(r);
            }
            if (rows[firstNormal].Count < 2 && maxCols >= 2 && firstNormal != lastRow) Fail("첫 일반 줄 방이 2개 미만");
            if (firstNormal + 1 <= lastRow - 1 && maxCols >= 2)
            {
                if (singleRows.Count < 1 || singleRows.Count > 2) Fail($"방 하나 줄 {singleRows.Count}개");
                for (int i = 1; i < singleRows.Count; ++i)
                {
                    if (singleRows[i] - singleRows[i - 1] <= 1) Fail("방 하나 줄이 연속");
                }
                if (!singleRows.OrderBy(x => x).SequenceEqual(result.SingleRoomRows.OrderBy(x => x))) Fail("SingleRoomRows 와 실제 방 하나 줄이 다름");
                foreach (int r in singleRows)
                {
                    var n = rows[r][0];
                    if (n.Col < (maxCols - 1) / 2 || n.Col > maxCols / 2) Fail($"방 하나 줄 {r} 이 가운데 열이 아님");
                    if (n.MaxPlayers != FloorRules.MaxPartySize) Fail($"방 하나 줄 {r} 정원이 최대가 아님");
                }
            }

            // 연결
            var parents = rooms.ToDictionary(r => r.RoomId, r => new List<RoomNode>());
            foreach (var n in rooms)
            {
                if (n.ConnectedRoomIds.Distinct().Count() != n.ConnectedRoomIds.Count) Fail($"방 {n.RoomId} 연결 중복");
                foreach (int to in n.ConnectedRoomIds)
                {
                    if (!byId.TryGetValue(to, out var t)) { Fail($"방 {n.RoomId} -> 없는 방 {to}"); continue; }
                    if (t.Row != n.Row + 1) Fail($"방 {n.RoomId}(줄 {n.Row}) -> 방 {to}(줄 {t.Row}) 바로 윗줄이 아님");
                    parents[to].Add(n);
                }
                if (n.Row != lastRow && n.ConnectedRoomIds.Count == 0) Fail($"방 {n.RoomId}(줄 {n.Row}) 위로 가는 길 없음");
            }
            foreach (var n in rooms)
            {
                if (n.Row > 0 && parents[n.RoomId].Count == 0) Fail($"방 {n.RoomId}(줄 {n.Row}) 아래에서 오는 길 없음");
            }

            // 교차
            for (int r = 0; r < lastRow; ++r)
            {
                var edges = new List<(int a, int b)>();
                foreach (var n in rows[r])
                {
                    foreach (int to in n.ConnectedRoomIds)
                    {
                        if (byId.TryGetValue(to, out var t)) edges.Add((n.Col, t.Col));
                    }
                }
                for (int i = 0; i < edges.Count; ++i)
                {
                    for (int j = i + 1; j < edges.Count; ++j)
                    {
                        if ((edges[i].a - edges[j].a) * (edges[i].b - edges[j].b) < 0)
                        {
                            Fail($"줄 {r}->{r + 1} 선 교차 {edges[i]} x {edges[j]}");
                        }
                    }
                }
            }

            // 모든 방에서 보스까지 갈 수 있음
            foreach (var start in rooms)
            {
                var seen = new HashSet<int> { start.RoomId };
                var q = new Queue<RoomNode>();
                q.Enqueue(start);
                bool reached = false;
                while (q.Count > 0 && !reached)
                {
                    var c = q.Dequeue();
                    if (c.Type == RoomType.BOSS) reached = true;
                    foreach (int to in c.ConnectedRoomIds)
                    {
                        if (seen.Add(to) && byId.TryGetValue(to, out var t)) q.Enqueue(t);
                    }
                }
                if (!reached) Fail($"방 {start.RoomId} 에서 보스로 못 감");
            }

            // 타입 고정 줄
            foreach (var n in rows[firstNormal])
            {
                if (firstNormal != lastRow && n.Type != RoomType.MONSTER) Fail($"첫 일반 줄 방 {n.RoomId} 이 몬스터가 아님 ({n.Type})");
            }
            if (hasRestRow)
            {
                foreach (var n in rows[restRow])
                {
                    if (n.Type != RoomType.BREAK) Fail($"보스 직전 줄 방 {n.RoomId} 이 휴식터가 아님 ({n.Type})");
                }
            }

            // 배분된 줄의 배치 규칙
            int distFirst = firstNormal + 1;
            int distLast = hasRestRow ? restRow - 1 : lastRow - 1;
            int noRestRow = hasRestRow ? restRow - 1 : -1;
            var allowed = new HashSet<RoomType> { RoomType.MONSTER, RoomType.BREAK, RoomType.STORE, RoomType.GUARDIAN, RoomType.EVENT };
            foreach (var n in rooms.Where(x => x.Row >= distFirst && x.Row <= distLast))
            {
                if (!allowed.Contains(n.Type)) Fail($"배분 줄 방 {n.RoomId} 타입 {n.Type}");
                if (n.Type == RoomType.BREAK && n.Row == noRestRow) Fail($"휴식터 줄 바로 앞 줄에 휴식터 (방 {n.RoomId})");

                var children = n.ConnectedRoomIds.Where(byId.ContainsKey).Select(id => byId[id]).ToList();
                if (RoomTypeRules.IsNoRepeatType(n.Type))
                {
                    if (parents[n.RoomId].Any(p => p.Type == n.Type) || children.Any(c => c.Type == n.Type))
                        Fail($"방 {n.RoomId} {n.Type} 가 경로상 앞뒤 방과 같은 종류");
                }
                if (RoomTypeRules.IsNoSiblingType(n.Type))
                {
                    var siblings = parents[n.RoomId].SelectMany(p => p.ConnectedRoomIds).Where(id => id != n.RoomId).Distinct()
                        .Select(id => byId[id]);
                    if (siblings.Any(s => s.Type == n.Type)) Fail($"방 {n.RoomId} {n.Type} 가 갈림길 형제와 같은 종류");
                }
            }

            // 정원
            var singleSet = new HashSet<int>(result.SingleRoomRows);
            foreach (var n in rooms)
            {
                if ((quest && n.Row == 0) || n.Row == lastRow) continue;
                bool open = singleSet.Contains(n.Row) || (!quest && n.Row == firstNormal);
                if (open)
                {
                    if (n.MaxPlayers != FloorRules.MaxPartySize) Fail($"열린 줄 방 {n.RoomId} 정원 {n.MaxPlayers}");
                }
                else if (RoomTypeRules.IsCapacityLimited(n.Type))
                {
                    if (n.MaxPlayers < 1 || n.MaxPlayers > players) Fail($"방 {n.RoomId} {n.Type} 정원 {n.MaxPlayers} (인원 {players})");
                }
                else if (n.MaxPlayers != players)
                {
                    Fail($"방 {n.RoomId} {n.Type} 정원 {n.MaxPlayers} != 인원 {players}");
                }
            }

            // 정원 검증 (언리얼 ValidatePlayerCapacity 와 같은 조건을 따로)
            foreach (var kv in rows)
            {
                if (kv.Value.Sum(r => r.MaxPlayers) < players) Fail($"줄 {kv.Key} 정원 합 < 인원");
            }
            foreach (var n in rooms.Where(x => x.ConnectedRoomIds.Count > 0))
            {
                int sum = n.ConnectedRoomIds.Sum(id => byId[id].MaxPlayers);
                if (sum < Math.Min(n.MaxPlayers, players)) Fail($"방 {n.RoomId} 에 들어간 인원이 다 못 나감");
                if (n.ConnectedRoomIds.Count >= 2 && sum <= players) Fail($"방 {n.RoomId} 갈림길 정원 합이 인원 이하");
            }

            return errors;
        }

        /// <summary>
        /// 파티가 실제로 갇히는 배치가 있는지 전부 따라가 봄 (정원 안에서 각자 고를 수 있는 모든 경우).
        /// 갇히는 줄을 돌려줌 (-1 = 없음). ValidatePlayerCapacity 는 방 단위 조건이라 이것보다 약할 수 있음
        /// </summary>
        public static int FindTrapRow(List<RoomNode> rooms, int players)
        {
            var byId = rooms.ToDictionary(r => r.RoomId);
            var rows = rooms.GroupBy(r => r.Row).OrderBy(g => g.Key).Select(g => g.OrderBy(r => r.Col).ToList()).ToList();
            int lastRow = rows.Count - 1;

            // 줄 0: 아무 방이나 (IsValidNextRoom: 시작 단계면 Row 0 아무 방)
            var configs = new HashSet<string>();
            EnumerateInto(rows[0].Select(r => Math.Min(r.MaxPlayers, players)).ToArray(), players, configs);

            for (int r = 0; r < lastRow; ++r)
            {
                var lower = rows[r];
                var upper = rows[r + 1];
                var upperIndex = upper.Select((u, i) => (u.RoomId, i)).ToDictionary(x => x.RoomId, x => x.i);
                var next = new HashSet<string>();

                foreach (var key in configs)
                {
                    int[] counts = Parse(key);
                    int[] remaining = upper.Select(u => u.MaxPlayers).ToArray();
                    int[] placed = new int[upper.Count];
                    int before = next.Count;
                    bool any = false;
                    Move(lower, counts, 0, remaining, placed, upperIndex, next, ref any);
                    if (!any) return r;
                }
                configs = next;
                if (configs.Count == 0) return r;
            }
            return -1;
        }

        static void Move(List<RoomNode> lower, int[] counts, int roomIdx, int[] remaining, int[] placed,
            Dictionary<int, int> upperIndex, HashSet<string> output, ref bool any)
        {
            if (roomIdx == lower.Count)
            {
                any = true;
                output.Add(string.Join(",", placed));
                return;
            }
            var children = lower[roomIdx].ConnectedRoomIds.Where(upperIndex.ContainsKey).Select(id => upperIndex[id]).ToList();
            Spread(lower, counts, roomIdx, children, 0, counts[roomIdx], remaining, placed, upperIndex, output, ref any);
        }

        // roomIdx 방의 left 명을 children[childPos..] 로 나눔
        static void Spread(List<RoomNode> lower, int[] counts, int roomIdx, List<int> children, int childPos, int left,
            int[] remaining, int[] placed, Dictionary<int, int> upperIndex, HashSet<string> output, ref bool any)
        {
            if (left == 0)
            {
                Move(lower, counts, roomIdx + 1, remaining, placed, upperIndex, output, ref any);
                return;
            }
            if (childPos == children.Count) return;

            int c = children[childPos];
            for (int k = Math.Min(left, remaining[c]); k >= 0; --k)
            {
                remaining[c] -= k;
                placed[c] += k;
                Spread(lower, counts, roomIdx, children, childPos + 1, left - k, remaining, placed, upperIndex, output, ref any);
                remaining[c] += k;
                placed[c] -= k;
            }
        }

        static void EnumerateInto(int[] caps, int players, HashSet<string> output)
        {
            var cur = new int[caps.Length];
            void Rec(int i, int left)
            {
                if (i == caps.Length)
                {
                    if (left == 0) output.Add(string.Join(",", cur));
                    return;
                }
                for (int k = Math.Min(left, caps[i]); k >= 0; --k)
                {
                    cur[i] = k;
                    Rec(i + 1, left - k);
                }
                cur[i] = 0;
            }
            Rec(0, players);
        }

        static int[] Parse(string key) => key.Split(',').Select(int.Parse).ToArray();

        /// <summary>같은 지도인지 비교용 문자열</summary>
        public static string Serialize(IEnumerable<RoomNode> rooms) =>
            string.Join("|", rooms.Select(r => $"{r.RoomId}:{r.Row}:{r.Col}:{r.Type}:{r.MaxPlayers}:{string.Join(".", r.ConnectedRoomIds)}:{r.SpecialRoom}"));
    }
}
