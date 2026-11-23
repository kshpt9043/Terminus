// [서버 전용] 던전 씬의 게임 진행. 언리얼 ADungeonGameMode + AMapManager + UDungeonAreaSubsystem + 구역 액터들을 묶은 자리
// Core(FloorProgression / DungeonAreaManager / DungeonCombat)를 돌리고, 결과를 스냅샷 / 알림으로 뿌린다
// 각 모듈 보고의 "언리얼 글루" 목록을 여기서 연결 (PORTING.md 다음 단계)
using System.Collections.Generic;
using System.Linq;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Game.Net;
using Terminus.Map;
using Terminus.Run;
using Unity.Netcode;
using UnityEngine;

namespace Terminus.Game
{
    public sealed partial class DungeonServer : MonoBehaviour, IAreaNotifier
    {
        public static DungeonServer Instance { get; private set; }

        // 구역 수 = 한 층에서 동시에 진행할 수 있는 방 수 (최대 인원만큼). 언리얼 레벨에 놓인 BP_DungeonArea 4개
        public const int AreaCount = 4;
        public const int MonsterSlots = 3;

        // 계층별 테마 (언리얼 AMapManager TierThemes. 몬스터 Theme_KR 과 같은 이름)
        static readonly string[][] TierThemeNames =
        {
            new[] { "슬라임 왕국", "용암 지대" },
            new[] { "리자드의 늪", "침묵의 숲" },
            new[] { "정령계" },
        };

        ServerSession _session;
        System.Random _rng;
        FloorProgression _floor;
        DungeonTimers _timers;
        DungeonAreaManager _areas;
        readonly List<DungeonCombat> _combats = new List<DungeonCombat>();
        readonly List<DungeonCombatHost> _combatHosts = new List<DungeonCombatHost>();
        readonly Dictionary<ulong, DungeonPlayer> _players = new Dictionary<ulong, DungeonPlayer>();

        string _lastMapJson = "";
        string _lastAreasJson = "";
        float _nextSync;
        float _nextForceSync;

        double Now => NetworkManager.Singleton != null ? NetworkManager.Singleton.ServerTime.Time : Time.timeAsDouble;

        public static void StartIfServer()
        {
            if (Instance != null || !NetBus.IsServer || ServerSession.Instance == null) return;
            var go = new GameObject("DungeonServer");
            go.AddComponent<DungeonServer>();
        }

        void Awake()
        {
            Instance = this;
            _session = ServerSession.Instance;
            // 주점을 안 거치고 던전을 바로 켬 -> 여기서 새 런 (언리얼 PIE 던전 직행)
            if (_session.Run.PartySize <= 0) _session.Run.BeginNewRun(_session.ConnectedMembers.Count());
            _rng = new System.Random(_session.Run.RunSeed != 0 ? _session.Run.RunSeed : Mathf.Abs(System.Environment.TickCount));

            DungeonCombat.LogInfo = s => Debug.Log(s);
            DungeonCombat.LogWarning = s => Debug.LogWarning(s);
            CombatStats.LogWarning = s => Debug.LogWarning(s);

            BuildPlayers();
            BuildMap();
            BuildAreas();
            RegisterRequests();

            _session.FillExtraInfo = FillPlayerInfo;
            _session.MemberLeft += OnMemberLeft;
            BuildRift();

            StartFirstFloor();

            // 손님들이 던전 씬을 다 불러온 뒤 상태를 한 번 통째로 (늦게 도착한 사람은 변화 비교에서 빠지므로)
            var nm = NetworkManager.Singleton;
            if (nm != null && nm.SceneManager != null) nm.SceneManager.OnLoadEventCompleted += OnLoadEventCompleted;
        }

        void OnLoadEventCompleted(string sceneName, UnityEngine.SceneManagement.LoadSceneMode mode, List<ulong> completed, List<ulong> timedOut)
        {
            if (sceneName != SceneNames.Dungeon) return;
            SyncState(true);
            foreach (var id in completed)
            {
                NetBus.Notify(id, new FloorTitleMsg { Title = _floor.CurrentFloor + "층", Subtitle = FloorRules.GetTierName(_floor.CurrentFloor) + " · " + ThemeName() });
            }
        }

        void OnDestroy()
        {
            if (Instance == this) Instance = null;
            var nm = NetworkManager.Singleton;
            if (nm != null && nm.SceneManager != null) nm.SceneManager.OnLoadEventCompleted -= OnLoadEventCompleted;
            if (_session != null)
            {
                _session.FillExtraInfo = null;
                _session.MemberLeft -= OnMemberLeft;
                _session.MemberJoinedMidRun -= OnMemberJoinedMidRun;
            }
            NetworkRoot.CanRejoinMidRun = null;
        }

