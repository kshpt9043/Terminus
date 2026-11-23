// 지도 생성 / 검증 (언리얼 AMapManager::GenerateMap, ConnectRowsWithoutCrossing, DistributeRoomTypes,
// GetRoomCapacity, ValidatePathToBoss, ValidatePlayerCapacity, OpenSingleRoomRows, GetLevelCount)
// 엔진 없음. 랜덤은 주입받은 System.Random (언리얼 FMath::Rand 자리). 알고리즘 순서는 언리얼 그대로라 시드가 같으면 같은 지도
using System;
using System.Collections.Generic;
using System.Linq;

namespace Terminus.Map
{
    /// <summary>GenerateMap 결과. Rooms 외에는 로그 / 테스트용</summary>
    public sealed class MapGenResult
    {
        public List<RoomNode> Rooms = new List<RoomNode>();
        // 방이 하나뿐인 줄로 뽑힌 줄들 (최대 2개, 연속 불가)
        public List<int> SingleRoomRows = new List<int>();
        // 몇 번 만에 조건을 만족했는지
        public int Attempts = 0;
        // MaxRetries 안에 조건(경로 + 정원)을 만족했는지. false 면 마지막 지도를 그대로 씀
        public bool bValid = false;
        public bool bQuestRow = false;
        // 일반 방이 시작되는 줄 (몬스터 고정). 퀘스트 층 1, 아니면 0
        public int FirstNormalRow = 0;
        // 보스방 줄
        public int LastRowIndex = 0;
    }

    public sealed class MapGenerator
    {
        public MapSettings Settings;
        readonly Random _rng;

        // 언리얼 UE_LOG 자리. Game 쪽이 Debug.Log 로 이어 붙임 (비워 두면 안 찍음)
        public Action<string> Log;

