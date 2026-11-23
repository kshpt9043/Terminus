// 지도 연결선 레이어 (언리얼 UMapEdgeLayer / WBP_MapEdgeLayer). 지도 캔버스 안, 방 아이콘들 뒤에 지도와 같은 크기로 깔림
//
// Slay the Spire 처럼 두 방 사이에 작은 점을 일정 간격으로 찍어 점선을 만든다
// 좌표는 지도 위젯이 계산한 지도 좌표를 그대로 받음 -> 화면 위치 조회가 없어서 창 크기가 바뀌어도 밀리지 않고,
// 스크롤 / 잘라내기 / 배율이 방 아이콘과 똑같이 적용됨
// 언리얼 NativePaint(MakeRotatedBox) 자리 = generateVisualContent 에서 점 텍스처를 돌린 사각형으로 찍음
using System.Collections.Generic;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    // 지도 연결선 상태. 그리는 순서도 이 순서 (뒤가 위에 그려짐)
    public enum MapEdgeState
    {
        Default,     // 그 외 모든 길
        Available,   // 지금 위치에서 갈 수 있는 길
        Taken,       // 지나온 길 + 지금 고른 길
    }

    /// <summary>점선 모양 (언리얼 FMapEdgeStyle. WBP_MapCanvasWidget 은 점 이미지만 T_Map_InkDot 으로 바꿈)</summary>
    public sealed class MapEdgeStyle
    {
        // 점 이미지. 없으면 둥근 점. 이미지는 선 방향으로 회전돼서 찍힘 (가로가 선 방향)
        public Texture2D DotTexture;
        public Vector2 DotSize = new Vector2(20f, 20f);

        // 점 사이 간격 (지도 좌표 기준)
        public float DotSpacing = 19f;

        // 선 양 끝을 비우는 길이. 방 아이콘이 실제로 보이는 반지름쯤 -> 점이 아이콘 밑으로 파고들지 않음
        public float EndPadding = 42f;

        public Color DefaultColor = UiColor.FromLinear(0.451f, 0.417f, 0.381f, 0.6f);   // #B3ADA6 60% 흐린 먹
        public Color AvailableColor = UiColor.FromLinear(1f, 0.846f, 0.351f, 1f);       // #FFEDA0 등불 먹
        public Color TakenColor = UiColor.FromLinear(0.027f, 0.024f, 0.020f, 1f);       // #2E2B27 마른 먹

        // 갈 수 있는 길 / 지나온 길의 점 크기 배율
        public float EmphasisDotScale = 1.15f;

        // 갈 수 있는 길(등불 먹) 뒤에 번지는 빛 (기본 T_Map_SoftDot)
        public Texture2D GlowTexture;
        public float GlowScale = 2.6f;
        public float GlowOpacity = 0.35f;
    }

    /// <summary>그릴 선 하나. 좌표는 지도 좌표 (이 레이어 기준 로컬)</summary>
    public struct MapEdgeDraw
    {
        public Vector2 From;
        public Vector2 To;
        public MapEdgeState State;
    }

    public sealed class MapEdgeLayer : VisualElement
    {
        MapEdgeStyle _style = new MapEdgeStyle();
        readonly List<MapEdgeDraw> _edges = new List<MapEdgeDraw>();

        public MapEdgeLayer()
        {
            name = "EdgeLayerRoot";
            pickingMode = PickingMode.Ignore;   // 클릭은 방 아이콘이 받게
            generateVisualContent += OnGenerate;
        }

        public void SetEdgeStyle(MapEdgeStyle style)
        {
            _style = style ?? new MapEdgeStyle();
            if (_style.GlowTexture == null) _style.GlowTexture = DungeonStyle.Tex(MapTextures.SoftDot);
            MarkDirtyRepaint();
        }

        /// <summary>상태 순으로 정렬해서 보관 (지나온 길이 맨 위에 그려지게)</summary>
        public void SetEdges(List<MapEdgeDraw> edges)
        {
            _edges.Clear();
            if (edges != null)
            {
                // 안정 정렬 (같은 상태끼리는 넣은 순서)
                for (int state = 0; state <= (int)MapEdgeState.Taken; ++state)
                {
                    foreach (var e in edges)
                    {
                        if ((int)e.State == state) _edges.Add(e);
                    }
                }
            }
            MarkDirtyRepaint();
        }

        // 그릴 사각형 하나 (회전 포함)
        struct Quad
        {
            public Vector2 Center;
            public Vector2 Size;
            public Vector2 Dir;   // 가로축 방향 (단위)
            public Color Color;
        }

        void OnGenerate(MeshGenerationContext ctx)
        {
            if (_edges.Count == 0) return;
            var s = _style;
            float spacing = Mathf.Max(2f, s.DotSpacing);

            // 언리얼은 갈 수 있는 길의 점만 한 층 위(MaxLayer + 1)에 그림 -> 나머지(빛 포함)를 먼저 순서대로, 그 다음 그 점들
            var lower = new List<(Texture2D tex, Quad q)>();
            var upper = new List<(Texture2D tex, Quad q)>();

            foreach (var edge in _edges)
            {
                var delta = edge.To - edge.From;
                float length = delta.magnitude;
                float usable = length - s.EndPadding * 2f;
                if (usable <= 0f) continue;   // 아이콘끼리 너무 붙어 있음

                var dir = delta / length;

                var color = s.DefaultColor;
                float dotScale = 1f;
                switch (edge.State)
                {
                    case MapEdgeState.Available: color = s.AvailableColor; dotScale = s.EmphasisDotScale; break;
                    case MapEdgeState.Taken: color = s.TakenColor; dotScale = s.EmphasisDotScale; break;
                }
                var dotSize = s.DotSize * dotScale;

                // 점 개수를 정하고 간격을 선 길이에 맞게 살짝 늘림 -> 양 끝 여백이 항상 같음
                int count = Mathf.FloorToInt(usable / spacing) + 1;
                float step = count > 1 ? usable / (count - 1) : 0f;
                var start = edge.From + dir * (count > 1 ? s.EndPadding : length * 0.5f);

                // 갈 수 있는 길: 점 뒤에 등불 빛을 먼저 깔고 (점은 한 층 위)
                bool glow = edge.State == MapEdgeState.Available && s.GlowOpacity > 0f && s.GlowTexture != null;
                if (glow)
                {
                    var glowColor = color;
                    glowColor.a *= s.GlowOpacity;
                    for (int k = 0; k < count; ++k)
                    {
                        lower.Add((s.GlowTexture, new Quad { Center = start + dir * (step * k), Size = dotSize * s.GlowScale, Dir = Vector2.right, Color = glowColor }));
                    }
                }

                var target = glow ? upper : lower;
                for (int k = 0; k < count; ++k)
                {
                    target.Add((s.DotTexture, new Quad { Center = start + dir * (step * k), Size = dotSize, Dir = dir, Color = color }));
                }
            }

            Emit(ctx, lower);
            Emit(ctx, upper);
        }

        // 같은 텍스처가 이어지는 것끼리 한 번에 할당
        static void Emit(MeshGenerationContext ctx, List<(Texture2D tex, Quad q)> quads)
        {
            int i = 0;
            while (i < quads.Count)
            {
                var tex = quads[i].tex;
                int j = i;
                while (j < quads.Count && quads[j].tex == tex && j - i < 8000) ++j;

                if (tex == null) EmitRound(ctx, quads, i, j);
                else EmitTextured(ctx, tex, quads, i, j);
                i = j;
            }
        }

        static void EmitTextured(MeshGenerationContext ctx, Texture2D tex, List<(Texture2D tex, Quad q)> quads, int from, int to)
        {
            int n = to - from;
            var mesh = ctx.Allocate(n * 4, n * 6, tex);
            var uv = mesh.uvRegion;
            var verts = new Vertex[n * 4];
            var indices = new ushort[n * 6];
            for (int k = 0; k < n; ++k)
            {
                var q = quads[from + k].q;
                var axisX = q.Dir * (q.Size.x * 0.5f);
                var axisY = new Vector2(-q.Dir.y, q.Dir.x) * (q.Size.y * 0.5f);
                Color32 tint = q.Color;

                // 화면은 y 가 아래로. 위쪽 두 점 = 텍스처 v 1
                var p0 = q.Center - axisX - axisY;
                var p1 = q.Center + axisX - axisY;
                var p2 = q.Center + axisX + axisY;
                var p3 = q.Center - axisX + axisY;
                int v = k * 4;
                verts[v + 0] = MakeVertex(p0, tint, uv, 0f, 1f);
                verts[v + 1] = MakeVertex(p1, tint, uv, 1f, 1f);
                verts[v + 2] = MakeVertex(p2, tint, uv, 1f, 0f);
                verts[v + 3] = MakeVertex(p3, tint, uv, 0f, 0f);

                int t = k * 6;
                indices[t + 0] = (ushort)(v + 0);
                indices[t + 1] = (ushort)(v + 1);
                indices[t + 2] = (ushort)(v + 2);
                indices[t + 3] = (ushort)(v + 2);
                indices[t + 4] = (ushort)(v + 3);
                indices[t + 5] = (ushort)(v + 0);
            }
            mesh.SetAllVertices(verts);
            mesh.SetAllIndices(indices);
        }

        static Vertex MakeVertex(Vector2 p, Color32 tint, Rect uvRegion, float u, float v) => new Vertex
        {
            position = new Vector3(p.x, p.y, Vertex.nearZ),
            tint = tint,
            uv = new Vector2(uvRegion.x + u * uvRegion.width, uvRegion.y + v * uvRegion.height),
        };

        // 점 이미지가 없을 때 쓰는 둥근 점
        static void EmitRound(MeshGenerationContext ctx, List<(Texture2D tex, Quad q)> quads, int from, int to)
        {
            var p = ctx.painter2D;
            for (int k = from; k < to; ++k)
            {
                var q = quads[k].q;
                p.fillColor = q.Color;
                p.BeginPath();
                p.Arc(q.Center, Mathf.Min(q.Size.x, q.Size.y) * 0.5f, 0f, 360f);
                p.Fill();
            }
        }
    }
}