        // ------------------------------------------------------------ 준비

        // 언리얼: 던전 PostLogin 에서 배틀러 스폰 + RunState 스텟으로 초기화, 세이브 체력 적용
        void BuildPlayers()
        {
            foreach (var m in _session.ConnectedMembers)
            {
                // 주점을 안 거치고 던전 씬을 바로 켠 경우 (언리얼 PIE 던전 직행) -> 기본 스텟으로 런 시작
                if (!m.Run.RunState.bStatsInitialized) m.Run.BeginRun(GameData.Upgrades);

                var battler = new CombatStats { DebugName = m.Run.PlayerName };
                battler.InitFrom(m.Run.RunState.Stats);
                int saved = m.Run.ConsumeSavedHealth();
                if (saved > 0) battler.Revive(Mathf.Clamp01((float)saved / Mathf.Max(1, battler.Stats.MaxHealth)));
                _players[m.ClientId] = new DungeonPlayer(m.ClientId, m.Run, battler);
            }
        }

        void BuildMap()
        {
            var settings = new MapSettings();
            settings.TierThemes.Clear();
            foreach (var names in TierThemeNames)
            {
                var tier = new TierThemes();
                foreach (var n in names) tier.Themes.Add(new DungeonTheme(n, n));
                settings.TierThemes.Add(tier);
            }

            _floor = new FloorProgression(settings, _rng);
            _floor.RequestPlayerCountRefresh = () => _floor.RefreshPlayerCount(_session.Run.PartySize, _players.Values.Count(p => p.bConnected));
            _floor.MapGenerated += rooms =>
            {
                _session.Run.SetRooms(rooms);
                _lastMapJson = "";
            };
            _floor.FloorAdvanced += OnFloorAdvanced;
            _floor.VoteChanged += _ => _lastMapJson = "";
            _floor.SystemNotice += text => _session.SystemChat(text);
            _floor.BetrayalRequested += OnBetrayalRequested;
            _floor.RunEnded += OnRunEnded;
        }

        void BuildAreas()
        {
            _timers = new DungeonTimers();
            _areas = new DungeonAreaManager(new DungeonAreaSettings(), _rng, _timers, this)
            {
                Session = _session.Run,
                CurrentFloorProvider = () => _floor.CurrentFloor,
                LogSink = s => Debug.Log(s),
            };

            // 테마마다 구역 무대 (언리얼 UDungeonThemeData 의 AreaSets. 지금은 이름만, 무대 그림은 게임 쪽 프리팹 키)
            foreach (var names in TierThemeNames)
            {
                foreach (var n in names)
                {
                    var data = new DungeonThemeData(new DungeonTheme(n, n));
                    data.AreaSets.Add(new DungeonAreaSet("Set_" + n));
                    _areas.RegisterTheme(data);
                }
            }

            for (int i = 0; i < AreaCount; ++i)
            {
                var host = new DungeonCombatHost { CurrentFloor = 1, NumMonsterSlots = MonsterSlots, RunSession = _session.Run };
                var combat = new DungeonCombat(new DungeonCombatSettings(), host, new System.Random(_rng.Next()));
                int areaIndex = i;
                combat.RoomRewardOffered += (p, offer) => NetBus.Notify(p.Id, new RoomRewardMsg { Offer = offer });
                combat.RoomRewardClosed += p => NetBus.Notify(p.Id, new CloseRoomRewardMsg());
                combat.MonsterActing += (m, kind) => NetBus.Broadcast(new MonsterActingMsg { AreaIndex = areaIndex, MonsterId = m.Id, Kind = kind.ToString() });
                _combats.Add(combat);
                _combatHosts.Add(host);
                _areas.RegisterArea(new DungeonArea(i, combat, NetworkRoot.MaxPartySize, MonsterSlots));
            }

            _areas.RoomsFinished += OnRoomsFinished;
            _areas.PartyWiped += reason => _floor.EndRunByDeath(reason);
            _areas.BetrayalFinished += (betrayer, won) =>
                _floor.HandleBetrayalResult(betrayer?.Id, won, _players.Values.Where(p => p.bConnected).Select(ToParticipant).ToList());
            _areas.SpecialRoomInjectionRequested += _ => InjectSpecialRooms();
        }

        static BetrayalParticipant ToParticipant(DungeonPlayer p) =>
            new BetrayalParticipant { PlayerId = p.Id, PlayerName = p.Name, Run = p.RunState };

