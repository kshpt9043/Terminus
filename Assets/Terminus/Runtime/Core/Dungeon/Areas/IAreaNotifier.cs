// 구역 / 이공간이 클라에 보내는 것 (언리얼 ATerminusPlayerController 의 Client_* RPC 와 BroadcastChat)
// Core 는 누구에게 무엇을 보낼지만 정하고, Game 쪽(NetworkBehaviour)이 이 인터페이스를 구현해 RPC 로 보냄
// 받는 쪽 화면(위젯)은 UI. 여기 값은 사본이라 그대로 직렬화해도 됨
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Dungeon
{
    public interface IAreaNotifier
    {
        // 전원에게 시스템 채팅 (언리얼 ATerminusPlayerController::BroadcastChat, Kind = System)
        void BroadcastSystemChat(string text);

        // ---- 휴식터 (Client_ShowRest / Client_RestResult / Client_CloseRest)
        void ShowRest(DungeonPlayer to, IReadOnlyList<DungeonPlayer> occupants, float healRatio, int exploreCurrencyMin, int exploreCurrencyMax, float exploreRelicChance);
        void RestResult(DungeonPlayer to, string result);
        void CloseRest(DungeonPlayer to);

        // ---- 상점 (Client_ShowShop / Client_CloseShop). state 는 사본
        void ShowShop(DungeonPlayer to, ShopState state);
        void CloseShop(DungeonPlayer to);

        // ---- 이벤트 / 퀘스트 방 (Client_ShowEvent / Client_EventResult / Client_CloseEvent). 퀘스트 방도 이벤트 화면을 같이 씀
        void ShowEvent(DungeonPlayer to, string title, string situation, IReadOnlyList<EventOption> options);
        void EventResult(DungeonPlayer to, string result);
        void CloseEvent(DungeonPlayer to);

        // ---- 구출 / 난입 투표 (Client_ShowRescue / Client_CloseRescue). bChooser = 투표하는 쪽(살아남은 사람)
        void ShowRescue(DungeonPlayer to, bool bChooser, IReadOnlyList<string> wipedNames, bool bCanIntervene, float healthCost, float seconds);
        void CloseRescue(DungeonPlayer to);

        // ---- 이공간 (Client_EnterRift / Client_LeaveRift). waitingFor = 아직 안 돌아온 사람 이름
        void EnterRift(DungeonPlayer to, IReadOnlyList<string> waitingFor, bool bFromSave);
        void LeaveRift(DungeonPlayer to);
    }
}
