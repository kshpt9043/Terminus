// 층 도착 화면 (언리얼 UFloorTitleWidget / WBP_FloorTitle). 다음 층으로 넘어갈 때 잠깐
// "2층 / 표층 · 슬라임 왕국 / 체력이 모두 회복되었습니다" 를 보여주고 스스로 사라짐 (UILayer.FloorTitle)
// 띄우는 건 FloorTitleMsg (언리얼 PC::Client_ShowFloorTitle)
//
// WBP 구조: FloorTitleDim (검정 75%, 화면 전체) + 가운데 VerticalBox
//   TitleText 40 #ECE3CF / SubtitleText 22 #C9A45C (위 8) / NoteText 16 #6FB380 (위 18)
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class FloorTitleWidget : VisualElement
    {
        // 보여주는 시간 (초)
        public float DisplaySeconds = 2.5f;

        readonly Label _titleText;
        readonly Label _subtitleText;
        readonly Label _noteText;

        public FloorTitleWidget()
        {
            name = "FloorTitle";
            DungeonStyle.Fill(this);
            // 보여주는 동안 아래 지도를 못 누르게
            pickingMode = PickingMode.Position;
            RegisterCallback<PointerDownEvent>(e => e.StopPropagation());

            var dim = new VisualElement { name = "FloorTitleDim", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(dim);
            dim.style.backgroundColor = new Color(0f, 0f, 0f, 0.75f);
            Add(dim);

            var column = new VisualElement { name = "FloorTitleColumn", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(column);
            column.style.alignItems = Align.Center;
            column.style.justifyContent = Justify.Center;
            Add(column);

            _titleText = UiStyle.Text("", 40, UiColor.FromLinear(0.838799f, 0.768151f, 0.623960f), TextAnchor.MiddleCenter);
            column.Add(_titleText);

            _subtitleText = UiStyle.Text("", 22, UiColor.FromLinear(0.584078f, 0.371238f, 0.107023f), TextAnchor.MiddleCenter);
            _subtitleText.style.marginTop = 8f;
            column.Add(_subtitleText);

            _noteText = UiStyle.Text("", 16, UiColor.FromLinear(0.158961f, 0.450786f, 0.215861f), TextAnchor.MiddleCenter);
            _noteText.style.marginTop = 18f;
            column.Add(_noteText);

            RegisterCallback<AttachToPanelEvent>(_ =>
                schedule.Execute(() => RemoveFromHierarchy()).StartingIn((long)(DisplaySeconds * 1000f)));
        }

        public void Setup(string title, string subtitle, string note)
        {
            _titleText.text = title ?? "";
            _subtitleText.text = subtitle ?? "";
            _noteText.text = note ?? "";
        }

        /// <summary>언리얼 PC::Client_ShowFloorTitle: 새로 만들어 띄움 (노트는 늘 "체력이 모두 회복되었습니다")</summary>
        public static FloorTitleWidget Show(string title, string subtitle)
        {
            var w = new FloorTitleWidget();
            w.Setup(title, subtitle, "체력이 모두 회복되었습니다");
            UIRoot.Instance?.Show(UILayer.FloorTitle, w);
            return w;
        }
    }
}