        void StartFirstFloor()
        {
            var run = _session.Run;
            _floor.RefreshPlayerCount(run.PartySize, _players.Count);
            if (run.HasMap)
            {
                var theme = FindTheme(run.ThemePath) ?? _floor.PickThemeForFloor(run.Floor);
                _floor.RestoreMap(run.Rooms, run.Floor, theme);
            }
            else
            {
                _floor.CurrentFloor = 1;
                _floor.FloorTheme = _floor.PickThemeForFloor(1);
                _floor.StartNewMap();
                foreach (var p in _players.Values)
                {
                    Quests.OnFloorReached(p.Run, 1);
                }
                InjectSpecialRooms();
            }
            run.Floor = _floor.CurrentFloor;
            run.ThemePath = _floor.FloorTheme != null ? _floor.FloorTheme.Id : "";
            UpdateCombatFloors();

            foreach (var p in _players.Values)
            {
                NetBus.Notify(p.Id, new FloorTitleMsg { Title = _floor.CurrentFloor + "층", Subtitle = FloorRules.GetTierName(_floor.CurrentFloor) + " · " + ThemeName() });
            }
        }

        DungeonTheme FindTheme(string id)
        {
            if (string.IsNullOrEmpty(id)) return null;
            foreach (var tier in _floor.Settings.TierThemes)
            {
                foreach (var t in tier.Themes)
                {
                    if (t != null && t.Id == id) return t;
                }
            }
            return null;
        }

        string ThemeName() => _floor.FloorTheme != null ? _floor.FloorTheme.DisplayName : "";

        void UpdateCombatFloors()
        {
            foreach (var h in _combatHosts) h.CurrentFloor = _floor.CurrentFloor;
        }

        // ------------------------------------------------------------ 매 프레임 (언리얼 타이머)

        void Update()
        {
            double now = Now;
            _timers.Tick(now);
            foreach (var c in _combats) c.Tick(now);
            _floor.Tick(now);

            if (Time.unscaledTime >= _nextSync)
            {
                _nextSync = Time.unscaledTime + 0.1f;
                // 2초마다 한 번은 통째로 (놓친 사람 대비)
                bool force = Time.unscaledTime >= _nextForceSync;
                if (force) _nextForceSync = Time.unscaledTime + 2f;
                SyncState(force);
            }
        }

        public void SyncState(bool force)
        {
            if (TerminusPlayer.AnyServerSide() == null) return;

            var map = BuildMapSnapshot();
            var mapJson = JsonUtility.ToJson(map);
            if (force || mapJson != _lastMapJson)
            {
                _lastMapJson = mapJson;
                NetBus.Broadcast(map);
            }

            var areas = BuildAreasSnapshot();
            var areasJson = JsonUtility.ToJson(areas);
            if (force || areasJson != _lastAreasJson)
            {
                _lastAreasJson = areasJson;
                NetBus.Broadcast(areas);
            }

            _session.SyncParty(force);
        }

        // ------------------------------------------------------------ 층 / 런 끝 (언리얼 AMapManager 글루)

        void OnFloorAdvanced(FloorArrival arrival)
        {
            var run = _session.Run;
            run.Floor = arrival.Floor;
            run.ThemePath = arrival.Theme != null ? arrival.Theme.Id : "";
            UpdateCombatFloors();

            foreach (var p in _players.Values)
            {
                p.Run.BeginFloor();
                p.Battler.Revive(1f);
                if (arrival.bNewTheme) Quests.ExpireForNewTheme(p.Run);
                Quests.OnFloorReached(p.Run, arrival.Floor);
            }
            // 지도는 이 이벤트 다음에 만들어짐 -> 특수 방 주입과 저장은 다음 프레임에
            StartCoroutine(AfterFloorAdvanced(arrival));
        }

        System.Collections.IEnumerator AfterFloorAdvanced(FloorArrival arrival)
        {
            yield return null;
            InjectSpecialRooms();
            foreach (var p in _players.Values)
            {
                NetBus.Notify(p.Id, new FloorTitleMsg { Title = arrival.Title, Subtitle = arrival.Subtitle });
            }
            if (!string.IsNullOrEmpty(arrival.ChatText)) _session.SystemChat(arrival.ChatText);
            SaveRun();
        }

        void InjectSpecialRooms()
        {
            var rooms = _floor.Rooms;
            if (Quests.InjectSpecialRooms(rooms, _floor.CurrentFloor, ThemeName(), _players.Values.Select(p => p.RunState), _rng))
            {
                _session.Run.SetRooms(rooms);
                _lastMapJson = "";
            }
        }

