// 지도 생성 속성 테스트 (언리얼엔 지도 테스트가 없음). 시드 여러 개 x 인원 1~4 x 층 1~6 으로 만든 지도가 조건을 지키는지
using System;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;
using Terminus.Map;

namespace Terminus.Tests
{
    public class MapGeneratorTests
    {
        const int SeedsPerCase = 40;

        static IEnumerable<TestCaseData> PlayerFloorCases()
        {
            for (int players = 1; players <= 4; ++players)
            {
                for (int floor = 1; floor <= 6; ++floor)
                {
                    yield return new TestCaseData(players, floor).SetName($"Generate_Invariants_{players}p_Floor{floor}");
                }
            }
        }

        [TestCaseSource(nameof(PlayerFloorCases))]
        public void Generate_Invariants(int players, int floor)
        {
            var settings = new MapSettings();
            var failures = new List<string>();

            for (int seed = 0; seed < SeedsPerCase; ++seed)
            {
                var gen = new MapGenerator(settings, new Random(seed * 7919 + players * 31 + floor));
                var result = gen.Generate(floor, players);

                foreach (var e in MapInvariants.Check(result, settings, floor, players))
                {
                    failures.Add($"seed {seed}: {e}");
                }
            }

            Assert.That(failures, Is.Empty, string.Join("\n", failures.Take(20)));
        }

        [Test]
        public void Generate_RowCount_WithAndWithoutQuestRow()
        {
            var settings = new MapSettings();
            var gen = new MapGenerator(settings, new Random(1));

            for (int floor = 1; floor <= 6; ++floor)
            {
                var rooms = gen.GenerateMap(floor, 4);
                int rows = MapGenerator.GetLevelCount(rooms, settings.TotalLevels);
                // 퀘스트 방이 있는 층(1 / 3 / 5)은 12줄, 테마 끝 층(2 / 4 / 6)은 퀘스트 줄이 빠져 11줄
                Assert.That(rows, Is.EqualTo(floor % 2 == 1 ? 12 : 11), $"floor {floor}");
                Assert.That(rooms.Any(r => r.Type == RoomType.QUEST), Is.EqualTo(floor % 2 == 1));
            }
        }

        [Test]
        public void Generate_SameSeed_SameMap()
        {
            var settings = new MapSettings();
            for (int seed = 0; seed < 10; ++seed)
            {
                var a = new MapGenerator(settings, new Random(seed)).GenerateMap(3, 3);
                var b = new MapGenerator(settings, new Random(seed)).GenerateMap(3, 3);
                Assert.That(MapInvariants.Serialize(a), Is.EqualTo(MapInvariants.Serialize(b)));
            }
        }

        [Test]
        public void Generate_DifferentSeeds_DifferentMaps()
        {
            var settings = new MapSettings();
            var maps = new HashSet<string>();
            for (int seed = 0; seed < 20; ++seed)
            {
                maps.Add(MapInvariants.Serialize(new MapGenerator(settings, new Random(seed)).GenerateMap(1, 2)));
            }
            Assert.That(maps.Count, Is.GreaterThan(15));
        }

        // 정원 검증(ValidatePlayerCapacity)은 방 단위 조건. 실제로 각자 고를 수 있는 모든 경우를 따라가도 갇히는 파티가 없는지
        [Test]
        public void Generate_PartyNeverTrapped_Exhaustive()
        {
            var settings = new MapSettings();
            var traps = new List<string>();
            for (int players = 2; players <= 4; ++players)
            {
                for (int floor = 1; floor <= 2; ++floor)
                {
                    for (int seed = 0; seed < 60; ++seed)
                    {
                        var rooms = new MapGenerator(settings, new Random(seed * 13 + players)).GenerateMap(floor, players);
                        int trapRow = MapInvariants.FindTrapRow(rooms, players);
                        if (trapRow >= 0) traps.Add($"{players}p floor {floor} seed {seed}: 줄 {trapRow} 에서 갇힘");
                    }
                }
            }
            Assert.That(traps, Is.Empty, string.Join("\n", traps.Take(20)));
        }