        public MapGenerator(MapSettings settings, Random rng)
        {
            Settings = settings ?? new MapSettings();
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        /// <summary>이 층의 지도를 만든다. playerCount = 지도에 쓸 인원 (1 ~ MaxPartySize)</summary>
        public List<RoomNode> GenerateMap(int floor, int playerCount) => Generate(floor, playerCount).Rooms;

        public MapGenResult Generate(int floor, int playerCount)
        {
            playerCount = Math.Max(1, Math.Min(FloorRules.MaxPartySize, playerCount));

            var map = new List<RoomNode>();
            bool bIsValidMap = false;
            int retryCount = 0;
            int maxRetries = Math.Max(1, Settings.MaxRetries);

            // 퀘스트 방은 테마가 시작되는 층에만 (기획: 테마에 들어갈 때 퀘스트를 받음). 없는 층은 그 줄을 빼고 일반 방부터
            bool bQuestRow = FloorRules.HasQuestRoom(floor);
            int firstNormalRow = bQuestRow ? 1 : 0;   // 일반 방이 시작되는 줄 (몬스터 고정)
            int validTotalLevels = Math.Max(2, bQuestRow ? Settings.TotalLevels : Settings.TotalLevels - 1);
            int lastRowIndex = validTotalLevels - 1; // 보스방 Row
            int validMaxRooms = Math.Max(1, Settings.MaxRoomsPerRow);
            int centerCol = validMaxRooms / 2;

            // 1개 방만 가질 레벨들의 집합 (최대 2개, 연속 불가)
            var chosenSingleRoomRows = new List<int>();

            while (!bIsValidMap && retryCount < maxRetries)
            {
                retryCount++;
                map.Clear();
                chosenSingleRoomRows.Clear();
                int globalRoomId = 1;

                // ==========================================
                // [Pass 0] 1개 방을 가질 레벨 추첨 (최대 2개, 연속X)
                // ==========================================
                // 중간 레벨 범위: 몬스터 고정 줄 다음 ~ (LastRowIndex - 1)
                int minMiddleRow = firstNormalRow + 1;
                int maxMiddleRow = lastRowIndex - 1;

                if (minMiddleRow <= maxMiddleRow)
                {
                    // 1개만 정할지, 2개까지 정할지 무작위 결정 (1 또는 2)
                    int targetSingleCount = RandRange(1, 2);

                    // 후보 레벨 목록 생성
                    var availableRows = new List<int>();
                    for (int r = minMiddleRow; r <= maxMiddleRow; ++r)
                    {
                        availableRows.Add(r);
                    }

                    for (int i = 0; i < targetSingleCount && availableRows.Count > 0; ++i)
                    {
                        int randomIdx = RandRange(0, availableRows.Count - 1);
                        int selectedRow = availableRows[randomIdx];
                        chosenSingleRoomRows.Add(selectedRow);

                        // ★ 연속 출현 방지: 선택된 Row 및 바로 인접한 Row(±1)를 후보에서 제거
                        availableRows.RemoveAll(rowVal => Math.Abs(rowVal - selectedRow) <= 1);
                    }
                }

                // ==========================================
                // [Pass 1] TotalLevels(Row 0 ~ LastRowIndex) 방 자리 생성
                //  타입은 여기서 정하지 않음 -> Pass 2.5 에서 지도 전체를 보고 고르게 나눔
                //  (방마다 따로 뽑으면 한쪽에 몰리거나 상점이 0개인 지도가 나옴)
                // ==========================================
                for (int row = 0; row < validTotalLevels; ++row)
                {
                    // 1. [1레벨 / Row 0] 퀘스트방 1개 고정 (퀘스트 방이 있는 층만)
                    if (bQuestRow && row == 0)
                    {
                        map.Add(CreateRoom(globalRoomId++, row, centerCol, RoomType.QUEST, FloorRules.MaxPartySize));
                        continue;
                    }

                    // 2. [마지막 레벨 / LastRowIndex] 보스방 1개 고정
                    if (row == lastRowIndex)
                    {
                        map.Add(CreateRoom(globalRoomId++, row, centerCol, RoomType.BOSS, FloorRules.MaxPartySize));
                        continue;
                    }

                    // 3. [1개 방 레벨로 당첨된 레벨들]
                    if (chosenSingleRoomRows.Contains(row))
                    {
                        // 가운데 열에. 위아래 줄의 방이 전부 이 방으로 모이는데, 끝 열에 있으면
                        // 반대쪽 끝에서 지도를 가로지르는 긴 선이 생김 (지도에서도 가운데에 그려짐)
                        int middleCol = RandRange((validMaxRooms - 1) / 2, validMaxRooms / 2);

                        // 타입 / 정원은 Pass 2.5 에서
                        map.Add(CreateRoom(globalRoomId++, row, middleCol));
                        continue;
                    }

                    // 4. [나머지 일반 레벨: 최소 2개 이상 방 생성 보장]
                    // 첫 일반 줄(FirstNormalRow: 퀘스트 층은 Row 1, 아니면 Row 0)은 몬스터방 고정. 나머지 타입 / 정원은 Pass 2.5 에서
                    var rowRooms = new List<RoomNode>();
                    for (int col = 0; col < validMaxRooms; ++col)
                    {
                        if (RandRange(0.0f, 1.0f) > 0.6f)
                        {
                            rowRooms.Add(CreateRoom(globalRoomId++, row, col));
                        }
                    }

                    // 방이 2개 미만으로 뽑혔다면 무조건 2개가 되도록 추가 생성
                    while (rowRooms.Count < 2 && validMaxRooms >= 2)
                    {
                        int randomCol = RandRange(0, validMaxRooms - 1);

                        bool bAlreadyExists = rowRooms.Exists(room => room.Col == randomCol);

                        if (!bAlreadyExists)
                        {
                            rowRooms.Add(CreateRoom(globalRoomId++, row, randomCol));
                        }
                    }

                    map.AddRange(rowRooms);
                }

                // ==========================================
                // [Pass 2] 레벨 간 연결(Edge) 구축. 선끼리 교차하지 않게
                //  (예전 방식은 열 차이 1 이하를 전부 이어서 4장 중 3장꼴로 선이 엇갈렸음)
                //  타입보다 먼저: 방 배치 규칙이 경로(앞뒤 방 / 갈림길)를 보고 정해지기 때문 (Slay the Spire 도 이 순서)
                //  연결은 열 위치만 보므로 타입 없이 만들 수 있음
                // ==========================================
                for (int row = 0; row < lastRowIndex; ++row)
                {
                    var currentRowIndices = new List<int>();
                    var nextRowIndices = new List<int>();

                    for (int i = 0; i < map.Count; ++i)
                    {
                        if (map[i].Row == row) currentRowIndices.Add(i);
                        else if (map[i].Row == row + 1) nextRowIndices.Add(i);
                    }

                    ConnectRowsWithoutCrossing(map, currentRowIndices, nextRowIndices, _rng);
                }

                // ==========================================
                // [Pass 2.5] 방 타입 / 정원
                // ==========================================
                // 보스 직전 레벨은 전부 휴식터 (기획: UI 레퍼런스 "보스방 직전 방들은 휴식터"). 비율 배분에는 안 셈
                // 배분보다 먼저 정해야 함 -> 배분이 경로 규칙을 검사할 때 이 줄도 보게
                int restRow = lastRowIndex - 1;
                bool bHasRestRow = restRow >= firstNormalRow + 1;   // 몬스터 고정 줄보다 짧은 지도면 생략

                if (bHasRestRow)
                {
                    foreach (var node in map)
                    {
                        if (node.Row == restRow) node.Type = RoomType.BREAK;
                    }
                }

                // 첫 일반 줄 다음 ~ 휴식터 줄 앞까지 비율대로 고르게 배분. 퀘스트 / 첫 일반 줄(몬스터) / 휴식터 줄 / 보스는 고정
                // 휴식터 줄 바로 앞 줄에는 휴식터 금지 (Slay the Spire 14층 규칙과 같음)
                DistributeRoomTypes(map, firstNormalRow + 1, bHasRestRow ? restRow - 1 : lastRowIndex - 1,
                    bHasRestRow ? restRow - 1 : -1, Settings.RoomTypeWeights, _rng, Log);

                foreach (var node in map)
                {
                    // 퀘스트 / 보스는 생성할 때 전원 수용으로 이미 정함
                    if ((bQuestRow && node.Row == 0) || node.Row == lastRowIndex) continue;

                    // 정원 = 최대 인원 (인원을 잘못 알아도 막히지 않게)
                    //  - 방이 하나뿐인 층: 파티 전원이 여길 지나가야 함
                    //  - 퀘스트 방이 없는 층(2 / 4 / 6층)의 첫 줄: 갈라져 나올 방이 없어 정원으로 나눌 이유가 없음
                    //    (정원 1 방끼리만 있으면 같은 방을 고른 사람이 막힘: 사용자 제보 10-10)
                    bool bOpenRow = chosenSingleRoomRows.Contains(node.Row) || (!bQuestRow && node.Row == firstNormalRow);
                    node.MaxPlayers = bOpenRow ? FloorRules.MaxPartySize : GetRoomCapacity(node.Type, playerCount, _rng);
                }

                // ==========================================
                // [Pass 3] 경로 및 분기 수용 조건 검증
                // ==========================================
                bool bPathValid = ValidatePathToBoss(map, lastRowIndex);
                bool bCapacityValid = ValidatePlayerCapacity(map, playerCount);

                bIsValidMap = bPathValid && bCapacityValid;
            }

            if (!bIsValidMap)
            {
                Log?.Invoke($"[MapGenerator] {maxRetries}번 재시도해도 조건을 만족하는 지도를 못 만듦. 마지막 지도를 그대로 씀 (진행 불가 구간이 있을 수 있음)");
            }

            Log?.Invoke($"[MapGenerator] Map Generated with SingleRoomRows at [ {string.Join(" ", chosenSingleRoomRows)} ] (Attempts: {retryCount})");

            return new MapGenResult
            {
                Rooms = map,
                SingleRoomRows = new List<int>(chosenSingleRoomRows),
                Attempts = retryCount,
                bValid = bIsValidMap,
                bQuestRow = bQuestRow,
                FirstNormalRow = firstNormalRow,
                LastRowIndex = lastRowIndex,
            };
        }

        public static RoomNode CreateRoom(int roomId, int row, int col, RoomType type = RoomType.MONSTER, int maxPlayers = 1)
        {
            return new RoomNode
            {
                RoomId = roomId,
                Row = row,
                Col = col,
                Type = type,
                MaxPlayers = maxPlayers,
            };
        }

        /// <summary>
        /// 아래 줄(Lower) -> 위 줄(Upper) 연결. 선끼리 교차하지 않고 모든 방이 위아래로 최소 하나씩 이어짐
        /// 화면에서도 방이 열(Col) 위치에 그려지므로 교차가 없으면 지도에서도 선이 엇갈리지 않음
        /// lower / upper 는 map 의 인덱스 (복사해서 씀)
        /// </summary>
        public static void ConnectRowsWithoutCrossing(List<RoomNode> map, IList<int> lowerIn, IList<int> upperIn, Random rng)
        {
            if (lowerIn.Count == 0 || upperIn.Count == 0) return;

            // 두 줄을 열 순서로. 선 A(아래 a1 -> 위 b1), B(a2 -> b2) 는 a1 < a2 인데 b1 > b2 일 때만 교차한다
            var lower = new List<int>(lowerIn);
            var upper = new List<int>(upperIn);
            lower.Sort((a, b) => map[a].Col.CompareTo(map[b].Col));
            upper.Sort((a, b) => map[a].Col.CompareTo(map[b].Col));

            // ------------------------------------------------------------------
            // 1. 계단식 연결: 양쪽 맨 왼쪽에서 출발해 "아래만 다음 / 위만 다음 / 둘 다 다음" 중
            //    열 거리가 가까운 쪽으로 한 칸씩 나아가며 잇는다. 양쪽 맨 오른쪽에 닿으면 끝
            //    -> 순서가 뒤집히는 일이 없으니 교차가 없고, 모든 방이 위아래로 최소 하나씩 연결됨
            // ------------------------------------------------------------------
            const float StepRandomness = 0.6f;   // 열 거리가 비슷한 후보 사이에서 매번 같은 모양이 안 나오게

            int i = 0, j = 0;
            int lastI = lower.Count - 1;
            int lastJ = upper.Count - 1;

            while (true)
            {
                AddUnique(map[lower[i]].ConnectedRoomIds, map[upper[j]].RoomId);
                if (i == lastI && j == lastJ) break;

                int bestI = i, bestJ = j;
                float bestScore = float.MaxValue;

                void Consider(int ni, int nj)
                {
                    float score = Math.Abs(map[lower[ni]].Col - map[upper[nj]].Col) + FRandRange(rng, 0f, StepRandomness);
                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestI = ni;
                        bestJ = nj;
                    }
                }

                if (i < lastI) Consider(i + 1, j);
                if (j < lastJ) Consider(i, j + 1);
                if (i < lastI && j < lastJ) Consider(i + 1, j + 1);

                i = bestI;
                j = bestJ;
            }

            // ------------------------------------------------------------------
            // 2. 갈림길 보강: 열 차이 1 이하인 방끼리, 기존 선과 교차하지 않으면 전부 추가 (순서는 랜덤)
            //    계단식만 쓰면 갈림길이 29% 로 줄고 정원 조건 맞추기도 어려워짐 (시뮬레이션: 추가 후 39%)
            // ------------------------------------------------------------------
            var candidates = new List<KeyValuePair<int, int>>();
            foreach (int a in lower)
            {
                foreach (int b in upper)
                {
                    if (Math.Abs(map[a].Col - map[b].Col) <= 1 && !map[a].ConnectedRoomIds.Contains(map[b].RoomId))
                    {
                        candidates.Add(new KeyValuePair<int, int>(a, b));
                    }
                }
            }

            for (int k = candidates.Count - 1; k > 0; --k)
            {
                Swap(candidates, k, RandRange(rng, 0, k));
            }

            foreach (var cand in candidates)
            {
                int aCol = map[cand.Key].Col;
                int bCol = map[cand.Value].Col;

                bool bCrosses = false;
                foreach (int src in lower)
                {
                    foreach (int dstId in map[src].ConnectedRoomIds)
                    {
                        int dstIdx = upper.FindIndex(u => map[u].RoomId == dstId);
                        if (dstIdx < 0) continue;

                        if ((map[src].Col - aCol) * (map[upper[dstIdx]].Col - bCol) < 0)
                        {
                            bCrosses = true;
                            break;
                        }
                    }
                    if (bCrosses) break;
                }

                if (!bCrosses)
                {
                    map[cand.Key].ConnectedRoomIds.Add(map[cand.Value].RoomId);
                }
            }
        }