        void OnRoomsFinished(bool bossCleared)
        {
            if (bossCleared)
            {
                bool single = RoomSelectionRules.IsSinglePlayerRun(_players.Values.Count(p => p.bConnected), false);
                _floor.HandleBossCleared(single, _players.Values.Count(p => p.bConnected), Now);
            }
            else
            {
                SaveRun();
            }
        }

        void OnBetrayalRequested(ulong? betrayerId)
        {
            var all = _players.Values.Where(p => p.bConnected).ToList();
            var betrayer = betrayerId.HasValue && _players.TryGetValue(betrayerId.Value, out var b) ? b : null;
            var order = BetrayalRules.BuildBattleOrder(all.Select(p => p.Id).ToList(), betrayerId);
            var ordered = order.Select(id => _players[id]).ToList();
            if (betrayer != null && BetrayalRules.CanStartBattle(betrayerId, order))
            {
                _areas.StartBetrayal(ordered, betrayer, _floor.FloorTheme, _floor.Settings.BetrayalVictimHealthCut, _floor.Settings.BetrayerStatPerOpponent);
                _session.SystemChat(BetrayalRules.StartNotice(betrayer.Name));
            }
            else
            {
                _floor.EndRunByEscape(BetrayalRules.MsgNoOpponent);
            }
        }

        // 언리얼 AMapManager::EndRun: 세이브 삭제 + 각자 정산
        void OnRunEnded(RunEndInfo info)
        {
            _session.Saves.DeleteRunSave(_session.Run.SaveSlot);
            _areas.AbortAllRooms();
            foreach (var p in _players.Values)
            {
                NetBus.Notify(p.Id, new RunEndedMsg
                {
                    Message = info.MessageFor(p.Id),
                    bDeath = info.bDeath,
                    Adjust = info.AdjustFor(p.Id) ?? new SettlementAdjust(),
                    RoomName = _session.Run.RoomName,
                    Run = p.RunState,
                    Floor = _floor.CurrentFloor,
                });
            }
        }

        // 언리얼 UTerminusSaveSubsystem::SaveCurrentRun
        void SaveRun()
        {
            if (_floor.IsRunEnded) return;
            var req = new RunSaveRequest
            {
                bMultiplayer = !NetworkRoot.IsSinglePlayer,
                MapRooms = _floor.Rooms,
                MapFloor = _floor.CurrentFloor,
                MapTheme = _floor.FloorTheme != null ? _floor.FloorTheme.Id : "",
            };
            // 세이브의 플레이어 아이디는 온라인 아이디 (연결 번호는 다시 들어오면 바뀜). 이어하기 / 이공간이 이걸로 사람을 맞춤
            foreach (var p in _players.Values)
            {
                var onlineId = _session.Find(p.Id)?.OnlineId ?? "";
                if (p.bConnected) req.Players.Add(RunSavePlayer.FromLive(onlineId, p.Name, p.RunState, p.Battler.State.Health));
                else req.DepartedPlayers.Add(RunSavePlayer.FromDeparted(onlineId, p.Name, p.RunState));
            }
            // 나가서 이공간에서 기다리는 사람 (배틀러가 없어 _players 에 없음)
            if (_rift != null)
            {
                foreach (var d in _rift.DepartedPlayers)
                {
                    req.DepartedPlayers.Add(RunSavePlayer.FromDeparted(d.PlayerId, d.PlayerName, d.RunState));
                }
            }
            if (_session.Saves.SaveCurrentRun(_session.Run, req)) _session.SystemChat("자동 저장했습니다.");
        }


        // ------------------------------------------------------------ 스냅샷

        void FillPlayerInfo(ulong clientId, PlayerInfo info)
        {
            if (!_players.TryGetValue(clientId, out var p)) return;
            info.bHasBattler = true;
            info.Stats = p.Battler.Stats;
            info.Combat = p.Battler.State;
            var area = _areas.GetCurrentArea(p);
            info.AreaIndex = area != null ? area.AreaIndex : -1;
        }

        MapSnapshot BuildMapSnapshot()
        {
            return new MapSnapshot
            {
                Floor = _floor.CurrentFloor,
                LastFloor = _floor.Settings.LastFloor,
                ThemeId = _floor.FloorTheme != null ? _floor.FloorTheme.Id : "",
                ThemeName = ThemeName(),
                TierName = FloorRules.GetTierName(_floor.CurrentFloor),
                bRunEnded = _floor.IsRunEnded,
                Rooms = _floor.Rooms,
                Vote = _floor.Vote.State,
            };
        }