        // 타입 개수를 비율로 먼저 확정하므로 많이 만들면 비율에 가깝게 나옴 (규칙 때문에 몬스터로 바뀐 만큼 몬스터가 조금 많음)
        [Test]
        public void Generate_TypeRatio_CloseToWeights()
        {
            var settings = new MapSettings();
            var counts = new Dictionary<RoomType, int>();
            int total = 0;
            int mapsWithoutStore = 0;

            for (int seed = 0; seed < 300; ++seed)
            {
                var result = new MapGenerator(settings, new Random(seed)).Generate(1, 4);
                int distFirst = result.FirstNormalRow + 1;
                int distLast = result.LastRowIndex - 2;
                var dist = result.Rooms.Where(r => r.Row >= distFirst && r.Row <= distLast).ToList();
                foreach (var r in dist)
                {
                    counts.TryGetValue(r.Type, out int c);
                    counts[r.Type] = c + 1;
                    total++;
                }
                if (!dist.Any(r => r.Type == RoomType.STORE)) mapsWithoutStore++;
            }

            float totalWeight = settings.RoomTypeWeights.Values.Sum();
            foreach (var kv in settings.RoomTypeWeights)
            {
                counts.TryGetValue(kv.Key, out int c);
                double ratio = (double)c / total;
                double expected = kv.Value / totalWeight;
                Assert.That(ratio, Is.EqualTo(expected).Within(0.06), $"{kv.Key}: {ratio:P1} (기대 {expected:P0})");
            }
            // 방마다 추첨하던 때는 상점 0개인 지도가 10% 가까이 나왔음
            Assert.That(mapsWithoutStore, Is.LessThan(300 * 0.02), "상점 없는 지도");
        }

        [Test]
        public void Generate_CustomSizes_StillValid()
        {
            // 줄을 늘리면(TotalLevels 15) MaxRetries 1000 으로도 4인 지도가 1~9% 실패함 (실측). 재시도 수는 12줄 기준으로 정한 값

            var sizes = new[] { (levels: 3, cols: 4), (levels: 4, cols: 2), (levels: 6, cols: 3), (levels: 12, cols: 5), (levels: 12, cols: 2) };
            foreach (var s in sizes)
            {
                var settings = new MapSettings { TotalLevels = s.levels, MaxRoomsPerRow = s.cols };
                for (int floor = 1; floor <= 2; ++floor)
                {
                    for (int players = 1; players <= 4; ++players)
                    {
                        for (int seed = 0; seed < 10; ++seed)
                        {
                            var result = new MapGenerator(settings, new Random(seed)).Generate(floor, players);
                            var errors = MapInvariants.Check(result, settings, floor, players);
                            Assert.That(errors, Is.Empty, $"levels {s.levels} cols {s.cols} floor {floor} {players}p seed {seed}:\n" + string.Join("\n", errors.Take(10)));
                        }
                    }
                }
            }
        }

        [Test]
        public void Generate_DegenerateSettings_DoNotThrow()
        {
            // 줄 2개 (퀘스트 + 보스 / 일반 + 보스), 한 줄에 방 1개 (일반 줄이 빌 수 있어 실패할 수 있음 - 언리얼 그대로)
            var tiny = new MapSettings { TotalLevels = 2, MaxRoomsPerRow = 1, MaxRetries = 50 };
            for (int floor = 1; floor <= 2; ++floor)
            {
                var result = new MapGenerator(tiny, new Random(5)).Generate(floor, 4);
                Assert.That(result.Rooms.Count, Is.GreaterThanOrEqualTo(1));
                Assert.That(result.Rooms.Count(r => r.Type == RoomType.BOSS), Is.EqualTo(1));
            }

            var oneCol = new MapSettings { MaxRoomsPerRow = 1, MaxRetries = 20 };
            Assert.DoesNotThrow(() => new MapGenerator(oneCol, new Random(1)).Generate(1, 4));
        }

        [Test]
        public void Generate_PlayerCountClamped()
        {
            var result = new MapGenerator(new MapSettings(), new Random(3)).Generate(1, 9);
            Assert.That(MapInvariants.Check(result, new MapSettings(), 1, 4), Is.Empty);
            result = new MapGenerator(new MapSettings(), new Random(3)).Generate(1, 0);
            Assert.That(MapInvariants.Check(result, new MapSettings(), 1, 1), Is.Empty);
        }

        // ---------------------------------------------------------------
        // 부분 함수
        // ---------------------------------------------------------------

