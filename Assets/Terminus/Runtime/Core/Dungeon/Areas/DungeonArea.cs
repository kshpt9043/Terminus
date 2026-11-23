// 던전 구역 (언리얼 Dungeon/DungeonArea 의 서버 규칙 부분)
// 언리얼 AActor 의 카메라 / 무대 ChildActor / 배틀러 순간이동 / 복제는 Game 쪽. 여기는 누가 어느 자리에 섰는지와 방 진행만
// 방 타입별 콘텐츠는 partial 로 나눔: DungeonArea.Rest / .Event / .Shop
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Dungeon
{
    /// <summary>구역 안에서 선 자리. 플레이어 줄(왼쪽) 또는 몬스터 줄(오른쪽, 배신자)</summary>
    public readonly struct AreaSlot
    {
        public readonly bool bMonsterSide;
        public readonly int Index;

        public AreaSlot(bool monsterSide, int index)
        {
            bMonsterSide = monsterSide;
            Index = index;
        }

        public override string ToString() => (bMonsterSide ? "M" : "P") + Index;
    }

    /**
     * 던전 구역. 지도의 방 하나가 실제로 펼쳐지는 무대.
     *
     * 멀티에서 파티가 서로 다른 방에 들어갈 수 있어서, 레벨 하나에 구역을 여러 개(최대 인원 수만큼) 두고
     * 방마다 구역 하나를 배정한다. 같은 방을 고른 사람들은 같은 구역에 들어감.
     * 전투 방만 쓰는 게 아니라 상점/휴식터/이벤트 방도 구역을 하나 받는다
     *
     * 방(RoomNode) = 지도 위의 칸, 구역(DungeonArea) = 그 방이 펼쳐지는 장소
     *
     * 배정/종료 관리는 DungeonAreaManager 가 함. 구역은 자기 무대만 책임짐
     */
    public sealed partial class DungeonArea
    {
        // 플레이어 서는 자리 수 / 몬스터 서는 자리 수 (언리얼 기본: 플레이어 4, 몬스터 3. 기획상 한 방에 1 ~ 3 마리)
        public const int DefaultPlayerSlots = 4;
        public const int DefaultMonsterSlots = 3;

        public DungeonArea(int areaIndex, IAreaCombat combat, int playerSlotCount = DefaultPlayerSlots, int monsterSlotCount = DefaultMonsterSlots)
        {
            AreaIndex = areaIndex;
            Combat = combat;
            NumPlayerSlots = Math.Max(0, playerSlotCount);
            NumMonsterSlots = Math.Max(0, monsterSlotCount);

            if (Combat != null)
            {
                Combat.AreaCleared += MarkCleared;
                Combat.AreaWiped += MarkWiped;
                Combat.BetrayalFinished += OnBetrayalFinished;
            }
        }

        // 구역 번호. 배정은 번호가 작은 구역부터. 레벨에 놓을 때 0 ~ 3 으로 겹치지 않게
        public int AreaIndex { get; }

        // 이 구역의 전투 진행. 전투 방이 아니면 싸우지 않음
        public IAreaCombat Combat { get; }

        public int NumPlayerSlots { get; }
        public int NumMonsterSlots { get; }

        // 등록한 관리자 (언리얼 GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
        public DungeonAreaManager Manager { get; internal set; }

        // ---- 복제되는 진행 상태. 클라의 HUD / 관전이 보고 쓸 것 (Changed 를 받은 Game 쪽이 보냄)

        public bool IsInUse { get; private set; }
        public bool IsCleared { get; private set; }

        // 전멸해서 구출 / 난입을 기다리는 중 (서버만)
        public bool IsWiped { get; private set; }

        RoomNode _room = new RoomNode();

        // 지금 이 구역에서 진행 중인 방. IsInUse 가 false 면 의미 없음
        public RoomNode Room => _room;

        readonly List<DungeonPlayer> _occupants = new List<DungeonPlayer>();

        // 이 구역에 들어와 있는 플레이어들 (들어온 순서 = 전투 대상 인덱스)
        public IReadOnlyList<DungeonPlayer> Occupants => _occupants;

        readonly Dictionary<DungeonPlayer, AreaSlot> _slots = new Dictionary<DungeonPlayer, AreaSlot>();

        // 지금 띄울 무대. 서버가 고르고 이름만 복제 -> 각 머신이 로컬로 띄움. null 이면 지금 떠 있는 무대(레벨 미리보기) 유지
        public DungeonAreaSet CurrentSet { get; private set; }

        // BeginRoom 에서 받은 이 층 테마 (이벤트 테마 / 쉬는 가디언 전투)
        public DungeonTheme CurrentTheme { get; private set; }

        /// <summary>복제할 값(IsInUse / IsCleared / Room / Occupants / CurrentSet)이 바뀜</summary>
        public event Action<DungeonArea> Changed;

        // 이 사람이 선 자리. 이 구역에 없으면 false
        public bool TryGetSlot(DungeonPlayer player, out AreaSlot slot)
        {
            slot = default;
            return player != null && _slots.TryGetValue(player, out slot);
        }

        public bool Contains(DungeonPlayer player) => player != null && _occupants.Contains(player);

        // ------------------------------------------------------------ 관리자에서 빌려 쓰는 것

        DungeonAreaSettings Settings => Manager != null ? Manager.Settings : DefaultSettings;
        static readonly DungeonAreaSettings DefaultSettings = new DungeonAreaSettings();

        Random Rng => Manager != null ? Manager.Rng : FallbackRng;
        static readonly Random FallbackRng = new Random(0);

        int CurrentFloor => Manager != null ? Manager.CurrentFloor : 1;

        DungeonTimers Timers => Manager?.Timers;

        void Chat(string text) => Manager?.Notifier?.BroadcastSystemChat(text);

        IAreaNotifier Notifier => Manager?.Notifier;

        void Log(string text) => Manager?.Log($"[Area {AreaIndex}] {text}");

        void SetTimer(TimerHandle handle, double delay, Action callback)
        {
            if (Timers != null) Timers.Set(handle, delay, callback);
            else callback();   // 시계가 없으면 바로 (테스트 / 관리자 없이)
        }

        void ClearTimer(TimerHandle handle) => Timers?.Clear(handle);

        void NotifyChanged() => Changed?.Invoke(this);

        void Place(DungeonPlayer player, AreaSlot slot)
        {
            _occupants.Add(player);
            _slots[player] = slot;

            // 시점 전환은 각 클라가 이 값의 복제를 받고 스스로 함 (언리얼 PS->SetCurrentArea(this))
            Manager?.SetCurrentArea(player, this);
        }

        // =====================================================================
        // 방 시작
        // =====================================================================

        /// <summary>
        /// 이 구역에서 방 하나를 시작. 테마에 맞는 무대를 고르고, 플레이어들을 슬롯에 세운다
        /// theme 이 없으면 무대는 지금 떠 있는 것(레벨에서 지정한 미리보기) 그대로
        /// </summary>
        public void BeginRoom(RoomNode inRoom, IReadOnlyList<DungeonPlayer> inPlayers, DungeonTheme theme)
        {
            _room = inRoom != null ? inRoom.Clone() : new RoomNode();
            CurrentTheme = theme;
            IsInUse = true;
            IsCleared = false;
            IsWiped = false;
            _occupants.Clear();
            _slots.Clear();

            // 테마에서 이 방 타입에 맞는 무대를 고름. 세트가 여러 개면 방마다 랜덤
            PickSet(theme, _room.Type);

            if (inPlayers != null)
            {
                for (int i = 0; i < inPlayers.Count; ++i)
                {
                    var p = inPlayers[i];
                    if (p == null) continue;

                    if (i >= NumPlayerSlots)
                    {
                        Log($"플레이어 슬롯 부족 ({i}번째 플레이어). 구역 중심에 세움");
                    }
                    // 배틀러를 슬롯으로. 위치 이동은 Game 쪽이 자리(AreaSlot)를 보고
                    Place(p, new AreaSlot(false, i));
                }
            }

            Log($"{_room.RoomId}번 방(Row {_room.Row}, {_room.Type}) 시작. 인원 {_occupants.Count}");
            NotifyChanged();

            // 방 타입별 콘텐츠
            //  - 몬스터 / 가디언 / 보스: 전투 (끝나면 전투 쪽이 MarkCleared)
            //  - 휴식터: 회복 대상 고르기 (전원이 고르면 MarkCleared)
            //  - 이벤트: DT_Event 하나, 각자 선택지 중 하나 고르기 (전원이 고르면 MarkCleared, 쉬는 가디언이면 전투)
            //  - 상점: 각자 사고팔기, 전원이 나가면 MarkCleared
            //  - 퀘스트: 각자 퀘스트 3개 중 1개 수락 / 거절 (전원이 고르면 MarkCleared)
            //  - 퀘스트 특수 방: 지정 몬스터와 전투

            // 퀘스트 '한 층에서 몬스터 / 가디언 방 N 번'
            foreach (var p in _occupants)
            {
                Quests.OnRoomEntered(p.Run, _room.Type);
            }

            if (_room.Type == RoomType.QUEST)
            {
                BeginQuestRoom();
                return;
            }
            if (!Ids.IsNone(_room.SpecialRoom) && Combat != null)
            {
                var special = Quests.GetSpecialRoomMonsters(_room.SpecialRoom);
                Log($"퀘스트 특수 방 {_room.SpecialRoom}: 몬스터 {special.Count}");
                Combat.SetNextForcedMonsters(special);
            }

            if (_room.Type == RoomType.BREAK)
            {
                BeginRest();
                return;
            }
            if (_room.Type == RoomType.EVENT)
            {
                BeginEvent();
                return;
            }
            if (_room.Type == RoomType.STORE)
            {
                BeginShop();
                return;
            }

            Combat?.StartCombat(_room, new List<DungeonPlayer>(_occupants), theme);
        }

        void PickSet(DungeonTheme theme, RoomType type)
        {
            if (theme == null) return;

            var data = Manager?.FindThemeData(theme);
            var set = data?.PickAreaSet(type, Rng);
            if (set != null)
            {
                CurrentSet = set;
            }
            else
            {
                Log($"테마 {theme} 에 {type} 방에 쓸 무대가 없음");
            }
        }

        // =====================================================================
        // 끝 / 전멸
        // =====================================================================

        /// <summary>이 구역의 방을 끝냈다고 표시. 전 구역이 끝났는지는 관리자가 판단</summary>
        public void MarkCleared()
        {
            if (!IsInUse || IsCleared) return;

            IsCleared = true;
            GiveGuardianWinBonus();

            if (!Ids.IsNone(_room.SpecialRoom))
            {
                foreach (var p in _occupants)
                {
                    Quests.OnSpecialRoomCleared(p.Run, _room.SpecialRoom);
                }
            }

            Log($"{_room.RoomId}번 방 클리어");
            NotifyChanged();

            Manager?.NotifyAreaCleared(this);
        }

        // 싸움이 끝났는가 (클리어했거나, 승리 후 결과·보상 화면). 싸우는 중이거나 전멸해서 구출 / 난입을 기다리면 false
        public bool IsFightOver()
        {
            if (IsCleared) return true;
            if (IsWiped) return false;

            return Combat != null && Combat.IsVictory;
        }

        /// <summary>이 구역 전멸 (전투 패배). 구출 / 난입 / 런 끝은 관리자가 판단</summary>
        public void MarkWiped()
        {
            if (!IsInUse || IsCleared || IsWiped) return;

            IsWiped = true;
            Log($"{_room.RoomId}번 방 전멸");

            Manager?.NotifyAreaWiped(this);
        }

        void OnBetrayalFinished(DungeonPlayer betrayer, bool bBetrayerWon)
        {
            Manager?.NotifyBetrayalFinished(this, betrayer, bBetrayerWon);
        }

        // =====================================================================
        // 배신 전투 / 난입
        // =====================================================================

        /// <summary>[배신 전투] 팀은 플레이어 자리, 배신자는 몬스터 자리에 세우고 배신 전투 시작</summary>
        public void BeginBetrayal(IReadOnlyList<DungeonPlayer> team, DungeonPlayer betrayer, DungeonTheme theme, float victimHealthCut, int statPerOpponent)
        {
            if (betrayer == null) return;

            _room = new RoomNode
            {
                RoomId = -1,
                Row = -1,
                Type = RoomType.BOSS,   // 무대는 보스방 것
            };
            CurrentTheme = theme;
            IsInUse = true;
            IsCleared = false;
            IsWiped = false;
            _occupants.Clear();
            _slots.Clear();

            PickSet(theme, RoomType.BOSS);

            // 순서 = 전투 Players 순서 = 대상 인덱스: 팀 먼저, 배신자 마지막
            int slotIndex = 0;
            if (team != null)
            {
                foreach (var p in team)
                {
                    if (p != null && p != betrayer && !_occupants.Contains(p)) Place(p, new AreaSlot(false, slotIndex++));
                }
            }
            Place(betrayer, new AreaSlot(true, 0));   // 배신자는 맞은편 (몬스터 자리)

            Log($"배신 전투: {betrayer.Name} vs {_occupants.Count - 1}명");
            NotifyChanged();

            Combat?.StartBetrayal(new List<DungeonPlayer>(_occupants), betrayer, victimHealthCut, statPerOpponent);
        }

        /// <summary>[난입] 다른 구역 사람들이 이 구역에 들어와 남은 몬스터와 이어서 싸움</summary>
        public void BeginIntervention(IReadOnlyList<DungeonPlayer> joiners)
        {
            if (!IsInUse || !IsWiped) return;

            IsWiped = false;

            if (joiners != null)
            {
                foreach (var p in joiners)
                {
                    if (p == null || _occupants.Contains(p)) continue;
                    Place(p, new AreaSlot(false, _occupants.Count));
                }
            }

            Log($"난입: {joiners?.Count ?? 0}명 합류");
            NotifyChanged();

            Combat?.ResumeWithPlayers(joiners != null ? new List<DungeonPlayer>(joiners) : new List<DungeonPlayer>());
        }

        // =====================================================================
        // 비우기
        // =====================================================================

        /// <summary>구역 비우기. 열린 화면을 닫고 전투를 정리하고 자리를 비운다 (배틀러를 원래 자리로 돌리는 건 Game 쪽)</summary>
        public void Release()
        {
            // 휴식터 화면 닫기 (전원 고르기 전에 닫히는 경우 포함: 누가 나가서 방 무효 등)
            if (_bResting)
            {
                ClearTimer(_restTimer);
                foreach (var p in _occupants) Notifier?.CloseRest(p);
                _bResting = false;
                _restChosen.Clear();
            }

            // 상점 화면 닫기
            if (_bShopping)
            {
                ClearTimer(_shopTimer);
                foreach (var p in _occupants) Notifier?.CloseShop(p);
                _bShopping = false;
                _shopStates.Clear();
                _shopDone.Clear();
            }

            // 이벤트 화면 닫기
            if (_bInEvent)
            {
                ClearTimer(_eventTimer);
                foreach (var p in _occupants) Notifier?.CloseEvent(p);
                _bInEvent = false;
                _eventOffers.Clear();
                _eventChosen.Clear();
            }
            _currentEvent = "";
            _eventGuardianHealth = 0f;
            _eventGuardianBonus.Clear();

            // 퀘스트 방 화면 닫기
            if (_bInQuest)
            {
                ClearTimer(_eventTimer);
                foreach (var p in _occupants) Notifier?.CloseEvent(p);
                _bInQuest = false;
                _questOffers.Clear();
            }

            // 몬스터 치우고 전투 상태 초기화
            Combat?.EndCombat();

            foreach (var p in _occupants)
            {
                Manager?.ClearCurrentArea(p, this);
            }

            _occupants.Clear();
            _slots.Clear();
            _room = new RoomNode();
            IsInUse = false;
            IsCleared = false;
            IsWiped = false;
            NotifyChanged();
        }
    }
}
