// [에디터 전용, 개발용] Multiplayer Play Mode 복제 플레이어를 "자동 손님"으로
// 메인 에디터(방장)만 조작해도 멀티 흐름을 끝까지 볼 수 있게:
//  - 메인 메뉴에서 127.0.0.1 로 자동 참가 (방장이 방을 열 때까지 2초마다 재시도)
//  - 주점에서 자동 준비, 던전 시작 고르기는 건너뜀
//  - 지도에서는 방장이 고른 방을 따라 고름 (정원이 안 되면 갈 수 있는 다른 방)
//  - 전투에서는 아무것도 안 하고 턴 종료, 보상은 안 받고 다음으로, 층 투표는 방장과 같은 표
// 끄려면: 복제 플레이어 실행 인자/태그 대신 아래 Enabled 를 false 로
#if UNITY_EDITOR
using System.Linq;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Map;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Terminus.Game.Dev
{
    public sealed class DevAutoGuest : MonoBehaviour
    {
        public static bool Enabled = true;

        static bool IsMppmClone => Application.dataPath.Replace('\\', '/').Contains("/Library/VP/");

        float _next;
        int _lastTurnCycle = -1;
        int _lastRewardRoom = -1;
        int _lastVoteId = -1;
        static bool _pickOther;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Init()
        {
            if (!Enabled || !IsMppmClone) return;
            var go = new GameObject("DevAutoGuest");
            DontDestroyOnLoad(go);
            go.AddComponent<DevAutoGuest>();
            NetworkRoot.LocalPlayerName = "자동 손님";
            Game.Save.LocalProfile.SetProfileKey("mppm_clone");
            NetBus.On<RoomRewardMsg>(_ => NetBus.Request(new FinishRoomRewardReq()));
            NetBus.On<ShowRestMsg>(_ => NetBus.Request(new RestExploreReq()));
            NetBus.On<ShowShopMsg>(_ => NetBus.Request(new ShopLeaveReq()));
            NetBus.On<ShowEventMsg>(m => NetBus.Request(new ChooseEventReq { Index = m.Options.Count - 1 }));
            NetBus.On<ShowRescueMsg>(m => { if (m.bChooser) NetBus.Request(new ChooseRescueReq { bIntervene = false }); });
            // 방장 방이 정원 초과면 갈 수 있는 다른 방으로
            NetBus.On<RoomSelectFailedMsg>(m => { _pickOther = true; Debug.Log("[DevAutoGuest] 방 선택 거절: " + m.Message); });
            Debug.Log("[DevAutoGuest] 복제 플레이어: 자동 손님으로 동작");
        }

        void Update()
        {
            if (Time.unscaledTime < _next) return;
            _next = Time.unscaledTime + 1f;

            var scene = SceneManager.GetActiveScene().name;
            var nm = NetworkManager.Singleton;
            bool connected = nm != null && nm.IsConnectedClient;

            if (scene == SceneNames.MainMenu)
            {
                if (nm == null || (!nm.IsClient && !nm.IsServer)) NetworkRoot.Join("127.0.0.1");
                return;
            }
            if (!connected || TerminusPlayer.Local == null) return;

            var me = ClientState.Me;
            if (me == null) return;

            if (scene == SceneNames.Tavern)
            {
                if (!me.bReady) NetBus.Request(new SetReadyReq { bReady = true });
                return;
            }

            if (scene != SceneNames.Dungeon || !me.bHasBattler) return;

            if (!me.Run.bStartSkillChosen) { NetBus.Request(new ChooseStartSkillReq()); return; }
            if (!me.Run.bStartRelicsChosen) { NetBus.Request(new ChooseStartRelicsReq()); return; }

            // 층 투표: 방장과 같은 표 (방장이 안 골랐으면 다음 층)
            var vote = ClientState.Map.Vote;
            if (vote != null && vote.Phase == FloorVotePhase.Voting && vote.VoteId != _lastVoteId)
            {
                _lastVoteId = vote.VoteId;
                var choice = vote.Options.Contains(FloorChoice.NextFloor) ? FloorChoice.NextFloor : FloorChoice.Escape;
                NetBus.Request(new CastFloorVoteReq { Choice = choice });
                return;
            }

            var area = ClientState.MyArea;
            if (area != null && area.bInUse)
            {
                if (area.Phase == "PlayerTurn" && !area.EndedTurn.Contains(me.Id) && me.Combat.Health > 0 && area.Cycle != _lastTurnCycle)
                {
                    _lastTurnCycle = area.Cycle;
                    NetBus.Request(new EndTurnReq());
                }
                return;
            }
            _lastTurnCycle = -1;

            // 지도: 방장이 고른 방을 따라감
            var host = ClientState.Party.Players.FirstOrDefault(p => p.bHost);
            if (host == null || host.Run.SelectedRoomId < 0 || me.Run.SelectedRoomId >= 0) return;
            if (!_pickOther)
            {
                NetBus.Request(new SelectRoomReq { RoomId = host.Run.SelectedRoomId });
                return;
            }
            _pickOther = false;
            var rooms = ClientState.Map.Rooms;
            var cur = rooms.FirstOrDefault(r => r.RoomId == me.Run.CurrentRoomId);
            var options = cur != null ? rooms.Where(r => cur.ConnectedRoomIds.Contains(r.RoomId)) : rooms.Where(r => r.Row == rooms.Min(x => x.Row));
            var other = options.FirstOrDefault(r => r.RoomId != host.Run.SelectedRoomId);
            if (other != null) NetBus.Request(new SelectRoomReq { RoomId = other.RoomId });
        }
    }
}
#endif
