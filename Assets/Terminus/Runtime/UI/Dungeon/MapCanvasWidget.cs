// 던전 지도 (언리얼 UMapCanvasWidget / WBP_MapCanvasWidget). Slay the Spire 방식으로 그린다
//  - 방 위치 = 격자(열, 줄) x 간격 + 방마다 고정된 작은 흔들림. 지도를 받을 때 한 번 계산해서 저장
//  - 연결선 = 그 좌표 사이에 점을 찍는 전용 레이어(MapEdgeLayer). 방 아이콘 뒤, 같은 캔버스 안
//  - 화면 위치를 조회하지 않음 -> 창 크기가 바뀌어도 선이 밀리거나 깨지지 않음
//
// WBP 구조: ScrollBox > Overlay > [종이 Image (T_Map_Paper 780x1200, 가로 가운데 / 세로 채움)] [MapCanvasPanel (가로 가운데)]
// 언리얼 지도 데이터 = MapManager 복제 -> 여기선 ClientState.Map (스냅샷), 내 위치 = ClientState.Me.Run
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Map;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class MapCanvasWidget : VisualElement
    {
        // 열 사이 간격(px). 아이콘이 실제로 보이는 크기보다 넉넉히 커야 옆 방과 안 붙음
        public float ColumnSpacing = 280f;   // 11-04 피드백: 양 옆이 너무 비어서 150 -> 280 (종이도 같이 넓어짐)

        // 줄(레벨) 사이 간격(px). 아이콘 보이는 크기 + 점선이 보일 틈
        public float RowSpacing = 130f;

        // 줄 안에서 방을 가운데로 고르게 펼치는 정도. 0 = 열 위치 그대로, 1 = 줄마다 가운데 기준 같은 간격
        // 열 위치 그대로면 방 2개짜리 줄이 한쪽(0,1열)에 쏠려 지도가 기울어 보이고 가로로 긴 선이 생김
        // 줄 안 순서는 안 바뀌므로 선이 엇갈리지 않음
        public float RowSpread = 0.5f;

        // 지도 가장자리 여백 (좌우, 위아래). 모자라면 아이콘이 안 잘리게 자동으로 늘림
        public Vector2 MapPadding = new Vector2(165f, 110f);

        // 방 위치 흔들림. 간격 대비 비율 (X, Y). 아이콘끼리 겹치지 않는 만큼으로 자동 제한됨
        public Vector2 PositionJitter = new Vector2(0.18f, 0.15f);

        // 방 아이콘 그림이 실제로 보이는 크기(px). 흔들림 제한에 씀
        public float RoomIconVisibleSize = 64f;

        // 방 위젯 왼쪽 위에서 아이콘 중심까지의 거리(px). 이 점이 방 좌표에 놓이고 선도 여기서 나감
        // (방 위젯은 [아이콘 상자 150x150][동료 칩] 가로 배치라 누가 고르면 폭이 늘어남 -> 가운데 기준이면 아이콘이 밀림)
        public Vector2 RoomIconCenter = new Vector2(RoomNodeWidget.BoxSize * 0.5f, RoomNodeWidget.BoxSize * 0.5f);

        // 종이 그림 기본 크기 (WBP Image_77 ImageSize)
        public Vector2 PaperSize = new Vector2(780f, 1200f);

        public readonly MapEdgeStyle EdgeStyle = new MapEdgeStyle();

        readonly ScrollView _scroll;
        readonly VisualElement _overlay;
        readonly VisualElement _paper;
        readonly VisualElement _canvas;
        MapEdgeLayer _edgeLayer;

        readonly Dictionary<int, RoomNodeWidget> _roomWidgets = new Dictionary<int, RoomNodeWidget>();
        readonly Dictionary<int, Vector2> _roomPositions = new Dictionary<int, Vector2>();
        List<RoomNode> _cachedMap = new List<RoomNode>();
        Vector2 _contentSize;

        bool _initialScrollDone;
        int _lastCenteredRoomId = int.MinValue;

        // 오른쪽 버튼 끌기 스크롤 (11-04 피드백: 지도 움직이기 편하게)
        bool _dragging;
        Vector2 _dragStart;
        float _dragStartOffset;

        public MapCanvasWidget()
        {
            DungeonStyle.Use(this);
            name = "MapCanvas";
            pickingMode = PickingMode.Ignore;

            EdgeStyle.DotTexture = DungeonStyle.Tex(MapTextures.InkDot);

            _scroll = new ScrollView(ScrollViewMode.Vertical) { name = "ScrollBox" };
            _scroll.AddToClassList("dm-map-scroll");
            DungeonStyle.Fill(_scroll);
            // 스크롤바를 보이게 (11-04 피드백)
            _scroll.verticalScrollerVisibility = ScrollerVisibility.AlwaysVisible;
            _scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _scroll.mouseWheelScrollSize = 60f;
            _scroll.contentViewport.style.overflow = Overflow.Hidden;
            Add(_scroll);

            _overlay = new VisualElement { name = "Overlay_0", pickingMode = PickingMode.Ignore };
            _overlay.style.width = Length.Percent(100);
            _overlay.style.height = PaperSize.y;
            _scroll.Add(_overlay);

            _paper = new VisualElement { name = "Paper", pickingMode = PickingMode.Ignore };
            DungeonStyle.SetImage(_paper, MapTextures.Paper);
            _paper.style.position = Position.Absolute;
            _paper.style.top = 0;
            _paper.style.bottom = 0;
            _paper.style.backgroundSize = new BackgroundSize(Length.Percent(100), Length.Percent(100));   // 언리얼 Image 처럼 칸에 맞춰 늘림
            _overlay.Add(_paper);

            _canvas = new VisualElement { name = "MapCanvasPanel", pickingMode = PickingMode.Ignore };
            _canvas.style.position = Position.Absolute;
            _canvas.style.top = 0;
            _overlay.Add(_canvas);

            SetPaperWidth(PaperSize.x);

            _scroll.RegisterCallback<PointerDownEvent>(OnPointerDown, TrickleDown.TrickleDown);
            _scroll.RegisterCallback<PointerMoveEvent>(OnPointerMove, TrickleDown.TrickleDown);
            _scroll.RegisterCallback<PointerUpEvent>(OnPointerUp, TrickleDown.TrickleDown);
            _scroll.RegisterCallback<PointerCaptureOutEvent>(_ => _dragging = false);
        }

        void SetPaperWidth(float width)
        {
            // 지도 칸이 가로 가운데 (ScrollBoxSlot / OverlaySlot HAlign_Center)
            _paper.style.width = width;
            _paper.style.left = Length.Percent(50);
            _paper.style.marginLeft = -width * 0.5f;
        }

        // ------------------------------------------------------------ 스크롤 끌기

        void OnPointerDown(PointerDownEvent e)
        {
            if (e.button != 1) return;
            _dragging = true;
            _dragStart = e.position;
            _dragStartOffset = _scroll.scrollOffset.y;
            _scroll.CapturePointer(e.pointerId);
            e.StopPropagation();
        }

        void OnPointerMove(PointerMoveEvent e)
        {
            if (!_dragging || !_scroll.HasPointerCapture(e.pointerId)) return;
            float y = _dragStartOffset - (e.position.y - _dragStart.y);
            _scroll.scrollOffset = new Vector2(0f, Mathf.Clamp(y, 0f, MaxScroll()));
        }

        void OnPointerUp(PointerUpEvent e)
        {
            if (!_dragging || e.button != 1) return;
            _dragging = false;
            if (_scroll.HasPointerCapture(e.pointerId)) _scroll.ReleasePointer(e.pointerId);
            e.StopPropagation();
        }

        float MaxScroll()
        {
            float view = _scroll.contentViewport.layout.height;
            float content = _overlay.layout.height;
            if (float.IsNaN(view) || float.IsNaN(content)) return 0f;
            return Mathf.Max(0f, content - view);
        }

        // ------------------------------------------------------------ 갱신 (지도 스냅샷 / 파티 스냅샷이 바뀔 때 DungeonView 가 부름)

        public void Refresh()
        {
            var map = ClientState.Map;
            if (map != null && map.Rooms != null && map.Rooms.Count > 0) BuildMapUI(map.Rooms);

            if (_roomWidgets.Count == 0) return;
            RefreshRoomStates();

            // 방을 옮겼으면 다음에 고를 줄을 가운데로
            var me = ClientState.Me;
            int current = me != null && me.Run != null ? me.Run.CurrentRoomId : -1;
            if (_initialScrollDone && current != _lastCenteredRoomId) CenterOnSelectableRooms();
        }

        static bool IsSameMap(List<RoomNode> a, List<RoomNode> b)
        {
            if (a.Count != b.Count) return false;
            for (int i = 0; i < a.Count; ++i)
            {
                var l = a[i];
                var r = b[i];
                if (l.RoomId != r.RoomId || l.Row != r.Row || l.Col != r.Col || l.Type != r.Type || l.MaxPlayers != r.MaxPlayers
                    || l.SpecialRoom != r.SpecialRoom || !l.ConnectedRoomIds.SequenceEqual(r.ConnectedRoomIds))
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>전체 지도 세팅. 지도 데이터가 이미 그려진 것과 같으면 위젯은 그대로 두고 상태만 갱신</summary>
        public void BuildMapUI(List<RoomNode> mapData)
        {
            if (mapData == null || mapData.Count == 0) return;

            // 같은 지도가 다시 들어온 경우 위젯을 새로 만들지 않음 (스크롤 위치가 날아가고 전체가 깜빡임)
            if (_roomWidgets.Count > 0 && IsSameMap(_cachedMap, mapData)) return;

            _canvas.Clear();
            _roomWidgets.Clear();
            _cachedMap = mapData.Select(r => r.Clone()).ToList();

            ComputeLayout(_cachedMap);

            _canvas.style.width = _contentSize.x;
            _canvas.style.height = _contentSize.y;
            _canvas.style.left = Length.Percent(50);
            _canvas.style.marginLeft = -_contentSize.x * 0.5f;
            _overlay.style.height = Mathf.Max(PaperSize.y, _contentSize.y);

            // 종이 배경을 지도 폭에 맞춤. WBP 는 780 고정이라 열 간격을 넓히면 방이 종이 밖으로 나감
            SetPaperWidth(Mathf.Max(PaperSize.x, _contentSize.x + 40f));

            // 연결선 레이어: 캔버스 첫 자식(맨 뒤), 지도 전체 크기로 (0,0) 에
            _edgeLayer = new MapEdgeLayer();
            _edgeLayer.SetEdgeStyle(EdgeStyle);
            _edgeLayer.style.position = Position.Absolute;
            _edgeLayer.style.left = 0;
            _edgeLayer.style.top = 0;
            _edgeLayer.style.width = _contentSize.x;
            _edgeLayer.style.height = _contentSize.y;
            _canvas.Add(_edgeLayer);

            // 방 아이콘: 아이콘 중심이 방 좌표에 오게 왼쪽 위 기준으로 배치 (초상화가 붙어 폭이 늘어도 아이콘은 그대로)
            foreach (var node in _cachedMap)
            {
                if (!_roomPositions.TryGetValue(node.RoomId, out var pos)) continue;
                var w = new RoomNodeWidget();
                w.SetupRoomNode(node);
                w.Clicked += HandleRoomClicked;
                w.style.position = Position.Absolute;
                w.style.left = pos.x - RoomIconCenter.x;
                w.style.top = pos.y - RoomIconCenter.y;
                _canvas.Add(w);
                _roomWidgets[node.RoomId] = w;
            }

            // 경로 제한 + 선택 표시 + 연결선 상태 초기 적용
            RefreshRoomStates();

            // 새 지도(처음 / 다음 층)를 그리면 고를 줄(맨 아래 퀘스트방)이 가운데 오게.
            // 그 뒤로는 방을 옮겼을 때만 다시 맞춤 (Refresh / 지도 화면이 다시 보일 때)
            _initialScrollDone = true;
            CenterOnSelectableRooms();
        }

        // ------------------------------------------------------------ 배치

        // 언리얼 FRandomStream (방마다 고정된 흔들림이 언리얼과 같은 자리에 오게)
        struct UnrealRandomStream
        {
            uint _seed;

            public UnrealRandomStream(int seed) { _seed = unchecked((uint)seed); }

            float GetFraction()
            {
                _seed = unchecked(_seed * 196314165u + 907633515u);
                uint bits = 0x3F800000u | (_seed >> 9);
                return BitConverter.ToSingle(BitConverter.GetBytes(bits), 0) - 1f;
            }

            public float FRandRange(float min, float max) => min + (max - min) * GetFraction();
        }

        void ComputeLayout(List<RoomNode> mapData)
        {
            _roomPositions.Clear();

            int maxRow = 0, maxCol = 0;
            var rowRooms = new SortedDictionary<int, List<RoomNode>>();
            foreach (var node in mapData)
            {
                maxRow = Math.Max(maxRow, node.Row);
                maxCol = Math.Max(maxCol, node.Col);
                if (!rowRooms.TryGetValue(node.Row, out var list)) rowRooms[node.Row] = list = new List<RoomNode>();
                list.Add(node);
            }

            // 격자 폭 / 높이. 방 중심이 놓이는 범위
            float gridWidth = maxCol * ColumnSpacing;
            float gridHeight = maxRow * RowSpacing;

            // 흔들림 한도: 이웃 방과 아이콘이 겹치지 않을 만큼만 (양쪽이 서로 다가와도 틈이 남게)
            float jitterMaxX = Mathf.Min(PositionJitter.x * ColumnSpacing, Mathf.Max(0f, (ColumnSpacing - RoomIconVisibleSize) * 0.5f - 10f));
            float jitterMaxY = Mathf.Min(PositionJitter.y * RowSpacing, Mathf.Max(0f, (RowSpacing - RoomIconVisibleSize) * 0.5f - 20f));

            // 여백: 끝 열 방이 흔들려 나가도 아이콘 상자가 안 잘리게
            var edgePadding = new Vector2(
                Mathf.Max(MapPadding.x, RoomIconCenter.x + jitterMaxX + 20f),
                Mathf.Max(MapPadding.y, RoomIconCenter.y + jitterMaxY + 20f));

            _contentSize = new Vector2(gridWidth, gridHeight) + edgePadding * 2f;

            foreach (var pair in rowRooms)
            {
                int row = pair.Key;
                var rooms = pair.Value.OrderBy(r => r.Col).ToList();
                int count = rooms.Count;

                // 퀘스트(맨 아래) / 보스(맨 위) 는 지도 한가운데, 흔들림 없음
                bool anchorRow = row == 0 || row == maxRow;

                // 방이 하나뿐인 줄도 가로는 한가운데 (위아래 방이 전부 모이는 자리라 끝에 있으면 선이 길게 가로지름)
                bool centerX = anchorRow || count == 1;

                // 이 줄을 가운데 기준으로 고르게 펼쳤을 때의 폭. 방 수에 비례 (4개면 전체 폭, 2개면 한 칸)
                float spreadSpan = maxCol > 0 && count > 1 ? gridWidth * (count - 1) / maxCol : 0f;

                for (int i = 0; i < count; ++i)
                {
                    var node = rooms[i];

                    // 방마다 고정된 흔들림 -> 다시 그려도 같은 자리
                    var stream = new UnrealRandomStream(node.RoomId * 100 + node.Row * 10 + node.Col);
                    float jitterX = centerX ? 0f : stream.FRandRange(-jitterMaxX, jitterMaxX);
                    float jitterY = anchorRow ? 0f : stream.FRandRange(-jitterMaxY, jitterMaxY);

                    // 가로 = 열 위치와 "줄 가운데 기준 고른 간격" 사이. 둘 다 열 순서를 지키므로 선이 엇갈리지 않음
                    float x = gridWidth * 0.5f;
                    if (!centerX)
                    {
                        float columnX = node.Col * ColumnSpacing;
                        float spreadX = gridWidth * 0.5f - spreadSpan * 0.5f + spreadSpan * i / (count - 1);
                        x = Mathf.Lerp(columnX, spreadX, RowSpread) + jitterX;
                    }

                    // 세로 = 아래가 1레벨
                    float y = (maxRow - row) * RowSpacing + jitterY;

                    _roomPositions[node.RoomId] = new Vector2(x, y) + edgePadding;
                }
            }
        }

        // ------------------------------------------------------------ 상태

        /// <summary>내 현재 위치 기준으로 이 방을 누를 수 있는가</summary>
        bool IsRoomSelectable(RoomNode node, PlayerInfo me)
        {
            // 내 정보가 아직 없거나, 1레벨(CurrentMapLevel == 0)일 때는 Row 0 (퀘스트방)만
            if (me == null || me.Run == null || me.Run.CurrentMapLevel == 0) return node.Row == 0;

            // 2레벨 이후: 현재 방과 연결된 방만
            var current = _cachedMap.Find(n => n.RoomId == me.Run.CurrentRoomId);
            return current != null && current.ConnectedRoomIds.Contains(node.RoomId);
        }

        /// <summary>이 방에서 앞으로 갈 수 있는 방들 (-1 = 출발 전 -> 전부). 여기 없는 앞쪽 방은 "못 감"</summary>
        HashSet<int> CollectReachableRooms(int fromRoomId)
        {
            var reachable = new HashSet<int>();
            if (fromRoomId < 0)
            {
                foreach (var n in _cachedMap) reachable.Add(n.RoomId);
                return reachable;
            }

            var byId = _cachedMap.ToDictionary(n => n.RoomId);
            // 연결은 다음 줄로만 이어지므로 앞으로 훑기만 하면 됨
            var stack = new Stack<int>();
            stack.Push(fromRoomId);
            while (stack.Count > 0)
            {
                if (!byId.TryGetValue(stack.Pop(), out var node)) continue;
                foreach (var next in node.ConnectedRoomIds)
                {
                    if (reachable.Add(next)) stack.Push(next);
                }
            }
            return reachable;
        }

        /// <summary>이미 만들어진 방 위젯들의 상태(선택 가능 여부 + 선택 표시)만 갱신. 위젯 재생성 / 스크롤 이동 없음</summary>
        public void RefreshRoomStates()
        {
            var me = ClientState.Me;

            // 지도 시안 상태: 지나옴 / 현재 위치(먹 동그라미) / 갈 수 있음(빛) / 못 감(흐림) / 기본
            var visited = new HashSet<int>();
            int currentRoomId = -1;
            if (me != null && me.Run != null)
            {
                foreach (var id in me.Run.VisitedRoomIds) visited.Add(id);
                if (me.Run.CurrentMapLevel > 0) currentRoomId = me.Run.CurrentRoomId;
            }
            var reachable = CollectReachableRooms(currentRoomId);

            foreach (var node in _cachedMap)
            {
                if (!_roomWidgets.TryGetValue(node.RoomId, out var w)) continue;
                bool selectable = IsRoomSelectable(node, me);
                w.SetRoomSelectable(selectable);

                var state = RoomNodeState.Default;
                if (node.RoomId == currentRoomId) state = RoomNodeState.Current;
                else if (visited.Contains(node.RoomId)) state = RoomNodeState.Visited;
                else if (selectable) state = RoomNodeState.Available;
                else if (!reachable.Contains(node.RoomId)) state = RoomNodeState.Unreachable;
                w.SetVisualState(state);
            }

            RefreshAllRoomSelections();
            RefreshEdges(me);
        }

        /// <summary>전체 방 위젯들의 선택 상태(테두리 / 동료 칩) 갱신</summary>
        public void RefreshAllRoomSelections()
        {
            // 나간 사람은 빼고 (언리얼 GameState PlayerArray)
            var players = ClientState.Party.Players.Where(p => p.bConnected).ToList();
            ulong localId = ClientState.MyId;
            foreach (var w in _roomWidgets.Values) w.RefreshSelectionState(players, localId);
        }

        void RefreshEdges(PlayerInfo me)
        {
            if (_edgeLayer == null) return;

            // 지나온 길 = 방문 순서상 연속한 두 방 + 지금 고른 다음 방으로 가는 길
            var taken = new HashSet<(int, int)>();
            int currentRoomId = -1;
            if (me != null && me.Run != null)
            {
                var run = me.Run;
                for (int k = 0; k + 1 < run.VisitedRoomIds.Count; ++k) taken.Add((run.VisitedRoomIds[k], run.VisitedRoomIds[k + 1]));
                if (run.CurrentMapLevel > 0)
                {
                    currentRoomId = run.CurrentRoomId;
                    if (run.SelectedRoomId >= 0) taken.Add((run.CurrentRoomId, run.SelectedRoomId));
                }
            }

            var edges = new List<MapEdgeDraw>();
            foreach (var node in _cachedMap)
            {
                if (!_roomPositions.TryGetValue(node.RoomId, out var from)) continue;
                foreach (var toId in node.ConnectedRoomIds)
                {
                    if (!_roomPositions.TryGetValue(toId, out var to)) continue;
                    var state = MapEdgeState.Default;
                    if (taken.Contains((node.RoomId, toId))) state = MapEdgeState.Taken;
                    else if (node.RoomId == currentRoomId) state = MapEdgeState.Available;
                    edges.Add(new MapEdgeDraw { From = from, To = to, State = state });
                }
            }
            _edgeLayer.SetEdges(edges);
        }

        // ------------------------------------------------------------ 스크롤

        /// <summary>지금 고를 수 있는 방들(다음에 갈 줄)이 화면 세로 가운데에 오게. 레이아웃이 잡힌 다음에 적용</summary>
        public void CenterOnSelectableRooms()
        {
            schedule.Execute(() => ApplyCenterScroll(5));
        }

        void ApplyCenterScroll(int retriesLeft)
        {
            if (_roomPositions.Count == 0) return;

            // 스크롤 상자 크기를 아직 모르면 다음 프레임에 다시
            float viewHeight = _scroll.contentViewport.layout.height;
            if (float.IsNaN(viewHeight) || viewHeight <= 0f)
            {
                if (retriesLeft > 0) schedule.Execute(() => ApplyCenterScroll(retriesLeft - 1));
                return;
            }

            var me = ClientState.Me;

            // 고를 수 있는 방들의 세로 평균 (같은 줄이라 흔들림 정도만 차이남)
            float sumY = 0f;
            int count = 0;
            foreach (var node in _cachedMap)
            {
                if (_roomPositions.TryGetValue(node.RoomId, out var pos) && IsRoomSelectable(node, me))
                {
                    sumY += pos.y;
                    ++count;
                }
            }

            // 고를 방이 없으면(마지막 방 등) 지금 있는 방
            if (count == 0 && me != null && me.Run != null && _roomPositions.TryGetValue(me.Run.CurrentRoomId, out var cur))
            {
                sumY = cur.y;
                count = 1;
            }
            if (count == 0) return;

            _lastCenteredRoomId = me != null && me.Run != null ? me.Run.CurrentRoomId : -1;

            // 방 좌표는 캔버스 기준 = 스크롤 내용 기준 (캔버스가 맨 위). 지도 끝을 넘지 않게
            float contentHeight = Mathf.Max(PaperSize.y, _contentSize.y);
            float maxOffset = Mathf.Max(0f, contentHeight - viewHeight);
            float offset = Mathf.Clamp(sumY / count - viewHeight * 0.5f, 0f, maxOffset);
            _scroll.scrollOffset = new Vector2(0f, offset);
        }

        // ------------------------------------------------------------ 클릭

        void HandleRoomClicked(int roomId)
        {
            // 싱글은 즉시 입장, 멀티는 전원 선택 대기 - 판단은 서버가 함 (언리얼 PC->Server_RequestSelectRoom)
            NetBus.Request(new SelectRoomReq { RoomId = roomId });
        }
    }
}