        // 배분 계산용 (언리얼 DistributeRoomTypes 안의 지역 구조체)
        sealed class Quota
        {
            public RoomType Type;
            public int Count;
            public float Remainder;
            public float TieBreak;     // 소수부가 같은 타입끼리(10% 셋) 순서를 랜덤으로
        }

        sealed class Target
        {
            public float Position;
            public float TieBreak;
            public RoomType Type;
        }

        /// <summary>
        /// FirstRow ~ LastRow 방들의 타입을 RoomTypeWeights 비율대로 고르게 나눔.
        /// 개수를 먼저 확정하고(비율 × 방 수), 타입마다 층 전체에 등간격으로 퍼뜨림
        /// 배치 규칙은 Slay the Spire 와 같음 (초반 층 휴식/가디언 금지만 뺌). 연결이 먼저 만들어져 있어야 함
        ///  - 휴식/상점/가디언은 바로 앞뒤 방(경로상)과 같은 종류 금지
        ///  - 같은 방에서 갈라지는 방끼리 휴식/상점/가디언/이벤트 중복 금지, 몬스터 중복은 가능하면 피함
        ///  - NoRestRow 줄에는 휴식터 금지 (보스 직전 휴식터 줄의 바로 앞 줄. 없으면 -1)
        /// </summary>
        public static void DistributeRoomTypes(List<RoomNode> map, int firstRow, int lastRow, int noRestRow,
            IDictionary<RoomType, float> roomTypeWeights, Random rng, Action<string> log = null)
        {
            // 대상 방들을 층 순서로 줄 세움. 같은 층 안에서는 섞음 (늘 왼쪽 방부터 특수방이 되지 않게)
            var order = new List<int>();
            for (int i = 0; i < map.Count; ++i)
            {
                if (map[i].Row >= firstRow && map[i].Row <= lastRow)
                {
                    order.Add(i);
                }
            }

            int n = order.Count;
            if (n == 0) return;

            for (int i = n - 1; i > 0; --i)
            {
                Swap(order, i, RandRange(rng, 0, i));
            }
            // StableSort: LINQ OrderBy 는 안정 정렬
            order = order.OrderBy(idx => map[idx].Row).ToList();

            // ------------------------------------------------------------------
            // 1. 타입별 개수를 먼저 확정. 비율 × 방 수를 내림하고, 남는 칸은 소수부가 큰 타입부터
            //    예) 방 21개 -> 몬스터 12 / 이벤트 3 / 휴식 2 / 상점 2 / 가디언 2
            //    방마다 추첨하던 때는 상점 0개인 지도가 10% 가까이 나왔음
            // ------------------------------------------------------------------
            float totalWeight = 0.0f;
            if (roomTypeWeights != null)
            {
                foreach (var pair in roomTypeWeights)
                {
                    if (pair.Value > 0.0f) totalWeight += pair.Value;
                }
            }

            if (totalWeight <= 0.0f)
            {
                foreach (int idx in order) map[idx].Type = RoomType.MONSTER;
                return;
            }

            var quotas = new List<Quota>();
            int assigned = 0;
            foreach (var pair in roomTypeWeights)
            {
                if (pair.Value <= 0.0f) continue;

                float exact = n * pair.Value / totalWeight;
                int count = (int)Math.Floor(exact);
                quotas.Add(new Quota { Type = pair.Key, Count = count, Remainder = exact - count, TieBreak = FRand(rng) });
                assigned += count;
            }

            quotas.Sort((a, b) =>
            {
                if (a.Remainder != b.Remainder) return b.Remainder.CompareTo(a.Remainder);
                return b.TieBreak.CompareTo(a.TieBreak);
            });

            for (int i = 0; assigned < n; i = (i + 1) % quotas.Count)
            {
                quotas[i].Count++;
                assigned++;
            }

            // ------------------------------------------------------------------
            // 2. 타입마다 등간격으로 목표 위치를 잡음. 시작점은 타입마다 랜덤
            //    - 등간격: 한 타입이 초반/후반 한쪽에 몰리지 않음
            //    - 타입별 랜덤 시작점: 비율이 같은 타입들(휴식/상점/가디언)이 같은 층에 뭉치지 않고,
            //      지도마다 위치가 달라짐 (시작점이 같으면 매번 4층, 8층에 상점이 나오는 식이 됨)
            //    - 약간의 흔들림: 너무 규칙적이지 않게. 크게 주면 다시 몰림 (시뮬레이션으로 0.15 선택)
            // ------------------------------------------------------------------
            const float SpreadJitter = 0.15f;

            var targets = new List<Target>();
            foreach (var q in quotas)
            {
                if (q.Count <= 0) continue;

                float spacing = (float)n / q.Count;
                float offset = FRand(rng);

                for (int k = 0; k < q.Count; ++k)
                {
                    float jitter = FRandRange(rng, -SpreadJitter, SpreadJitter) * spacing;
                    targets.Add(new Target { Position = (k + offset) * spacing + jitter, TieBreak = FRand(rng), Type = q.Type });
                }
            }

            targets.Sort((a, b) =>
            {
                if (a.Position != b.Position) return a.Position.CompareTo(b.Position);
                return a.TieBreak.CompareTo(b.TieBreak);
            });

            // 3. 목표 위치 순서대로 아래층 방부터 배정 (개수 합 = 방 수라 딱 맞음)
            for (int i = 0; i < n; ++i)
            {
                map[order[i]].Type = targets[i].Type;
            }

            // ------------------------------------------------------------------
            // 4. 경로 규칙 (Slay the Spire 와 같음. 초반 층 휴식/가디언 금지만 뺌)
            //    - 연속 금지: 휴식/상점/가디언은 바로 앞 방이나 바로 다음 방이 같은 종류면 안 됨
            //    - 갈림길 금지: 같은 방에서 갈라지는 방들끼리 휴식/상점/가디언/이벤트가 겹치면 안 됨
            //    - 보스 직전 휴식터 줄의 바로 앞 줄(NoRestRow)에는 휴식터 금지
            //    걸리는 특수방은 둬도 되는 가장 가까운 몬스터방과 맞바꿈
            //    지도 전체 그래프를 봄 -> 배분 대상 밖의 보스 직전 휴식터 줄도 연속 검사에 걸림
            // ------------------------------------------------------------------
            var indexById = new Dictionary<int, int>();
            for (int i = 0; i < map.Count; ++i)
            {
                indexById[map[i].RoomId] = i;
            }

            // 부모 = 나로 들어오는 방, 자식 = 내가 가는 방, 형제 = 같은 부모에서 갈라진 다른 방
            var parentsOf = new List<int>[map.Count];
            var childrenOf = new List<int>[map.Count];
            var siblingsOf = new List<int>[map.Count];
            for (int i = 0; i < map.Count; ++i)
            {
                parentsOf[i] = new List<int>();
                childrenOf[i] = new List<int>();
                siblingsOf[i] = new List<int>();
            }

            for (int i = 0; i < map.Count; ++i)
            {
                foreach (int toId in map[i].ConnectedRoomIds)
                {
                    if (indexById.TryGetValue(toId, out int to))
                    {
                        childrenOf[i].Add(to);
                        parentsOf[to].Add(i);
                    }
                }
            }

            for (int i = 0; i < map.Count; ++i)
            {
                foreach (int p in parentsOf[i])
                {
                    foreach (int c in childrenOf[p])
                    {
                        if (c != i) AddUnique(siblingsOf[i], c);
                    }
                }
            }

            // At 자리에 Type 을 둬도 되는가. Ignore = 지금 옮기려는 방 자신 (자리를 비운다고 보고 검사에서 뺌)
            bool IsAllowed(RoomType type, int at, int ignore)
            {
                if (type == RoomType.MONSTER) return true;   // 몬스터는 빈자리 채우는 용도라 늘 허용 (Slay the Spire 와 같음)

                if (type == RoomType.BREAK && map[at].Row == noRestRow) return false;

                if (RoomTypeRules.IsNoRepeatType(type))
                {
                    foreach (int o in parentsOf[at]) { if (o != ignore && map[o].Type == type) return false; }
                    foreach (int o in childrenOf[at]) { if (o != ignore && map[o].Type == type) return false; }
                }

                if (RoomTypeRules.IsNoSiblingType(type))
                {
                    foreach (int o in siblingsOf[at]) { if (o != ignore && map[o].Type == type) return false; }
                }

                return true;
            }

            for (int pass = 0; pass < 6; ++pass)
            {
                bool bChanged = false;

                for (int pos = 0; pos < n; ++pos)
                {
                    int roomIdx = order[pos];
                    RoomType type = map[roomIdx].Type;
                    int row = map[roomIdx].Row;
                    if (IsAllowed(type, roomIdx, roomIdx)) continue;

                    // 이 타입을 둬도 되는 몬스터방 중 가장 가까운 층 (같은 층도 됨)
                    var candidates = new List<int>();
                    int bestDist = int.MaxValue;
                    foreach (int candIdx in order)
                    {
                        var cand = map[candIdx];
                        if (candIdx == roomIdx || cand.Type != RoomType.MONSTER) continue;
                        if (!IsAllowed(type, candIdx, roomIdx)) continue;

                        int dist = Math.Abs(cand.Row - row);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            candidates.Clear();
                        }
                        if (dist == bestDist)
                        {
                            candidates.Add(candIdx);
                        }
                    }

                    if (candidates.Count > 0)
                    {
                        int swapIdx = candidates[RandRange(rng, 0, candidates.Count - 1)];
                        map[swapIdx].Type = type;
                    }
                    else
                    {
                        // 둘 곳이 아예 없으면 몬스터방으로 (그 타입 개수가 하나 줄어듦). 규칙 위반보다 낫다
                        log?.Invoke($"[MapGenerator] {type} 를 규칙에 맞게 둘 곳이 없어 몬스터방으로 바꿈 (Row {row})");
                    }

                    map[roomIdx].Type = RoomType.MONSTER;
                    bChanged = true;
                }

                if (!bChanged) break;
            }

