// 방 패널 (언리얼 ATerminusPlayerController 의 Client_ShowRoomReward / Client_ShowRest / Client_ShowShop / Client_ShowEvent / Client_ShowRescue 와 그 Close)
// DungeonView 가 던전에 들어올 때 Attach() 를 한 번 부름. 서버 알림(NetBus)을 받아 띄움
//  - 보상 / 휴식 / 이벤트(퀘스트 방 포함) / 상점: UILayer.RoomPanel
//  - 구출 투표: UILayer.Rescue (방 패널 위)
// 내가 그 구역을 떠나면 (지도로 / 다른 구역) 그 구역의 패널은 닫음. Detach() 는 전부 닫고 알림을 무시
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public static class RoomPanels
    {
        // 패널 하나 + 띄울 때 있던 구역 (그 구역을 떠나면 닫음)
        sealed class Open
        {
            public VisualElement Widget;
            public int AreaIndex = -1;   // -1 = 아직 모름 (스냅샷이 알림보다 늦게 올 수 있어서 처음 본 구역을 기억)
            public bool TiedToArea = true;
        }

        static bool _hooked;
        static bool _attached;
        static Open _reward, _rest, _event, _shop, _rescue;

        public static void Attach()
        {
            _attached = true;
            if (!_hooked)
            {
                // 처리기는 한 번만 (씬을 다시 들어와도 쌓이지 않게). 붙어 있지 않으면 무시
                _hooked = true;
                NetBus.On<RoomRewardMsg>(m => { if (_attached) ShowReward(m); });
                NetBus.On<CloseRoomRewardMsg>(_ => Close(ref _reward));
                NetBus.On<ShowRestMsg>(m => { if (_attached) ShowRest(m); });
                NetBus.On<RestResultMsg>(m => (_rest?.Widget as RestWidget)?.ShowResult(m.Result));
                NetBus.On<CloseRestMsg>(_ => Close(ref _rest));
                NetBus.On<ShowShopMsg>(m => { if (_attached) ShowShop(m); });
                NetBus.On<CloseShopMsg>(_ => Close(ref _shop));
                NetBus.On<ShowEventMsg>(m => { if (_attached) ShowEvent(m); });
                NetBus.On<EventResultMsg>(m => (_event?.Widget as EventWidget)?.ShowResult(m.Result));
                NetBus.On<CloseEventMsg>(_ => Close(ref _event));
                NetBus.On<ShowRescueMsg>(m => { if (_attached) ShowRescue(m); });
                NetBus.On<CloseRescueMsg>(_ => Close(ref _rescue));
            }
            ClientState.PartyChanged -= OnPartyChanged;
            ClientState.PartyChanged += OnPartyChanged;
        }

        public static void Detach()
        {
            _attached = false;
            ClientState.PartyChanged -= OnPartyChanged;
            CloseAll();
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics()
        {
            _hooked = false;
            _attached = false;
            _reward = _rest = _event = _shop = _rescue = null;
        }

        static void CloseAll()
        {
            Close(ref _reward);
            Close(ref _rest);
            Close(ref _event);
            Close(ref _shop);
            Close(ref _rescue);
        }

        static void Close(ref Open open)
        {
            if (open == null) return;
            UiTooltip.Hide();
            open.Widget?.RemoveFromHierarchy();
            open = null;
        }

        static Open ShowOn(UILayer layer, VisualElement widget, bool tiedToArea)
        {
            var open = new Open { Widget = widget, TiedToArea = tiedToArea };
            var me = ClientState.Me;
            if (me != null) open.AreaIndex = me.AreaIndex;
            UIRoot.Instance?.Show(layer, widget);
            return open;
        }

        // 내가 구역을 떠나면 그 구역의 패널을 닫음
        static void OnPartyChanged()
        {
            var me = ClientState.Me;
            if (me == null) return;
            Check(ref _reward, me.AreaIndex);
            Check(ref _rest, me.AreaIndex);
            Check(ref _event, me.AreaIndex);
            Check(ref _shop, me.AreaIndex);
            Check(ref _rescue, me.AreaIndex);
        }

        static void Check(ref Open open, int myArea)
        {
            if (open == null) return;
            if (open.Widget == null || open.Widget.parent == null)
            {
                // 스스로 닫힘 (보상 '다음으로')
                open = null;
                return;
            }
            if (!open.TiedToArea) return;
            if (open.AreaIndex < 0)
            {
                if (myArea >= 0) open.AreaIndex = myArea;
                return;
            }
            if (myArea != open.AreaIndex) Close(ref open);
        }

        // ------------------------------------------------------------ 띄우기

        static void ShowReward(RoomRewardMsg m)
        {
            Close(ref _reward);
            var widget = new MonsterRewardWidget();
            widget.Setup(m.Offer);
            _reward = ShowOn(UILayer.RoomPanel, widget, true);
        }

        static void ShowRest(ShowRestMsg m)
        {
            Close(ref _rest);
            var widget = new RestWidget();
            widget.Setup(m.Occupants, m.HealRatio, m.ExploreMin, m.ExploreMax, m.RelicChance);
            _rest = ShowOn(UILayer.RoomPanel, widget, true);
        }

        static void ShowShop(ShowShopMsg m)
        {
            // 사고 / 팔 때마다 진열이 다시 옴 -> 열려 있으면 내용만 바꿈
            if (_shop?.Widget is ShopWidget open && open.parent != null)
            {
                open.SetShopState(m.State);
                return;
            }
            Close(ref _shop);
            var widget = new ShopWidget();
            widget.SetShopState(m.State);
            _shop = ShowOn(UILayer.RoomPanel, widget, true);
        }

        static void ShowEvent(ShowEventMsg m)
        {
            Close(ref _event);
            var widget = new EventWidget();
            widget.Setup(m.Title, m.Situation, m.Options);
            _event = ShowOn(UILayer.RoomPanel, widget, true);
        }

        static void ShowRescue(ShowRescueMsg m)
        {
            Close(ref _rescue);
            var widget = new RescueWidget();
            widget.Setup(m.bChooser, m.WipedNames, m.bCanIntervene, m.HealthCost, m.Seconds);
            // 구출은 다른 방(또는 지도)에 있는 사람에게도 뜸 -> 구역과 묶지 않음. 결과가 나오면 서버가 닫음
            _rescue = ShowOn(UILayer.Rescue, widget, false);
        }
    }
}
