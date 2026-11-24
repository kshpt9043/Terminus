// 던전 구역 하나의 전투 진행 (언리얼 Dungeon/DungeonCombatComponent 이식)
// 언리얼에선 ADungeonArea 에 붙은 UActorComponent + 복제였음 -> 여기선 서버 규칙만 도는 일반 클래스.
//  - 복제(Phase / Cycle / Monsters / Intents / EndedTurn / Betrayer / bBetrayerTurn / bBetrayerWon)는 Game 쪽이 StateChanged 를 받아 보냄
//  - 몬스터 액터 스폰 / 숨김 / 연출, 클라 RPC(보상 화면), 구역 / 맵 매니저 알림은 아래 event 로 바깥에 맡김
//  - 타이머(StepTimer)는 Tick(now) 로 시간을 받아 돌림. 랜덤은 생성자에서 받은 System.Random
// 판정은 전부 서버. 이 클래스는 서버에서만 만듦 (언리얼 HasAuthority 검사 자리)
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Dungeon
{
    /// <summary>
    /// 던전 구역 하나의 전투 진행. 구역(DungeonArea)마다 하나씩
    /// -> 파티가 갈라져 여러 방에서 동시에 싸워도 구역마다 따로 돈다
    ///
    /// 사이클 = 플레이어 턴 + 몬스터 턴 (기획 전투 로직)
    ///  1. 사이클 시작: 몬스터 행동 예고, 플레이어 보호막 제거 + 에너지 회복
    ///  2. 플레이어 턴: 이 구역 플레이어 전원이 동시에 스킬 사용. 각자 턴 종료 (중독 피해, 에너지 0)
    ///  3. 전원 턴 종료 -> 몬스터 턴: 살아 있는 몬스터가 예고대로 한 마리씩 (몬스터 보호막은 자기 차례 직전에 제거)
    ///  4. 사이클 끝: 몬스터 중독 피해, 모두의 상태이상 지속시간 1 감소 -> 다음 사이클
    ///  몬스터 전멸 = 승리 -> 구역 클리어 (지도로). 플레이어 전멸 = 패배
    ///
    /// 판정은 전부 서버. 클라는 자기 PC 의 Server_UseSkill / Server_EndTurn 으로 요청만 보냄
    /// </summary>
    public sealed partial class DungeonCombat : IAreaCombat
    {
        // 엔진 없는 Core 라 UE_LOG 대신. Game 쪽이 Debug.Log 를 꽂음 (안 꽂으면 버림)
        public static Action<string> LogInfo;
        public static Action<string> LogWarning;

        // 유물 효과가 또 유물을 부르는 최대 깊이 (언리얼 코드에 8 로 박혀 있었음)
        public const int MaxRelicDepth = 8;

        readonly DungeonCombatSettings _settings;
        readonly IDungeonCombatHost _host;
        readonly Random _rng;

        public DungeonCombat(DungeonCombatSettings settings = null, IDungeonCombatHost host = null, Random rng = null)
        {
            _settings = settings ?? new DungeonCombatSettings();
            _host = host;
            _rng = rng ?? new Random();
        }

        public DungeonCombatSettings Settings => _settings;

        // =====================================================================
        // 바깥 알림 (언리얼에서 구역 / 맵 매니저 / PC / 몬스터 액터를 직접 부르던 곳)
        // =====================================================================

        // 복제되는 진행 상태가 바뀜 (Phase / Cycle / Monsters / Intents / EndedTurn / Betrayer / 배신 차례 / 결과)
        public event Action StateChanged;

        // 몬스터 스폰 (언리얼 SpawnActorDeferred + SetMonsterRow). Game 쪽이 SlotIndex 자리에 액터를 세움
        public event Action<DungeonMonster> MonsterSpawned;

        // 몬스터 제거 (언리얼 EndCombat 의 Monster->Destroy)
        public event Action<DungeonMonster> MonsterRemoved;

        // 죽은 몬스터 숨김 요청 (언리얼 ATerminusMonster::HideAfterDeath: 마지막 피격 + 사망을 다 보여 준 뒤 숨김)
        public event Action<DungeonMonster> MonsterHideRequested;

        // 몬스터가 예고한 행동을 하기 직전 (언리얼 ATerminusMonster::PlayIntentAnim)
        public event Action<DungeonMonster, MonsterIntentKind> MonsterActing;

        // 스킬을 쓰기 직전 (스킬 이펙트용): 스킬 행, 맞는 몬스터 Id, 맞는 플레이어 Id
        public event Action<string, List<int>, List<ulong>> SkillUsed;

        // 반격이 터짐 (이펙트용): 반격 맞는 몬스터 Id, 플레이어 Id
        public event Action<List<int>, List<ulong>> CounterHit;

        // 구역 클리어 (언리얼 ClearArea -> ADungeonArea::MarkCleared)
        public event Action AreaCleared;

        // 구역 전멸 (언리얼 ReportWipe -> ADungeonArea::MarkWiped). 구조 / 난입은 구역 쪽이 판단
        public event Action AreaWiped;

        // 배신 전투 결과 (언리얼 ReportBetrayalResult -> AMapManager::HandleBetrayalResult(Betrayer, bBetrayerWon))
        public event Action<DungeonPlayer, bool> BetrayalFinished;

        // 방 클리어 보상 화면 (언리얼 PC->Client_ShowRoomReward). 재화는 이미 지급됨
        public event Action<DungeonPlayer, RoomRewardOffer> RoomRewardOffered;

        // 보상 화면 닫기 (언리얼 PC->Client_CloseMonsterReward). 보상 중에 전투가 정리될 때
        public event Action<DungeonPlayer> RoomRewardClosed;

        // =====================================================================
        // 상태
        // =====================================================================

        float _nextSpawnHealthScale = 1f;
        readonly List<string> _nextForcedMonsters = new List<string>();

        // ---- 복제되는 진행 상태
        CombatPhase _phase = CombatPhase.None;
        int _cycle = 0;
        readonly List<DungeonMonster> _monsters = new List<DungeonMonster>();
        readonly List<MonsterIntent> _intents = new List<MonsterIntent>();

        // 배신 전투: 배신자 (없으면 보통 전투), 지금 배신자 차례인가, 결과
        DungeonPlayer _betrayer;
        bool _bBetrayerTurn = false;
        bool _bBetrayerWon = false;

        // 이번 플레이어 턴에 턴 종료를 누른 사람
        readonly List<DungeonPlayer> _endedTurn = new List<DungeonPlayer>();

        // ---- 서버만

        // 이 구역의 플레이어 (구역 Occupants 와 같은 순서). 나간 사람(bConnected = false)은 언리얼의 끊긴 약한 포인터처럼 없는 셈
        readonly List<DungeonPlayer> _players = new List<DungeonPlayer>();

        // 몬스터 턴에서 다음에 행동할 몬스터
        int _nextMonsterIndex = 0;

        // 스킬이 비어 임시 스킬을 쓴다고 이미 알린 몬스터 행 (로그 한 번만)
        readonly HashSet<string> _warnedFallbackRows = new HashSet<string>();

        int _nextMonsterId = 1;

        // 지금 방 번호 (로그용)
        int _currentRoomId = -1;

        // 지금 방 종류 (가디언 / 보스 전투 시작 발동, 보상 종류)
        RoomType _currentRoomType = RoomType.MONSTER;

        // ---- StepTimer (언리얼 FTimerHandle 하나)
        CombatStep _step = CombatStep.None;
        double _stepTime = 0;
        double _now = 0;

        // 다타 공격의 남은 타격 (HitInterval 마다 하나). 남아 있는 동안은 다음 행동 / 단계가 기다림
        readonly Queue<Action> _hits = new Queue<Action>();
        double _hitTime = 0;
        float _hitInterval = 0;

        /// <summary>
        /// 스킬 행 -> 타격 간격(초). 게임 쪽이 그 스킬 이펙트의 길이를 넣어 줌 (이펙트가 끝나고 다음 타격).
        /// 없거나 0 이하를 주면 Settings.HitInterval. Settings.HitInterval 이 0 이면 언제나 한꺼번에 (테스트)
        /// </summary>
        public Func<string, float> HitIntervalFor;

        /// <summary>다타 공격이 아직 들어가는 중 (스킬 / 턴 종료 요청을 안 받음)</summary>
        public bool IsResolvingHits => _hits.Count > 0;

        // =====================================================================
        // [조회] 클라 HUD 가 씀
        // =====================================================================

        public CombatPhase Phase => _phase;
        public CombatPhase GetPhase() => _phase;
        public bool IsInCombat() => _phase != CombatPhase.None;

        // 구역 쪽이 보는 것 (Dungeon/Areas 의 IAreaCombat). 언리얼 ADungeonArea::IsFightOver / 관전 바가 Phase 로 보던 것
        public bool IsVictory => _phase == CombatPhase.Victory;
        public bool IsTurnInProgress => _phase == CombatPhase.PlayerTurn || _phase == CombatPhase.MonsterTurn;
        bool IAreaCombat.IsBetrayal => IsBetrayal();

        public int Cycle => _cycle;
        public int GetCycle() => _cycle;

        public IReadOnlyList<DungeonMonster> Monsters => _monsters;
        public IReadOnlyList<DungeonMonster> GetMonsters() => _monsters;

        public IReadOnlyList<MonsterIntent> Intents => _intents;
        public IReadOnlyList<MonsterIntent> GetIntents() => _intents;

        // 이 구역 플레이어 (구역 Occupants 순서 = 아군 / 배신 전투 대상 인덱스)
        public IReadOnlyList<DungeonPlayer> Players => _players;

        // 이번 플레이어 턴에 턴 종료를 누른 사람 (복제용)
        public IReadOnlyList<DungeonPlayer> EndedTurn => _endedTurn;

        public RoomType CurrentRoomType => _currentRoomType;
        public int CurrentRoomId => _currentRoomId;

        // 다음 StepTimer 일과 시각 (Game / 테스트용)
        public CombatStep PendingStep => _step;
        public double PendingStepTime => _stepTime;

        // 이 몬스터의 이번 사이클 예고. 없으면 null
        public MonsterIntent FindIntent(DungeonMonster monster)
        {
            return _intents.Find(i => i.Monster == monster);
        }

        public bool HasEndedTurn(DungeonPlayer p) => p != null && _endedTurn.Contains(p);

        // =====================================================================
        // 시간 (언리얼 TimerManager 자리)
        // =====================================================================

        /// <summary>
        /// 서버가 매 프레임 지금 시각(초)을 넣어 줌. 걸린 일이 때가 되면 실행.
        /// 실행한 일이 다시 거는 일은 그 시각 기준이라 한 번 Tick 에 하나씩 진행됨 (간격 0 인 것만 이어서)
        /// </summary>
        public void Tick(double now)
        {
            _now = now;
            if (_hits.Count > 0 && _now >= _hitTime)
            {
                _hitTime = _now + _hitInterval;
                _hits.Dequeue()();
            }
            if (_hits.Count > 0) return;
            for (int guard = 0; guard < 64 && _step != CombatStep.None && _now >= _stepTime; ++guard)
            {
                var step = _step;
                _step = CombatStep.None;
                RunStep(step);
            }
        }

        // 첫 단계는 지금, 나머지는 타격 간격마다 (Settings.HitInterval 0 이면 전부 지금)
        void Resolve(List<Action> steps, string skillRow)
        {
            float custom = _settings.HitInterval > 0f && HitIntervalFor != null ? HitIntervalFor(skillRow) : 0f;
            _hitInterval = custom > 0f ? custom : _settings.HitInterval;
            for (int i = 0; i < steps.Count; ++i)
            {
                if (i == 0 || _settings.HitInterval <= 0f) steps[i]();
                else _hits.Enqueue(steps[i]);
            }
            if (_hits.Count > 0) _hitTime = _now + _hitInterval;
        }

        void SetTimer(CombatStep step, float delay)
        {
            _step = step;
            _stepTime = _now + Math.Max(0f, delay);
        }

        void ClearTimer()
        {
            _step = CombatStep.None;
            _hits.Clear();
        }

        void RunStep(CombatStep step)
        {
            switch (step)
            {
                case CombatStep.MonsterAction: RunNextMonsterAction(); break;
                case CombatStep.ReportWipe: ReportWipe(); break;
                case CombatStep.StartRoomRewards: StartRoomRewards(); break;
                case CombatStep.ClearArea: ClearArea(); break;
                case CombatStep.ReportBetrayalResult: ReportBetrayalResult(); break;
            }
        }

        // =====================================================================
        // 내부 조회
        // =====================================================================

        int CurrentFloor => _host != null ? _host.CurrentFloor : 1;
        int NumMonsterSlots => _host != null ? _host.NumMonsterSlots : 3;

        void NotifyChanged() => StateChanged?.Invoke();

        // 언리얼 GetStats(PS): 폰(배틀러)이 없거나 나간 사람이면 null
        static CombatStats GetStats(DungeonPlayer p)
        {
            return p != null && p.bConnected ? p.Battler : null;
        }

        static bool IsValid(DungeonPlayer p) => p != null && p.bConnected;

        List<CombatStats> GetAlivePlayerStats()
        {
            var output = new List<CombatStats>();
            foreach (var p in _players)
            {
                var stats = GetStats(p);
                if (stats != null && !stats.IsDead()) output.Add(stats);
            }
            return output;
        }

        List<CombatStats> GetAliveMonsterStats()
        {
            var output = new List<CombatStats>();
            foreach (var m in _monsters)
            {
                var stats = m?.Stats;
                if (stats != null && !stats.IsDead()) output.Add(stats);
            }
            return output;
        }

        int RandRange(int min, int max)
        {
            // FMath::RandRange: 양 끝 포함. 최대 < 최소면 최소
            return max <= min ? min : _rng.Next(min, max + 1);
        }

        float FRandRange(float min, float max)
        {
            return min + (float)_rng.NextDouble() * (max - min);
        }

        static bool IsCombatRoom(RoomType type)
        {
            return type == RoomType.MONSTER || type == RoomType.GUARDIAN || type == RoomType.BOSS;
        }

        // 몬스터가 쓰는 스킬의 대상 해석. 몬스터 입장에서 "적" = 플레이어, "아군" = 몬스터
        List<CombatStats> ResolveTargets(TargetType type, CombatStats self, List<CombatStats> enemies, List<CombatStats> allies)
        {
            var output = new List<CombatStats>();
            switch (type)
            {
                case TargetType.Self:
                    output.Add(self);
                    break;
                case TargetType.SingleEnemy:
                    if (enemies.Count > 0) output.Add(enemies[RandRange(0, enemies.Count - 1)]);
                    break;
                case TargetType.AllEnemies:
                    output.AddRange(enemies);
                    break;
                case TargetType.SingleAlly:
                    if (allies.Count > 0) output.Add(allies[RandRange(0, allies.Count - 1)]);
                    break;
                case TargetType.AllAllies:
                    output.AddRange(allies);
                    break;
            }
            return output;
        }

        // =====================================================================
        // [서버 전용] 시작 / 정리
        // =====================================================================

        // 다음 StartCombat 한 번만 몬스터 체력에 곱함 (이벤트 '쉬는 가디언' 70%). StartCombat 이 1 로 되돌림
        public void SetNextSpawnHealthScale(float scale)
        {
            _nextSpawnHealthScale = Math.Max(0.01f, scale);
        }

        // 다음 StartCombat 한 번만 이 몬스터들로 (퀘스트 특수 방). 비어 있으면 평소대로 고름
        public void SetNextForcedMonsters(IReadOnlyList<string> rows)
        {
            _nextForcedMonsters.Clear();
            if (rows != null) _nextForcedMonsters.AddRange(rows);
        }

        /// <summary>몬스터를 스폰하고 첫 사이클 시작. 전투 방(몬스터 / 가디언 / 보스)일 때 구역이 부름</summary>
        public void StartCombat(RoomNode room, IReadOnlyList<DungeonPlayer> players, DungeonTheme theme)
        {
            EndCombat();   // 혹시 남은 게 있으면

            if (room == null || !IsCombatRoom(room.Type)) return;

            if (players != null)
            {
                foreach (var p in players)
                {
                    if (p != null) _players.Add(p);
                }
            }

            _currentRoomType = room.Type;
            _currentRoomId = room.RoomId;
            _pendingRewards.Clear();
            SpawnMonsters(room, theme);
            _nextSpawnHealthScale = 1f;
            _nextForcedMonsters.Clear();
            BuildRelicHolders();

            // 이벤트 방에서 받은 일시 버프: 이번 전투 동안 용기(공격) / 용암(방어 감소)
            foreach (var p in _players)
            {
                ApplyTempBuffs(p);
            }

            if (_monsters.Count == 0)
            {
                LogWarning?.Invoke($"[Combat] {room.RoomId}번 방: 스폰된 몬스터가 없어 바로 클리어");
                FinishCombat(true);
                return;
            }

            LogInfo?.Invoke($"[Combat] {room.RoomId}번 방({room.Type}) 전투 시작. 플레이어 {_players.Count}, 몬스터 {_monsters.Count}");

            _cycle = 0;
            StartCycle();
        }

        /// <summary>전투 정리 (몬스터 제거, 타이머 정지). 구역을 비울 때</summary>
        public void EndCombat()
        {
            ClearTimer();

            // 싸우는 도중에 닫힘(누가 나가서 방 취소): 전투에서만 의미 있는 효과를 지우고, 쓰러진 사람은 체력 1 로
            if (_phase == CombatPhase.PlayerTurn || _phase == CombatPhase.MonsterTurn)
            {
                foreach (var p in _players)
                {
                    var stats = GetStats(p);
                    if (stats == null) continue;

                    stats.ClearCombatEffects();
                    if (stats.IsDead())
                    {
                        var saved = stats.Stats.Clone();
                        stats.InitFrom(saved);
                        stats.ApplyDamage(saved.MaxHealth - 1);
                    }
                }
            }

            // 보상 화면이 떠 있던 사람은 닫음
            foreach (var reward in _pendingRewards)
            {
                if (IsValid(reward.Player) && !reward.bDone)
                {
                    RoomRewardClosed?.Invoke(reward.Player);
                }
            }

            ClearRelicHolders();

            var removed = new List<DungeonMonster>(_monsters);
            _monsters.Clear();
            _intents.Clear();
            _pendingRewards.Clear();
            _endedTurn.Clear();
            _players.Clear();
            _cycle = 0;
            _phase = CombatPhase.None;
            _betrayer = null;
            _bBetrayerTurn = false;
            _bBetrayerWon = false;

            foreach (var m in removed)
            {
                MonsterRemoved?.Invoke(m);
            }
            NotifyChanged();
        }

        // 이벤트 방에서 받은 일시 버프를 상태로 (이번 전투 동안)
        void ApplyTempBuffs(DungeonPlayer p)
        {
            var stats = GetStats(p);
            if (p == null || stats == null || p.RunState == null) return;

            foreach (var buff in p.RunState.TempBuffs)
            {
                if (buff.Attack > 0) stats.ApplyStatus(StatusEffect.Brave, buff.Attack, -1);
                if (buff.Defense < 0) stats.ApplyStatus(StatusEffect.Lava, -buff.Defense, -1);
            }
        }

        /// <summary>
        /// [난입] 전멸한 이 구역의 전투를 새로 들어온 사람들과 이어서 함 (몬스터 체력은 그대로). 패배 상태에서만
        /// </summary>
        public void ResumeWithPlayers(IReadOnlyList<DungeonPlayer> joiners)
        {
            if (_phase != CombatPhase.Defeat) return;

            ClearTimer();

            // 기획 사망 순서도 '난입': 남아 있는 몬스터들과 새롭게 전투. 몬스터 체력은 그대로, 쓰러진 사람은 이기면 체력 1 로
            // 들어온 사람만 유물 홀더 추가 (몬스터 유물의 남은 횟수 같은 건 그대로 둠)
            var joinerHolders = new List<int>();
            int joined = 0;
            if (joiners != null)
            {
                foreach (var p in joiners)
                {
                    if (p == null || _players.Contains(p)) continue;
                    _players.Add(p);
                    ++joined;
                    ApplyTempBuffs(p);

                    int holderIndex = AddRelicHolder(GetStats(p), p, false, p.Run != null ? p.Run.Relics : null);
                    if (holderIndex >= 0) joinerHolders.Add(holderIndex);
                }
            }
            _pendingRewards.Clear();

            LogInfo?.Invoke($"[Combat] {_currentRoomId}번 방 난입: {joined}명 합류, 남은 몬스터 {GetAliveMonsterStats().Count}");
            StartCycle();

            // 들어온 사람에겐 새 전투 -> 전투 시작 유물 발동 (첫 턴 에너지 회복 뒤, 보통 전투와 같은 순서)
            foreach (int holderIndex in joinerHolders)
            {
                FireRelics(holderIndex, RelicTrigger.OnBattleStart);
                if (_currentRoomType == RoomType.GUARDIAN) FireRelics(holderIndex, RelicTrigger.OnGuardianBattleStart);
                if (_currentRoomType == RoomType.BOSS) FireRelics(holderIndex, RelicTrigger.OnBossBattleStart);
            }
            NotifyChanged();
        }

        // =====================================================================
        // 사이클
        // =====================================================================

        void StartCycle()
        {
            ++_cycle;
            _endedTurn.Clear();
            _bBetrayerTurn = false;

            // 플레이어 보호막은 사이클이 끝나면 사라짐 (몬스터 턴까지 버티고 여기서 버림), 에너지는 최대치로
            // 배신 전투면 팀 차례부터 (배신자는 BeginBetrayerTurn 에서)
            foreach (var p in _players)
            {
                if (IsBetrayal() && p == _betrayer) continue;
                var stats = GetStats(p);
                if (stats == null || stats.IsDead()) continue;
                stats.ClearShield();
                stats.RefillEnergy();
            }

            _phase = CombatPhase.PlayerTurn;

            // 전투 시작 유물은 첫 턴 에너지 회복 "뒤" (보조 배터리: 3 -> 4)
            if (_cycle == 1)
            {
                FireRelicsForAll(RelicTrigger.OnBattleStart, true, true);
                if (_currentRoomType == RoomType.GUARDIAN) FireRelicsForAll(RelicTrigger.OnGuardianBattleStart, true, true);
                if (_currentRoomType == RoomType.BOSS) FireRelicsForAll(RelicTrigger.OnBossBattleStart, true, true);
            }

            // 플레이어 턴 시작: 중독 피해, 활력(턴 시작 에너지 +수치), 턴 시작 유물
            foreach (var p in _players)
            {
                if (IsBetrayal() && p == _betrayer) continue;
                var stats = GetStats(p);
                if (stats == null || stats.IsDead()) continue;

                stats.OnTurnStart();
                if (stats.IsDead()) continue;

                int vitality = stats.GetStatusValue(StatusEffect.Vitality);
                if (vitality > 0)
                {
                    stats.AddEnergy(vitality, true);
                }
                FireRelics(FindHolder(stats), RelicTrigger.OnTurnStart);
            }

            // 유탄 발사기처럼 시작하자마자 몬스터가 다 죽을 수도 있음
            HideDeadMonsters();
            if (CheckCombatEnd()) return;

            ChooseIntents();

            LogInfo?.Invoke($"[Combat] 사이클 {_cycle} 시작. 플레이어 턴");
            NotifyChanged();
        }

        void ChooseIntents()
        {
            _intents.Clear();

            foreach (var monster in _monsters)
            {
                var stats = monster?.Stats;
                if (stats == null || stats.IsDead()) continue;

                string picked = PickMonsterSkill(monster.RowName);
                string skillName = GameData.ResolveSkillRow(picked);   // SkillID 로 적힌 데이터도
                var skill = Ids.IsNone(skillName) ? null : GameData.FindSkillRow(skillName);
                if (skill == null)
                {
                    LogWarning?.Invoke($"[Combat] {monster.RowName} 의 스킬 '{picked}' 이 DT_Skill 에 없음 -> 이 몬스터는 이번 사이클에 아무것도 안 함");
                    continue;
                }

                _intents.Add(new MonsterIntent
                {
                    Monster = monster,
                    SkillRow = skillName,
                    Kind = DungeonMonster.ToIntentKind(skill),
                    Amount = SkillExecutor.PreviewAmount(skill, stats),
                    HitCount = Math.Max(1, skill.HitCount),
                });
            }
        }

        /// <summary>
        /// 플레이어 요청. SkillIndex = 기본 스킬 0~2 (공격 방어 특수), 강화 스킬 3~5 (장착 칸)
        /// TargetIndex = 적 1명 스킬이면 Monsters 의 인덱스, 아군 1명 스킬이면 구역 Occupants(= Players) 의 인덱스
        /// </summary>
        public void HandleUseSkill(DungeonPlayer p, int skillIndex, int targetIndex)
        {
            if (!IsTurnOf(p) || _endedTurn.Contains(p)) return;
            if (!_players.Contains(p)) return;
            if (IsResolvingHits) return;

            var caster = GetStats(p);
            if (caster == null || caster.IsDead() || p.Run == null) return;

            // 0~2 기본 스킬, 3~5 장착한 강화 스킬
            var skillPtr = p.Run.GetCombatSkill(skillIndex);
            if (skillPtr == null) return;

            // 기본 스킬은 훈련소 강화만큼 수치가 오름 (복사본에만)
            var skill = skillPtr.Clone();
            p.Run.ApplyBasicSkillUpgrade(skillIndex, skill);

            // 대상 먼저 확인 (에너지만 날리고 실패하지 않게)
            var targets = new List<CombatStats>();

            // 배신 전투: 적 = 상대 편 플레이어, 아군 = 같은 편 (대상 인덱스는 Players = 구역 Occupants 순서)
            if (IsBetrayal())
            {
                bool bMine = p == _betrayer;
                void PickPlayer(bool bWantSameSide)
                {
                    var targetPlayer = targetIndex >= 0 && targetIndex < _players.Count ? _players[targetIndex] : null;
                    var t = GetStats(targetPlayer);
                    if (t != null && !t.IsDead() && IsSameSide(p, targetPlayer) == bWantSameSide) targets.Add(t);
                }

                switch (skill.TargetType)
                {
                    case TargetType.Self: targets.Add(caster); break;
                    case TargetType.SingleEnemy: PickPlayer(false); break;
                    case TargetType.AllEnemies: targets = GetBetrayalSideStats(!bMine); break;
                    case TargetType.SingleAlly: PickPlayer(true); break;
                    case TargetType.AllAllies: targets = GetBetrayalSideStats(bMine); break;
                }
            }
            else
            {
                switch (skill.TargetType)
                {
                    case TargetType.Self:
                        targets.Add(caster);
                        break;
                    case TargetType.SingleEnemy:
                        if (targetIndex >= 0 && targetIndex < _monsters.Count && _monsters[targetIndex] != null)
                        {
                            var t = _monsters[targetIndex].Stats;
                            if (t != null && !t.IsDead()) targets.Add(t);
                        }
                        break;
                    case TargetType.AllEnemies:
                        targets = GetAliveMonsterStats();
                        break;
                    case TargetType.SingleAlly:
                        if (targetIndex >= 0 && targetIndex < _players.Count)
                        {
                            var t = GetStats(_players[targetIndex]);
                            if (t != null && !t.IsDead()) targets.Add(t);
                        }
                        break;
                    case TargetType.AllAllies:
                        targets = GetAlivePlayerStats();
                        break;
                }
            }

            if (targets.Count == 0)
            {
                LogInfo?.Invoke($"[Combat] {p.Name}: 대상이 없어 '{skill.DisplayName_KR}' 취소");
                return;
            }

            // 비용은 둘 다 되는지 먼저 보고 나서 씀 (한쪽만 빠지지 않게)
            var casterState = caster.State;
            int energyCost = caster.GetEffectiveEnergyCost(skill.EnergyCost);   // 과욕(산성) +1
            if (casterState.Energy < energyCost || casterState.SkillEnergy < skill.SkillEnergyCost)
            {
                LogInfo?.Invoke($"[Combat] {p.Name}: 에너지 부족 ({skill.DisplayName_KR})");
                return;
            }
            caster.SpendEnergy(energyCost);
            caster.SpendSkillEnergy(skill.SkillEnergyCost);

            // 광역 추가 피해(마도 캐논 등)가 맞을 상대 편 전체
            var opponents = IsBetrayal() ? GetBetrayalSideStats(p != _betrayer) : GetAliveMonsterStats();
            var row = p.Run.GetCombatSkillRow(skillIndex);
            LogInfo?.Invoke($"[Combat] {p.Name} -> {skill.DisplayName_KR}");

            // 타격마다: 이펙트 알림 -> 피해 -> 죽음 / 끝 확인. 스킬 사용 유물은 마지막 타격 뒤
            var plan = SkillExecutor.Plan(skill, caster, targets, opponents, _rng);
            var steps = new List<Action>();
            for (int i = 0; i < plan.Count; ++i)
            {
                var hit = plan[i];
                bool last = i == plan.Count - 1;
                steps.Add(() =>
                {
                    RaiseSkillUsed(row, targets.FindAll(t => !t.IsDead()));
                    hit();
                    if (last) FireRelics(FindHolder(caster), RelicTrigger.OnSkillUsed, 0, row, skill);
                    HideDeadMonsters();
                    CheckCombatEnd();
                    NotifyChanged();
                });
            }
            Resolve(steps, row);
        }

        void RaiseSkillUsed(string skillRow, List<CombatStats> targets)
        {
            if (SkillUsed == null) return;
            ToIds(targets, out var monsterIds, out var playerIds);
            SkillUsed(skillRow, monsterIds, playerIds);
        }

        void HandleCountered(CombatStats self, CombatStats victim)
        {
            if (CounterHit == null) return;
            ToIds(new List<CombatStats> { victim }, out var monsterIds, out var playerIds);
            CounterHit(monsterIds, playerIds);
        }

        void ToIds(List<CombatStats> targets, out List<int> monsterIds, out List<ulong> playerIds)
        {
            monsterIds = new List<int>();
            playerIds = new List<ulong>();
            foreach (var t in targets)
            {
                var m = _monsters.Find(x => x != null && x.Stats == t);
                if (m != null) { monsterIds.Add(m.Id); continue; }
                var pl = _players.Find(x => GetStats(x) == t);
                if (pl != null) playerIds.Add(pl.Id);
            }
        }

        public void HandleEndTurn(DungeonPlayer p)
        {
            if (!IsTurnOf(p) || !_players.Contains(p) || _endedTurn.Contains(p)) return;
            if (IsResolvingHits) return;

            _endedTurn.Add(p);

            // 자기 턴 끝: 상태이상 남은 사이클 감소, 남은 에너지 버림
            var stats = GetStats(p);
            if (stats != null)
            {
                stats.OnTurnEnd();
                stats.DrainEnergy();
                FireRelics(FindHolder(stats), RelicTrigger.OnTurnEnd);
            }
            NotifyChanged();

            if (CheckCombatEnd()) return;

            if (IsBetrayal())
            {
                TryAdvanceBetrayalTurn();
                return;
            }

            // 살아 있는 사람이 전원 턴 종료했으면 몬스터 턴
            foreach (var other in _players)
            {
                var otherStats = GetStats(other);
                if (otherStats != null && !otherStats.IsDead() && !_endedTurn.Contains(other)) return;
            }

            BeginMonsterTurn();
        }

        /// <summary>[테스트] 몬스터 전부 처치 -> 승리 (PC 의 DebugWinCombat)</summary>
        public void DebugKillAllMonsters()
        {
            if (!IsInCombat() || _phase == CombatPhase.Victory || _phase == CombatPhase.Defeat) return;

            foreach (var stats in GetAliveMonsterStats())
            {
                // 테스트용이라 몬스터 유물 / 상태를 무시하고 확실히 죽임
                // (왕의 위엄 = 피해 1 고정, 고대의 슬라임 코어 = 부활 -> 보스방에서 안 죽던 원인)
                stats.PreventDeath = null;
                stats.ClearCombatEffects();
                stats.ApplyDamage(stats.State.Health + stats.State.Shield);

                if (!stats.IsDead())
                {
                    LogWarning?.Invoke($"[Debug] {stats.DebugName} 가 안 죽음 (체력 {stats.State.Health})");
                }
            }

            HideDeadMonsters();
            CheckCombatEnd();
            NotifyChanged();
        }

        /// <summary>
        /// [테스트] 이 플레이어가 장착할 수 있는 스킬을 DT_Skill 순서대로 돌려 강화 스킬 칸(4번 칸)에 넣고 에너지를 채움.
        /// step 1 = 다음, -1 = 이전. 바꾼 스킬 행 이름 (없으면 "")
        /// </summary>
        public string DebugCycleSkill(DungeonPlayer p, int step)
        {
            if (!IsInCombat() || p?.Run == null || !_players.Contains(p)) return "";

            var rows = new List<string>();
            foreach (var row in GameData.Skills.RowNames)
                if (Terminus.Run.ProfileService.IsEquippableSkill(row, p.Run.CharacterClass)) rows.Add(row);
            if (rows.Count == 0) return "";

            var current = p.Run.GetCombatSkillRow(3);
            int i = rows.IndexOf(current);
            int next = i < 0 ? 0 : ((i + step) % rows.Count + rows.Count) % rows.Count;
            p.Run.DebugSetEnhanceSkill(rows[next]);
            GetStats(p)?.DebugRefillAll();
            LogInfo?.Invoke($"[Debug] {p.Name} 강화 스킬 -> {rows[next]} ({next + 1}/{rows.Count})");
            NotifyChanged();
            return rows[next];
        }

        /// <summary>[테스트] 이 구역 플레이어 전부 쓰러뜨림 -> 패배 (PC 의 DebugLoseCombat)</summary>
        public void DebugKillAllPlayers()
        {
            if (!IsInCombat() || _phase == CombatPhase.Victory || _phase == CombatPhase.Defeat) return;

            foreach (var stats in GetAlivePlayerStats())
            {
                // 부활 / 피해 감소 유물을 무시하고 확실히 쓰러뜨림
                stats.PreventDeath = null;
                stats.ClearCombatEffects();
                stats.ApplyDamage(stats.State.Health + stats.State.Shield);
            }

            CheckCombatEnd();
            NotifyChanged();
        }

        void BeginMonsterTurn()
        {
            _phase = CombatPhase.MonsterTurn;
            _nextMonsterIndex = 0;

            LogInfo?.Invoke($"[Combat] 사이클 {_cycle} 몬스터 턴");
            NotifyChanged();

            SetTimer(CombatStep.MonsterAction, _settings.MonsterActionInterval);
        }

        void RunNextMonsterAction()
        {
            if (_phase != CombatPhase.MonsterTurn) return;

            // 다음으로 살아 있는 몬스터
            while (_nextMonsterIndex < _monsters.Count)
            {
                var monster = _monsters[_nextMonsterIndex++];
                var stats = monster?.Stats;
                if (stats == null || stats.IsDead()) continue;

                // 몬스터 보호막은 "다음 자기 턴이 돌아올 때까지" -> 행동 직전에 버림
                stats.ClearShield();
                stats.OnTurnStart();   // 중독
                HideDeadMonsters();
                if (CheckCombatEnd()) return;
                if (stats.IsDead())
                {
                    // 중독으로 죽었으면 행동 없이 다음 몬스터 (한 마리씩 간격)
                    SetTimer(CombatStep.MonsterAction, _settings.MonsterActionInterval);
                    return;
                }
                FireRelics(FindHolder(stats), RelicTrigger.OnTurnStart);
                if (stats.IsDead()) continue;

                var intent = FindIntent(monster);
                var skill = intent != null ? GameData.FindSkillRow(intent.SkillRow) : null;
                if (skill == null)
                {
                    FinishMonsterAction(stats);
                    return;
                }

                var targets = ResolveTargets(skill.TargetType, stats, GetAlivePlayerStats(), GetAliveMonsterStats());
                MonsterActing?.Invoke(monster, intent.Kind);
                LogInfo?.Invoke($"[Combat]   몬스터 {_nextMonsterIndex - 1} -> {skill.DisplayName_KR}");

                // 타격마다 이펙트 알림 -> 피해. 턴 마무리는 마지막 타격 뒤
                var plan = SkillExecutor.Plan(skill, stats, targets, GetAlivePlayerStats(), _rng);
                var steps = new List<Action>();
                for (int i = 0; i < plan.Count; ++i)
                {
                    var hit = plan[i];
                    bool last = i == plan.Count - 1;
                    steps.Add(() =>
                    {
                        RaiseSkillUsed(intent.SkillRow, targets.FindAll(t => !t.IsDead()));
                        hit();
                        if (last) FinishMonsterAction(stats);
                        else HideDeadMonsters();
                    });
                }
                Resolve(steps, intent.SkillRow);
                return;
            }

            FinishCycle();
        }

        // 몬스터 한 마리 행동 끝 (마지막 타격 뒤)
        void FinishMonsterAction(CombatStats stats)
        {
            // 이 몬스터의 자기 턴 끝: 상태이상 남은 사이클 감소 (이번 행동에서 자기에게 건 강철 등은 다음 자기 턴까지 남음)
            if (!stats.IsDead())
            {
                stats.OnTurnEnd();
                FireRelics(FindHolder(stats), RelicTrigger.OnTurnEnd);
            }

            HideDeadMonsters();
            if (CheckCombatEnd()) return;

            // 한 마리씩 간격을 두고
            SetTimer(CombatStep.MonsterAction, _settings.MonsterActionInterval);
        }

        void FinishCycle()
        {
            // 상태이상 시간은 각자 자기 턴 끝에 줄어듦 (HandleEndTurn / RunNextMonsterAction). 여기선 다음 사이클로만
            HideDeadMonsters();
            if (CheckCombatEnd()) return;

            StartCycle();
        }

        // =====================================================================
        // 승패
        // =====================================================================

        void HideDeadMonsters()
        {
            foreach (var monster in _monsters)
            {
                var stats = monster?.Stats;
                if (stats != null && stats.IsDead() && !monster.bHideRequested)
                {
                    monster.bHideRequested = true;
                    MonsterHideRequested?.Invoke(monster);   // 사망 플립북이 끝나면 숨김 (복제됨)
                }
            }
        }

        // 승패 판정. 끝났으면 true (다음 진행 멈춤)
        bool CheckCombatEnd()
        {
            if (_phase == CombatPhase.Victory || _phase == CombatPhase.Defeat || _phase == CombatPhase.None) return true;

            // 배신 전투: 배신자가 쓰러지면 팀 승리, 팀이 전부 쓰러지면 배신자 승리 (몬스터는 없음)
            if (IsBetrayal())
            {
                if (GetBetrayalSideStats(true).Count == 0)
                {
                    FinishBetrayal(false);
                    return true;
                }
                if (GetBetrayalSideStats(false).Count == 0)
                {
                    FinishBetrayal(true);
                    return true;
                }
                return false;
            }

            if (GetAliveMonsterStats().Count == 0)
            {
                FinishCombat(true);
                return true;
            }

            if (GetAlivePlayerStats().Count == 0)
            {
                FinishCombat(false);
                return true;
            }

            return false;
        }

        void FinishCombat(bool bVictory)
        {
            _phase = bVictory ? CombatPhase.Victory : CombatPhase.Defeat;
            _intents.Clear();

            ClearTimer();

            LogInfo?.Invoke($"[Combat] 전투 {(bVictory ? "승리" : "패배")} (사이클 {_cycle})");

            // 퀘스트 '몬스터 처치': 이긴 방의 몬스터는 그 방 사람 모두에게 (쓰러졌다 살아나는 사람 포함)
            if (bVictory && !IsBetrayal())
            {
                var rows = _monsters.ConvertAll(m => m.RowName);
                foreach (var p in _players)
                {
                    if (IsValid(p) && p.Run != null) Quests.OnMonstersKilled(p.Run, rows);
                }
            }

            // 살아 있는 플레이어의 전투 종료 유물 (재화 / 회복 / 최대 체력). 그다음 전투에서만 의미 있는 것들을 지움
            FireRelicsForAll(RelicTrigger.OnBattleEnd, true, false);
            foreach (var p in _players)
            {
                GetStats(p)?.ClearCombatEffects();

                // 이벤트 일시 버프는 전투 한 번이 끝날 때마다 1 씩
                if (IsValid(p)) p.Run?.ConsumeTempBuffBattle();
            }

            // 기획 사망 순서도
            //  - 이김: 쓰러졌던 사람은 체력 1 로 부활
            //    반드시 위의 전투 종료 유물(OnBattleEnd) 발동 '뒤'에 살릴 것: 쓰러져 있던 사람(난입으로 살아나는 사람 포함)은
            //    전투 종료 유물이 발동하면 안 됨 (사용자 결정 10-08). FireRelics 는 죽은 사람을 건너뜀
            //  - 이 구역 전멸: 잠시 뒤 구역에 알림 -> 다른 방 동료가 구출 / 난입, 아무도 없으면 런 끝 (구역 서브시스템이 판단)
            if (bVictory)
            {
                foreach (var p in _players)
                {
                    var stats = GetStats(p);
                    if (stats != null && stats.IsDead())
                    {
                        stats.Revive(0f);   // 최소 1
                        LogInfo?.Invoke($"[Combat] {p.Name} 체력 1 로 부활");
                    }
                }
            }
            else
            {
                NotifyChanged();
                SetTimer(CombatStep.ReportWipe, _settings.CombatEndDelay);
                return;
            }

            NotifyChanged();

            // 몬스터 / 가디언 / 보스방 승리: 잠시 뒤 각자 보상 화면 -> 전원 '다음으로' 를 누르면 구역 클리어
            bool bRewardRoom = _currentRoomType == RoomType.MONSTER || _currentRoomType == RoomType.GUARDIAN || _currentRoomType == RoomType.BOSS;
            if (bVictory && bRewardRoom && _players.Count > 0)
            {
                SetTimer(CombatStep.StartRoomRewards, _settings.RewardDelay);
                return;
            }

            // 그 외: 잠시 결과를 보여 준 뒤 구역 클리어 -> 모든 구역이 끝나면 지도로
            SetTimer(CombatStep.ClearArea, _settings.CombatEndDelay);
        }

        void ReportWipe()
        {
            AreaWiped?.Invoke();
        }

        void ClearArea()
        {
            AreaCleared?.Invoke();
        }
    }
}
