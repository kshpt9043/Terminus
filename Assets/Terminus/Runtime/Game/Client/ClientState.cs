// [클라] 서버에서 받은 최신 상태 (언리얼 클라에 복제된 PlayerState / MapManager / 구역 / 전투 컴포넌트 자리)
// 화면은 여기만 보고 그린다. 바뀌면 Changed 이벤트 (언리얼 OnRep)
using System;
using System.Collections.Generic;
using Terminus.Game.Net;
using Terminus.Map;
using UnityEngine;

namespace Terminus.Game.Client
{
    public static class ClientState
    {
        public static PartySnapshot Party { get; private set; } = new PartySnapshot();
        public static MapSnapshot Map { get; private set; } = new MapSnapshot();
        public static AreasSnapshot Areas { get; private set; } = new AreasSnapshot();

        public static readonly List<ChatMsg> ChatLog = new List<ChatMsg>();

        public static event Action PartyChanged;
        public static event Action MapChanged;
        public static event Action AreasChanged;
        public static event Action<ChatMsg> ChatReceived;

        static bool _hooked;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            _hooked = false;
            Hook();
        }

        public static void Hook()
        {
            if (_hooked) return;
            _hooked = true;
            NetBus.On<PartySnapshot>(s => { Party = s; PartyChanged?.Invoke(); });
            NetBus.On<MapSnapshot>(s => { Map = s; MapChanged?.Invoke(); });
            NetBus.On<AreasSnapshot>(s => { Areas = s; AreasChanged?.Invoke(); });
            NetBus.On<ChatMsg>(m =>
            {
                ChatLog.Add(m);
                if (ChatLog.Count > 100) ChatLog.RemoveAt(0);
                ChatReceived?.Invoke(m);
            });
        }

        public static void Reset()
        {
            Party = new PartySnapshot();
            Map = new MapSnapshot();
            Areas = new AreasSnapshot();
            ChatLog.Clear();
        }

        // ------------------------------------------------------------ 자주 쓰는 조회

        public static ulong MyId => NetBus.LocalClientId;

        public static PlayerInfo Me => Party.Find(MyId);

        public static bool IAmHost => Me != null && Me.bHost;

        /// <summary>내가 들어가 있는 구역. 지도에 있으면 null</summary>
        public static AreaInfo MyArea
        {
            get
            {
                var me = Me;
                return me == null || me.AreaIndex < 0 ? null : Areas.Find(me.AreaIndex);
            }
        }

        public static RoomNode FindRoom(int roomId) => Map.Rooms.Find(r => r.RoomId == roomId);

        public static string NameOf(ulong id)
        {
            var p = Party.Find(id);
            return p != null ? p.Name : "?";
        }
    }
}
