// 화면 높이. 숫자가 클수록 위에 그려짐 (언리얼 Widgets/TerminusUILayers.h 그대로)
// 새 화면을 붙일 땐 여기서 고르고, 순서를 바꾸고 싶으면 여기만 고칠 것
namespace Terminus.UI
{
    public enum UILayer
    {
        Map = 0,          // 지도 (맨 아래)
        CombatHUD = 10,   // 전투 HUD
        Comms = 11,       // 핑 / 퀵챗 (전투 HUD 바로 위)
        Pick = 20,        // 시작 스킬 / 유물 고르기
        RoomPanel = 22,   // 보상 / 휴식 / 이벤트 / 상점
        RoomTopBar = 23,  // 휴식 / 이벤트 / 상점 위에 띄우는 지도 상단바
        Rescue = 24,      // 구출 투표 (방 패널 위)
        Overlay = 25,     // 관전 바 / 이공간 / 층 투표 (패널 위, 채팅 아래)
        Chat = 26,        // 채팅
        CommsMenu = 27,   // 핑 메뉴 (채팅 위)
        FloorTitle = 30,  // "n층" 타이틀
        MenuWindow = 40,  // 메인 메뉴의 거점 / 창고 / 이어하기 창
        Settlement = 45,  // 정산
        Popup = 50,       // 확인 팝업 (어떤 화면보다도 위)
    }
}