        [Test]
        public void ConnectRows_NoCrossing_AllConnected()
        {
            var rng = new Random(42);
            for (int iter = 0; iter < 3000; ++iter)
            {
                var map = new List<RoomNode>();
                int id = 1;
                var lower = new List<int>();
                var upper = new List<int>();
                foreach (int col in RandomCols(rng, 5)) { lower.Add(map.Count); map.Add(MapGenerator.CreateRoom(id++, 0, col)); }
                foreach (int col in RandomCols(rng, 5)) { upper.Add(map.Count); map.Add(MapGenerator.CreateRoom(id++, 1, col)); }

                MapGenerator.ConnectRowsWithoutCrossing(map, lower, upper, rng);

                var edges = new List<(int a, int b)>();
                foreach (int li in lower)
                {
                    Assert.That(map[li].ConnectedRoomIds, Is.Not.Empty, "아래 방이 위로 안 이어짐");
                    Assert.That(map[li].ConnectedRoomIds, Is.Unique);
                    foreach (int to in map[li].ConnectedRoomIds)
                    {
                        var t = map.Find(r => r.RoomId == to);
                        Assert.That(t.Row, Is.EqualTo(1));
                        edges.Add((map[li].Col, t.Col));
                    }
                }
                foreach (int ui in upper)
                {
                    Assert.That(lower.Any(li => map[li].ConnectedRoomIds.Contains(map[ui].RoomId)), "위 방으로 오는 길 없음");
                    Assert.That(map[ui].ConnectedRoomIds, Is.Empty);
                }
                for (int i = 0; i < edges.Count; ++i)
                    for (int j = i + 1; j < edges.Count; ++j)
                        Assert.That((edges[i].a - edges[j].a) * (edges[i].b - edges[j].b), Is.GreaterThanOrEqualTo(0), "선 교차");
            }
        }

        static List<int> RandomCols(Random rng, int maxCols)
        {
            var cols = Enumerable.Range(0, maxCols).Where(_ => rng.NextDouble() < 0.5).ToList();
            if (cols.Count == 0) cols.Add(rng.Next(maxCols));
            return cols;
        }

        [Test]
        public void ConnectRows_EmptySide_NoEdges()
        {
            var map = new List<RoomNode> { MapGenerator.CreateRoom(1, 0, 0) };
            MapGenerator.ConnectRowsWithoutCrossing(map, new List<int> { 0 }, new List<int>(), new Random(1));
            Assert.That(map[0].ConnectedRoomIds, Is.Empty);
        }

        [Test]
        public void ValidatePathToBoss_Cases()
        {
            Assert.That(MapGenerator.ValidatePathToBoss(new List<RoomNode>(), 1), Is.False);

            var a = MapGenerator.CreateRoom(1, 0, 0);
            var b = MapGenerator.CreateRoom(2, 1, 0);
            var boss = MapGenerator.CreateRoom(3, 2, 0, RoomType.BOSS, 4);
            var map = new List<RoomNode> { a, b, boss };
            Assert.That(MapGenerator.ValidatePathToBoss(map, 2), Is.False, "연결 없음");

            a.ConnectedRoomIds.Add(2);
            Assert.That(MapGenerator.ValidatePathToBoss(map, 2), Is.False, "중간에서 끊김");

            b.ConnectedRoomIds.Add(3);
            Assert.That(MapGenerator.ValidatePathToBoss(map, 2), Is.True);
        }

        [Test]
        public void ValidatePlayerCapacity_Cases()
        {
            // 정원 4인 방이 정원 1인 방 하나로만 이어지면 3명이 갇힌다
            var start = MapGenerator.CreateRoom(1, 0, 0, RoomType.QUEST, 4);
            var narrow = MapGenerator.CreateRoom(2, 1, 0, RoomType.MONSTER, 1);
            var wide = MapGenerator.CreateRoom(3, 1, 1, RoomType.MONSTER, 3);
            var boss = MapGenerator.CreateRoom(4, 2, 0, RoomType.BOSS, 4);
            narrow.ConnectedRoomIds.Add(4);
            wide.ConnectedRoomIds.Add(4);
            var map = new List<RoomNode> { start, narrow, wide, boss };

            start.ConnectedRoomIds.Add(2);
            Assert.That(MapGenerator.ValidatePlayerCapacity(map, 4), Is.False, "4인 방 -> 1인 방 하나");

            // 갈라지는데 정원 합(1 + 3) == 인원 4 -> 분할이 하나로 강제됨 -> 실패
            start.ConnectedRoomIds.Add(3);
            Assert.That(MapGenerator.ValidatePlayerCapacity(map, 4), Is.False, "갈림길 정원 합 == 인원");

            // 합이 인원보다 크면 통과
            wide.MaxPlayers = 4;
            Assert.That(MapGenerator.ValidatePlayerCapacity(map, 4), Is.True);

            // 줄 정원 합 < 인원
            narrow.MaxPlayers = 1;
            wide.MaxPlayers = 1;
            Assert.That(MapGenerator.ValidatePlayerCapacity(map, 4), Is.False, "줄 정원 합 < 인원");
            Assert.That(MapGenerator.ValidatePlayerCapacity(map, 1), Is.True, "혼자면 정원 1 로 충분");
        }