            // ------------------------------------------------------------------
            // 5. 갈림길에서 몬스터끼리 겹치는 것 줄이기 ("몬스터 vs 몬스터" 선택지)
            //    Slay the Spire 도 될 수 있으면 피하고 자리가 없으면 몬스터로 채움 -> 여기서도 가능할 때만
            //    겹친 몬스터방을 2줄 이내의 특수방과 맞바꿔 보고, 규칙을 안 깨면서 겹침이 줄 때만 바꿈
            //    (멀리서 끌어오면 위에서 고르게 퍼뜨린 분포가 무너짐. 시뮬레이션: 지도당 1.7쌍 -> 0.3쌍)
            // ------------------------------------------------------------------
            var inOrder = new HashSet<int>(order);

            int CountMonsterSiblingPairs()
            {
                int pairs = 0;
                foreach (int idx in order)
                {
                    if (map[idx].Type != RoomType.MONSTER) continue;
                    foreach (int s in siblingsOf[idx])
                    {
                        if (s > idx && inOrder.Contains(s) && map[s].Type == RoomType.MONSTER) ++pairs;
                    }
                }
                return pairs;
            }

            const int MaxSwapRowDistance = 2;

            for (int pass = 0; pass < 3; ++pass)
            {
                bool bImproved = false;

                foreach (int monsterIdx in order)
                {
                    if (map[monsterIdx].Type != RoomType.MONSTER) continue;

                    bool bHasMonsterSibling = siblingsOf[monsterIdx].Exists(s => inOrder.Contains(s) && map[s].Type == RoomType.MONSTER);
                    if (!bHasMonsterSibling) continue;

                    int basePairs = CountMonsterSiblingPairs();
                    int bestIdx = -1;
                    int bestGain = 0;
                    int bestDist = int.MaxValue;

                    foreach (int specialIdx in order)
                    {
                        RoomType specialType = map[specialIdx].Type;
                        if (specialType == RoomType.MONSTER) continue;

                        int dist = Math.Abs(map[specialIdx].Row - map[monsterIdx].Row);
                        if (dist > MaxSwapRowDistance) continue;
                        if (!IsAllowed(specialType, monsterIdx, specialIdx)) continue;

                        // 맞바꿔 보고 겹침이 얼마나 주는지
                        map[specialIdx].Type = RoomType.MONSTER;
                        map[monsterIdx].Type = specialType;
                        int gain = basePairs - CountMonsterSiblingPairs();
                        map[specialIdx].Type = specialType;
                        map[monsterIdx].Type = RoomType.MONSTER;

                        if (gain > bestGain || (gain > 0 && gain == bestGain && dist < bestDist))
                        {
                            bestIdx = specialIdx;
                            bestGain = gain;
                            bestDist = dist;
                        }
                    }

                    if (bestIdx != -1)
                    {
                        map[monsterIdx].Type = map[bestIdx].Type;
                        map[bestIdx].Type = RoomType.MONSTER;
                        bImproved = true;
                    }
                }

                if (!bImproved) break;
            }
        }

