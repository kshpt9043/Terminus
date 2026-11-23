// 구역 테스트 공용: 가짜 전투 / 알림 기록 / 플레이어와 관리자 만들기
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    public sealed class FakeAreaCombat : IAreaCombat
    {
        public readonly List<string> Calls = new List<string>();
        public RoomNode LastRoom;
        public List<DungeonPlayer> LastPlayers = new List<DungeonPlayer>();
        public DungeonTheme LastTheme;
        public float NextHealthScale = 1f;
        public float StartedHealthScale = 1f;
        public List<string> NextForced = new List<string>();
        public List<string> StartedForced = new List<string>();
        public DungeonPlayer Betrayer;
        public List<DungeonPlayer> Joiners = new List<DungeonPlayer>();
        public DungeonPlayer BetrayalLeaver;
        public float VictimHealthCut;
        public int StatPerOpponent;

        // 휴식터 탐색이 받을 유물
        public List<string> RewardRelics = new List<string>();

        public bool IsBetrayal { get; set; }
        public bool IsVictory { get; set; }
        public bool IsTurnInProgress { get; set; }

        public event Action AreaCleared;
        public event Action AreaWiped;
        public event Action<DungeonPlayer, bool> BetrayalFinished;

        public void SetNextSpawnHealthScale(float scale)
        {
            Calls.Add("SetNextSpawnHealthScale");
            NextHealthScale = Math.Max(0.01f, scale);
        }

        public void SetNextForcedMonsters(IReadOnlyList<string> rows)
        {
            Calls.Add("SetNextForcedMonsters");
            NextForced = new List<string>(rows);
        }

        public void StartCombat(RoomNode room, IReadOnlyList<DungeonPlayer> players, DungeonTheme theme)
        {
            Calls.Add("StartCombat");
            LastRoom = room.Clone();
            LastPlayers = new List<DungeonPlayer>(players);
            LastTheme = theme;
            StartedHealthScale = NextHealthScale;
            StartedForced = NextForced;
            NextHealthScale = 1f;
            NextForced = new List<string>();
            IsTurnInProgress = true;
            IsVictory = false;
        }

        public void EndCombat()
        {
            Calls.Add("EndCombat");
            IsTurnInProgress = false;
            IsVictory = false;
            IsBetrayal = false;
        }

        public void StartBetrayal(IReadOnlyList<DungeonPlayer> players, DungeonPlayer betrayer, float victimHealthCut, int statPerOpponent)
        {
            Calls.Add("StartBetrayal");
            LastPlayers = new List<DungeonPlayer>(players);
            Betrayer = betrayer;
            VictimHealthCut = victimHealthCut;
            StatPerOpponent = statPerOpponent;
            IsBetrayal = true;
            IsTurnInProgress = true;
        }

        public void HandleBetrayalLeaver(DungeonPlayer leaver)
        {
            Calls.Add("HandleBetrayalLeaver");
            BetrayalLeaver = leaver;
        }

        public void ResumeWithPlayers(IReadOnlyList<DungeonPlayer> joiners)
        {
            Calls.Add("ResumeWithPlayers");
            Joiners = new List<DungeonPlayer>(joiners);
            IsTurnInProgress = true;
        }

        public List<string> PickRewardRelics(DungeonPlayer player, int count)
        {
            Calls.Add("PickRewardRelics");
            var r = new List<string>(RewardRelics);
            if (r.Count > count) r.RemoveRange(count, r.Count - count);
            return r;
        }

        // ---- 테스트가 전투 결과를 흉내냄
        public void Win()
        {
            IsTurnInProgress = false;
            IsVictory = true;
        }

        public void Clear()
        {
            Win();
            AreaCleared?.Invoke();
        }

        public void Wipe()
        {
            IsTurnInProgress = false;
            AreaWiped?.Invoke();
        }

        public void FinishBetrayal(DungeonPlayer betrayer, bool won) => BetrayalFinished?.Invoke(betrayer, won);

        public int Count(string call) => Calls.FindAll(c => c == call).Count;
    }

    public sealed class RecordingNotifier : IAreaNotifier
    {
        public readonly List<string> Chat = new List<string>();
        public readonly List<string> Log = new List<string>();
        public readonly Dictionary<DungeonPlayer, ShopState> LastShop = new Dictionary<DungeonPlayer, ShopState>();
        public readonly Dictionary<DungeonPlayer, List<EventOption>> LastEvent = new Dictionary<DungeonPlayer, List<EventOption>>();
        public readonly Dictionary<DungeonPlayer, string> LastEventTitle = new Dictionary<DungeonPlayer, string>();
        public readonly Dictionary<DungeonPlayer, string> LastResult = new Dictionary<DungeonPlayer, string>();
        public readonly Dictionary<DungeonPlayer, bool> RescueChooser = new Dictionary<DungeonPlayer, bool>();
        public readonly Dictionary<DungeonPlayer, List<string>> RiftWaiting = new Dictionary<DungeonPlayer, List<string>>();
        public bool LastRescueCanIntervene;

        void Add(DungeonPlayer to, string what) => Log.Add($"{what}:{to?.Name}");

        public int Count(string what, DungeonPlayer to = null) => Log.FindAll(l => to == null ? l.StartsWith(what + ":") : l == $"{what}:{to.Name}").Count;

        public void BroadcastSystemChat(string text) => Chat.Add(text);

        public void ShowRest(DungeonPlayer to, IReadOnlyList<DungeonPlayer> occupants, float healRatio, int exploreCurrencyMin, int exploreCurrencyMax, float exploreRelicChance) => Add(to, "ShowRest");
        public void RestResult(DungeonPlayer to, string result) { Add(to, "RestResult"); LastResult[to] = result; }
        public void CloseRest(DungeonPlayer to) => Add(to, "CloseRest");

        public void ShowShop(DungeonPlayer to, ShopState state) { Add(to, "ShowShop"); LastShop[to] = state; }
        public void CloseShop(DungeonPlayer to) => Add(to, "CloseShop");

        public void ShowEvent(DungeonPlayer to, string title, string situation, IReadOnlyList<EventOption> options)
        {
            Add(to, "ShowEvent");
            LastEvent[to] = new List<EventOption>(options);
            LastEventTitle[to] = title;
        }
        public void EventResult(DungeonPlayer to, string result) { Add(to, "EventResult"); LastResult[to] = result; }
        public void CloseEvent(DungeonPlayer to) => Add(to, "CloseEvent");

        public void ShowRescue(DungeonPlayer to, bool bChooser, IReadOnlyList<string> wipedNames, bool bCanIntervene, float healthCost, float seconds)
        {
            Add(to, "ShowRescue");
            RescueChooser[to] = bChooser;
            LastRescueCanIntervene = bCanIntervene;
        }
        public void CloseRescue(DungeonPlayer to) => Add(to, "CloseRescue");

        public void EnterRift(DungeonPlayer to, IReadOnlyList<string> waitingFor, bool bFromSave) { Add(to, "EnterRift"); RiftWaiting[to] = new List<string>(waitingFor); }
        public void LeaveRift(DungeonPlayer to) => Add(to, "LeaveRift");
    }

    public static class AreaTestKit
    {
        public static readonly DungeonTheme Slime = new DungeonTheme("DA_Slime", "슬라임 왕국");

        public static DungeonPlayer MakePlayer(ulong id, string name, CharacterClass c = CharacterClass.Fighter, int seed = 1)
        {
            var run = new RunPlayer(new Random(seed), name);
            run.SetCharacterClass(c);
            run.BeginRun(UpgradeSettings.Default);
            var battler = new CombatStats { DebugName = name };
            battler.InitFrom(run.RunState.Stats);
            return new DungeonPlayer(id, run, battler);
        }

        public sealed class Rig
        {
            public DungeonAreaManager Manager;
            public DungeonTimers Timers;
            public RecordingNotifier Notifier;
            public List<DungeonArea> Areas = new List<DungeonArea>();
            public List<FakeAreaCombat> Combats = new List<FakeAreaCombat>();
            public int Floor = 1;
            public bool BossCleared;
            public int RoomsFinishedCount;
            public string WipeReason;

            public FakeAreaCombat CombatOf(DungeonArea area) => Combats[Areas.IndexOf(area)];
        }

        /// <summary>구역 count 개 (번호 0..count-1) 를 등록한 관리자</summary>
        public static Rig MakeRig(int areaCount = 4, int seed = 7, DungeonAreaSettings settings = null)
        {
            var rig = new Rig
            {
                Timers = new DungeonTimers(),
                Notifier = new RecordingNotifier(),
            };
            rig.Manager = new DungeonAreaManager(settings ?? new DungeonAreaSettings(), new Random(seed), rig.Timers, rig.Notifier);
            rig.Manager.CurrentFloorProvider = () => rig.Floor;
            rig.Manager.RoomsFinished += boss => { rig.BossCleared = boss; ++rig.RoomsFinishedCount; };
            rig.Manager.PartyWiped += reason => rig.WipeReason = reason;

            var data = new DungeonThemeData(Slime);
            data.AreaSets.Add(new DungeonAreaSet("Set_Slime_A"));
            data.RoomTypeAreaSets[RoomType.STORE] = new DungeonAreaSetList { Sets = { new DungeonAreaSet("Set_Slime_Shop") } };
            rig.Manager.RegisterTheme(data);

            for (int i = 0; i < areaCount; ++i)
            {
                var combat = new FakeAreaCombat();
                var area = new DungeonArea(i, combat);
                rig.Combats.Add(combat);
                rig.Areas.Add(area);
                rig.Manager.RegisterArea(area);
            }
            return rig;
        }

        public static RoomNode Room(int id, RoomType type, int row = 1, string special = "")
        {
            return new RoomNode { RoomId = id, Row = row, Type = type, MaxPlayers = 4, SpecialRoom = special };
        }

        /// <summary>각자 방을 고르고 입장</summary>
        public static bool Enter(Rig rig, IReadOnlyList<RoomNode> rooms, params (DungeonPlayer player, int roomId)[] picks)
        {
            var players = new List<DungeonPlayer>();
            foreach (var pick in picks)
            {
                pick.player.Run.SetSelectedRoomId(pick.roomId);
                players.Add(pick.player);
            }
            return rig.Manager.StartSelectedRooms(players, rooms, Slime);
        }

        public static string FirstSkill(Func<SkillRow, bool> pred)
        {
            foreach (var s in GameData.Skills.Rows)
            {
                if (pred(s)) return s.RowName;
            }
            return "";
        }
    }
}