        [Test]
        public void GetRoomCapacity_OnlyMonsterAndGuardianLimited()
        {
            var rng = new Random(7);
            for (int players = 1; players <= 4; ++players)
            {
                var seen = new HashSet<int>();
                for (int i = 0; i < 200; ++i)
                {
                    int cap = MapGenerator.GetRoomCapacity(RoomType.MONSTER, players, rng);
                    Assert.That(cap, Is.InRange(1, players));
                    seen.Add(cap);
                    Assert.That(MapGenerator.GetRoomCapacity(RoomType.GUARDIAN, players, rng), Is.InRange(1, players));
                    foreach (var t in new[] { RoomType.BREAK, RoomType.STORE, RoomType.EVENT, RoomType.QUEST, RoomType.BOSS })
                    {
                        Assert.That(MapGenerator.GetRoomCapacity(t, players, rng), Is.EqualTo(players));
                    }
                }
                Assert.That(seen.Count, Is.EqualTo(players), "1 ~ 인원 전부 나와야 함");
            }
        }

        [Test]
        public void OpenSingleRoomRows_OpensSingleRowsAndFirstRowWithoutQuest()
        {
            // 퀘스트 있는 지도: 방 하나 줄만 열림
            var rooms = new List<RoomNode>
            {
                MapGenerator.CreateRoom(1, 0, 2, RoomType.QUEST, 4),
                MapGenerator.CreateRoom(2, 1, 0, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(3, 1, 1, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(4, 2, 1, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(5, 3, 2, RoomType.BOSS, 4),
            };
            MapGenerator.OpenSingleRoomRows(rooms);
            Assert.That(rooms.Select(r => r.MaxPlayers), Is.EqualTo(new[] { 4, 1, 1, 4, 4 }));

            // 퀘스트 없는 지도: 첫 줄도 열림
            var noQuest = new List<RoomNode>
            {
                MapGenerator.CreateRoom(1, 0, 0, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(2, 0, 1, RoomType.MONSTER, 2),
                MapGenerator.CreateRoom(3, 1, 0, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(4, 1, 1, RoomType.MONSTER, 1),
                MapGenerator.CreateRoom(5, 2, 2, RoomType.BOSS, 4),
            };
            MapGenerator.OpenSingleRoomRows(noQuest);
            Assert.That(noQuest.Select(r => r.MaxPlayers), Is.EqualTo(new[] { 4, 4, 1, 1, 4 }));
        }

        [Test]
        public void GetLevelCount_EmptyMapUsesTotalLevels()
        {
            Assert.That(MapGenerator.GetLevelCount(new List<RoomNode>(), 12), Is.EqualTo(12));
            Assert.That(MapGenerator.GetLevelCount(null, 12), Is.EqualTo(12));
            Assert.That(MapGenerator.GetLevelCount(new List<RoomNode> { MapGenerator.CreateRoom(1, 10, 0) }, 12), Is.EqualTo(11));
        }

        [Test]
        public void DistributeRoomTypes_ZeroWeights_AllMonster()
        {
            var map = new List<RoomNode>();
            for (int i = 0; i < 6; ++i) map.Add(MapGenerator.CreateRoom(i + 1, i / 2, i % 2, RoomType.STORE));
            var weights = new Dictionary<RoomType, float> { { RoomType.STORE, 0f } };
            MapGenerator.DistributeRoomTypes(map, 0, 2, -1, weights, new Random(1));
            Assert.That(map.All(r => r.Type == RoomType.MONSTER));
        }

        [Test]
        public void DistributeRoomTypes_OnlyTouchesGivenRows()
        {
            var map = new List<RoomNode>();
            for (int row = 0; row < 5; ++row)
                for (int col = 0; col < 3; ++col)
                    map.Add(MapGenerator.CreateRoom(map.Count + 1, row, col, RoomType.QUEST));
            MapGenerator.DistributeRoomTypes(map, 1, 3, -1, MapSettings.DefaultRoomTypeWeights(), new Random(2));
            Assert.That(map.Where(r => r.Row == 0 || r.Row == 4).All(r => r.Type == RoomType.QUEST));
            Assert.That(map.Where(r => r.Row >= 1 && r.Row <= 3).All(r => r.Type != RoomType.QUEST));
        }
    }
}