        /// <summary>
        /// 방 타입별 최대 입장 인원.
        /// 기획: 인원 제한은 몬스터/가디언 방에만 붙는다("몬스터 방 [인원 제한 가능]").
        /// 휴식터/상점/이벤트/퀘스트/보스는 파티가 쪼개질 이유가 없으니 전원 수용
        /// </summary>
        public static int GetRoomCapacity(RoomType type, int playerCount, Random rng)
        {
            // 상점이나 휴식터에 정원 1 이 걸리면 파티가 아무 이유 없이 쪼개진다
            bool bLimited = type == RoomType.MONSTER || type == RoomType.GUARDIAN;

            return bLimited ? RandRange(rng, 1, playerCount) : playerCount;
        }

        /// <summary>1레벨 -> 보스방 경로 존재 검증 (BFS)</summary>
        public static bool ValidatePathToBoss(IReadOnlyList<RoomNode> inMap, int lastRowIndex)
        {
            if (inMap == null || inMap.Count == 0) return false;

            var roomLookup = new Dictionary<int, RoomNode>();
            foreach (var node in inMap) roomLookup[node.RoomId] = node;

            var searchQueue = new Queue<int>();
            var visitedRoomIds = new HashSet<int>();

            foreach (var node in inMap)
            {
                if (node.Row == 0)
                {
                    searchQueue.Enqueue(node.RoomId);
                    visitedRoomIds.Add(node.RoomId);
                }
            }

            while (searchQueue.Count > 0)
            {
                int currentRoomId = searchQueue.Dequeue();

                if (!roomLookup.TryGetValue(currentRoomId, out var currentNode) || currentNode == null) continue;

                if (currentNode.Type == RoomType.BOSS || currentNode.Row == lastRowIndex)
                {
                    return true;
                }

                foreach (int connectedId in currentNode.ConnectedRoomIds)
                {
                    if (visitedRoomIds.Add(connectedId))
                    {
                        searchQueue.Enqueue(connectedId);
                    }
                }
            }

            return false;
        }