        AreasSnapshot BuildAreasSnapshot()
        {
            var snap = new AreasSnapshot();
            foreach (var area in _areas.GetSortedAreas())
            {
                var info = new AreaInfo
                {
                    AreaIndex = area.AreaIndex,
                    bInUse = area.IsInUse,
                    bCleared = area.IsCleared,
                    Room = area.Room ?? new RoomNode(),
                    SetId = area.CurrentSet != null ? area.CurrentSet.Id : "",
                };
                foreach (var o in area.Occupants) info.Occupants.Add(o.Id);

                if (area.Combat is DungeonCombat combat)
                {
                    info.Phase = combat.Phase.ToString();
                    info.Cycle = combat.Cycle;
                    foreach (var p in combat.EndedTurn) info.EndedTurn.Add(p.Id);
                    info.bBetrayal = combat.IsBetrayal();
                    info.BetrayerId = combat.Betrayer != null ? combat.Betrayer.Id : 0;
                    info.bBetrayerTurn = combat.IsBetrayerTurn();
                    info.bBetrayerWon = combat.DidBetrayerWin();
                    foreach (var m in combat.Monsters)
                    {
                        var visual = GameData.FindMonsterVisualRow(m.RowName);
                        var mi = new MonsterInfo
                        {
                            Id = m.Id,
                            Index = m.Index,
                            Slot = m.SlotIndex,
                            Row = m.RowName,
                            Name = m.Row != null ? m.Row.DisplayName_KR : m.RowName,
                            VisualSource = visual != null ? visual.Source : "",
                            bHide = m.bHideRequested,
                            Stats = m.Stats.Stats,
                            State = m.Stats.State,
                        };
                        var intent = combat.FindIntent(m);
                        if (intent != null)
                        {
                            mi.bHasIntent = true;
                            mi.Intent = new IntentInfo { SkillRow = intent.SkillRow, Kind = intent.Kind.ToString(), Amount = intent.Amount, HitCount = intent.HitCount };
                        }
                        info.Monsters.Add(mi);
                    }
                }
                snap.Areas.Add(info);
            }
            return snap;
        }

        // ------------------------------------------------------------ IAreaNotifier (언리얼 Client_* RPC)

        public void BroadcastSystemChat(string text) => _session.SystemChat(text);

        public void ShowRest(DungeonPlayer to, IReadOnlyList<DungeonPlayer> occupants, float healRatio, int exploreCurrencyMin, int exploreCurrencyMax, float exploreRelicChance) =>
            NetBus.Notify(to.Id, new ShowRestMsg
            {
                Occupants = occupants.Select(o => o.Id).ToList(),
                HealRatio = healRatio,
                ExploreMin = exploreCurrencyMin,
                ExploreMax = exploreCurrencyMax,
                RelicChance = exploreRelicChance,
            });

        public void RestResult(DungeonPlayer to, string result) => NetBus.Notify(to.Id, new RestResultMsg { Result = result });
        public void CloseRest(DungeonPlayer to) => NetBus.Notify(to.Id, new CloseRestMsg());
        public void ShowShop(DungeonPlayer to, ShopState state) => NetBus.Notify(to.Id, new ShowShopMsg { State = state });
        public void CloseShop(DungeonPlayer to) => NetBus.Notify(to.Id, new CloseShopMsg());

        public void ShowEvent(DungeonPlayer to, string title, string situation, IReadOnlyList<EventOption> options) =>
            NetBus.Notify(to.Id, new ShowEventMsg { Title = title, Situation = situation, Options = options.ToList() });

        public void EventResult(DungeonPlayer to, string result) => NetBus.Notify(to.Id, new EventResultMsg { Result = result });
        public void CloseEvent(DungeonPlayer to) => NetBus.Notify(to.Id, new CloseEventMsg());

        public void ShowRescue(DungeonPlayer to, bool bChooser, IReadOnlyList<string> wipedNames, bool bCanIntervene, float healthCost, float seconds) =>
            NetBus.Notify(to.Id, new ShowRescueMsg { bChooser = bChooser, WipedNames = wipedNames.ToList(), bCanIntervene = bCanIntervene, HealthCost = healthCost, Seconds = seconds });

        public void CloseRescue(DungeonPlayer to) => NetBus.Notify(to.Id, new CloseRescueMsg());
        public void EnterRift(DungeonPlayer to, IReadOnlyList<string> waitingFor, bool bFromSave) => NetBus.Notify(to.Id, new EnterRiftMsg { WaitingFor = waitingFor.ToList(), bFromSave = bFromSave });
        public void LeaveRift(DungeonPlayer to) => NetBus.Notify(to.Id, new LeaveRiftMsg());
    }
}
