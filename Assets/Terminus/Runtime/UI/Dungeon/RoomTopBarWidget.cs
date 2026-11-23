// 휴식 / 이벤트 / 상점 화면 위에 지도 상단바를 띄우는 틀 (언리얼 URoomTopBarWidget / WBP_RoomTopBar, 사용자 결정 2026-10-12)
// 지도 화면은 방에 들어가면 통째로 숨겨지므로, 같은 상단바(MapTopBarWidget)를 하나 더 만들어 화면 맨 위에 붙임 (UILayer.RoomTopBar)
// 상단바 아래는 비어 있고 클릭이 통과함 (방 화면을 그대로 누를 수 있게)
// 보이고 숨기는 건 DungeonView 가 (언리얼 PC::UpdateRoomTopBar: 휴식 / 이벤트 / 상점 화면이 열려 있을 때)
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class RoomTopBarWidget : VisualElement
    {
        // 지도 화면 WBP 의 상단바 높이 (WBP_MapScreen MapTopBar Bottom = 64)
        public const float BarHeight = 64f;

        public MapTopBarWidget TopBar { get; }

        public RoomTopBarWidget()
        {
            name = "RoomTopBarRoot";
            pickingMode = PickingMode.Ignore;
            DungeonStyle.Fill(this);

            TopBar = new MapTopBarWidget();
            TopBar.style.height = BarHeight;
            TopBar.style.flexShrink = 0;
            Add(TopBar);
        }
    }
}