        /// <summary>정원 때문에 파티가 갇히는 구간이 없는지 검증</summary>
        public static bool ValidatePlayerCapacity(IReadOnlyList<RoomNode> inMap, int playerCount)
        {
            var roomLookup = new Dictionary<int, RoomNode>();
            var capacityByRow = new Dictionary<int, int>();
            foreach (var node in inMap)
            {
                roomLookup[node.RoomId] = node;
                capacityByRow.TryGetValue(node.Row, out int sum);
                capacityByRow[node.Row] = sum + node.MaxPlayers;
            }

            // 층 단위: 한 층의 정원 합이 인원보다 적으면 누군가는 그 층을 못 지나감
            foreach (var pair in capacityByRow)
            {
                if (pair.Value < playerCount)
                {
                    return false;
                }
            }

            // 방 단위: 그 방에 들어간 인원이 전원 다음 방으로 넘어갈 수 있어야 함
            //
            // 타입(몬스터/가디언)이나 분기 개수(2개 이상)로 거르면 구멍이 남는다.
            // 연결이 1개뿐인 방도 검사해야 한다 - 정원 4인 방이 정원 1인 방 하나로만 이어지면 3명이 갇힌다
            foreach (var sourceNode in inMap)
            {
                // 보스방(마지막 레벨)은 나가는 길이 없는 게 정상
                if (sourceNode.ConnectedRoomIds.Count == 0) continue;

                int combinedCapacity = 0;
                foreach (int targetId in sourceNode.ConnectedRoomIds)
                {
                    if (roomLookup.TryGetValue(targetId, out var target))
                    {
                        combinedCapacity += target.MaxPlayers;
                    }
                }

                // 이 방에 실제로 들어올 수 있는 최대 인원만큼은 빠져나갈 수 있어야 한다
                int incomingPlayers = Math.Min(sourceNode.MaxPlayers, playerCount);

                if (combinedCapacity < incomingPlayers)
                {
                    return false;
                }

                // 기획: 길이 2개 이상으로 갈라질 때는 정원 합이 "인원보다 커야" 한다.
                // 합이 인원과 같으면 분할이 한 가지로 강제되어 방을 고를 여지가 없어진다
                if (sourceNode.ConnectedRoomIds.Count >= 2 && combinedCapacity <= playerCount)
                {
                    return false;
                }
            }

            return true;
        }

        /// <summary>
        /// 방이 하나뿐인 줄(퀘스트 / 보스 포함)과 퀘스트 방이 없는 층의 첫 줄은 정원을 최대 인원으로. 예전에 저장된 지도도 고쳐짐
        /// </summary>
        public static void OpenSingleRoomRows(List<RoomNode> inOutRooms)
        {
            var roomsPerRow = new Dictionary<int, int>();
            bool bHasQuest = false;
            foreach (var node in inOutRooms)
            {
                roomsPerRow.TryGetValue(node.Row, out int c);
                roomsPerRow[node.Row] = c + 1;
                bHasQuest |= node.Type == RoomType.QUEST;
            }
            foreach (var node in inOutRooms)
            {
                // 방이 하나뿐인 줄 + 퀘스트 방이 없는 층의 첫 줄 (GenerateMap 과 같은 규칙)
                roomsPerRow.TryGetValue(node.Row, out int c);
                if (c == 1 || (!bHasQuest && node.Row == 0))
                {
                    node.MaxPlayers = FloorRules.MaxPartySize;
                }
            }
        }

        /// <summary>지금 지도의 줄 수 (퀘스트 줄이 없으면 TotalLevels - 1). 지도가 아직 없으면 totalLevels</summary>
        public static int GetLevelCount(IReadOnlyList<RoomNode> rooms, int totalLevels)
        {
            int maxRow = -1;
            if (rooms != null)
            {
                foreach (var node in rooms)
                {
                    maxRow = Math.Max(maxRow, node.Row);
                }
            }
            return maxRow >= 0 ? maxRow + 1 : totalLevels;
        }

        // ---------------------------------------------------------------
        // 언리얼 FMath 랜덤 자리
        // ---------------------------------------------------------------

        int RandRange(int min, int max) => RandRange(_rng, min, max);
        float RandRange(float min, float max) => FRandRange(_rng, min, max);

        // FMath::RandRange(int, int): 양 끝 포함
        internal static int RandRange(Random rng, int min, int max) => max <= min ? min : rng.Next(min, max + 1);

        // FMath::FRand: [0, 1)
        internal static float FRand(Random rng) => (float)rng.NextDouble();

        // FMath::FRandRange
        internal static float FRandRange(Random rng, float min, float max) => min + (max - min) * FRand(rng);

        static void AddUnique(List<int> list, int value)
        {
            if (!list.Contains(value)) list.Add(value);
        }

        static void Swap<T>(List<T> list, int a, int b)
        {
            T t = list[a];
            list[a] = list[b];
            list[b] = t;
        }
    }

    /// <summary>방 타입 배치 규칙 (DistributeRoomTypes 안의 람다. 테스트 / 화면에서도 씀)</summary>
    public static class RoomTypeRules
    {
        // 바로 앞뒤 방(경로상)과 같은 종류 금지
        public static bool IsNoRepeatType(RoomType t) =>
            t == RoomType.BREAK || t == RoomType.STORE || t == RoomType.GUARDIAN;

        // 같은 방에서 갈라지는 방끼리 중복 금지
        public static bool IsNoSiblingType(RoomType t) =>
            t == RoomType.BREAK || t == RoomType.STORE || t == RoomType.GUARDIAN || t == RoomType.EVENT;

        // 인원 제한이 붙는 타입 (GetRoomCapacity)
        public static bool IsCapacityLimited(RoomType t) => t == RoomType.MONSTER || t == RoomType.GUARDIAN;
    }
}
