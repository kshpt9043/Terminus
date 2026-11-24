// 플레이어 한 명의 런 규칙 (언리얼 Player/TerminusPlayerState 의 규칙 부분 이식)
// 언리얼 APlayerState 의 복제 / HasAuthority / CopyProperties / 구역(ADungeonArea) 은 안 옮김
//  -> 유니티 쪽 NetworkBehaviour 가 이 객체를 서버에서만 고치고, RunStateChanged 를 받아 NetworkVariable 로 보냄
// 배틀러(전투 스텟 컴포넌트)를 같이 고쳐야 하는 곳은 IRunBattler 로 알림 (없으면 런 스텟만 바뀜)
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Run
{
    /// <summary>
    /// 지금 살아 있는 배틀러 (언리얼 ATerminusBattler + UCombatStatsComponent). 유니티 쪽이 구현. 폰이 없으면 null
    /// </summary>
    public interface IRunBattler
    {
        // 최대 체력을 바꿈 (언리얼 ModifyMaxHealth)
        void ModifyMaxHealth(int delta);
        // 체력 회복 (언리얼 Heal)
        void Heal(int amount);
        // 공격 / 방어를 바꿈. 전투 밖이라 다른 상태는 없음 -> 스텟을 다시 넣고 체력 비율을 되돌리면 됨
        // (언리얼: InitFrom(NewStats) 후 Revive(Health / MaxHealth))
        void ModifyAttackDefense(int attackDelta, int defenseDelta);
        // 체력 전부 회복 (언리얼 Revive(1))
        void ReviveFull();
    }

    public sealed class RunPlayer
    {
        // 던전에 들고 갈 수 있는 창고 유물 수 (기획: 창고 유물 중 1개, 안 골라도 됨)
        public const int MaxStartRelics = 1;

        // 인게임 유물 칸 (패시브 + 시작 유물 포함). 기본 6, 연무장 '유물 최대치' 강화로 최대 15 (중간 수치 미정)
        public const int BaseRelicCapacity = Data.RelicCapacity.Base;
        public const int MaxRelicCapacity = Data.RelicCapacity.Max;

        // 전투에서 쓰는 스킬 칸: 0~2 = 기본 스킬(공격 방어 특수), 3~ = 장착한 강화 스킬
        public const int NumBasicSkills = 3;
        public const int NumEnhanceSkills = 5;   // 기본 3 + 연무장 '픽업 스킬 칸' 강화 2

        // [픽업 스킬 강화] 최대 단계. 비용 50 / 100 / 150 / 200 / 250 (11-04 피드백) 이라 5단계
        public const int MaxPickupSkillLevel = 5;

        readonly Random _rng;
        RunState _runState = new RunState();

        // 거점 강화 (모든 직업). 서버만, 복제 안 함. 손님은 자기 PC 의 Server_ReportUpgrades 로 알려 줌
        List<ClassUpgrades> _reportedUpgrades = new List<ClassUpgrades>();

        public RunPlayer(Random rng, string playerName = "")
        {
            _rng = rng ?? new Random();
            PlayerName = playerName ?? "";
        }

        // 로그 / 알림용 이름
        public string PlayerName { get; set; }

        // 지금 배틀러 (유니티 쪽이 폰이 생기고 없어질 때 넣고 뺌)
        public IRunBattler Battler { get; set; }

        // 런 상태가 바뀜 (언리얼 OnRep_RunState / OnRunStateChanged). 받은 쪽은 복제 / 화면 갱신
        public event Action<RunState> RunStateChanged;

        // 이 사람에게만 보낼 알림 (퀘스트 진행 등. 언리얼: PC->Client_ReceiveChat 시스템 메시지)
        public event Action<string> Notice;

        // 참조로 돌려줌 (UI 가 매 틱 불러도 배열째 복사하지 않게). 고쳐 쓸 땐 ModifyRunState 또는 복사해서 SetRunState
        public RunState RunState => _runState;

        public CharacterClass CharacterClass => _runState.CharacterClass;

        // 준비 완료 여부. RunState 에는 안 들어감 (처음 들어갈 때만 중요)
        public bool bReady { get; set; }

        // 이어하기 주점에서 세이브 자리에 앉아 직업이 고정됐는가. 주점 화면이 직업 버튼을 잠글 때 씀
        public bool bClassLocked { get; private set; }

        void Changed() => RunStateChanged?.Invoke(_runState);

        internal void Tell(string text)
        {
            Notice?.Invoke(text);
            RunLog.Log($"[Quest] {PlayerName}: {text}");
        }

        // ============================================================ 기본

        public void SetCharacterClass(CharacterClass c)
        {
            _runState.CharacterClass = c;
        }

        // 이어하기 주점에서 세이브 당시 직업으로 고정. 고정되면 직업 변경 요청을 무시 (무시는 부른 쪽이 bClassLocked 로)
        public void LockClassFromSave(CharacterClass c)
        {
            _runState.CharacterClass = c;
            bClassLocked = true;
            Changed();
        }

        // 세이브에서 불러온 체력을 꺼내고 비움. 없으면 -1
        public int ConsumeSavedHealth()
        {
            if (_runState.SavedHealth < 0) return -1;

            int health = _runState.SavedHealth;
            _runState.SavedHealth = -1;
            return health;
        }

        public void SetReportedUpgrades(IEnumerable<ClassUpgrades> upgrades)
        {
            _reportedUpgrades = new List<ClassUpgrades>();
            if (upgrades == null) return;
            foreach (var u in upgrades) _reportedUpgrades.Add(u.Clone());
        }

        /// <summary>
        /// 손님이 보낸 강화 목록을 한도 안으로 잘라서 넣음 (언리얼 Server_ReportUpgrades). 16개 넘게 보내면 전부 버림
        /// </summary>
        public void SetReportedUpgradesFromClient(IList<ClassUpgrades> upgrades, UpgradeSettings settings = null)
        {
            var s = settings ?? GameData.Upgrades;
            var clean = new List<ClassUpgrades>();
            if (upgrades != null && upgrades.Count <= 16)
            {
                foreach (var entry in upgrades) clean.Add(s.Sanitize(entry));
            }
            _reportedUpgrades = clean;
        }

        /// <summary>
        /// 주점에서 던전으로 출발할 때 서버가 부름. 클래스 기본 스텟을 런 스텟으로 복사하고 진행도 초기화
        /// </summary>
        public void BeginRun(UpgradeSettings settings = null)
        {
            // 강화 전 기본 스텟에서 시작. 던전에서 늘어나는 값은 이후 여기(RunState.Stats)에 쌓임
            var row = GameData.FindCharacterClassRow(_runState.CharacterClass);
            if (row != null)
            {
                _runState.Stats = row.BaseStats.Clone();
                _runState.bStatsInitialized = true;
            }

            // 거점 강화 (이 직업 것): 연무장 -> 런 스텟, 훈련소 -> 기본 스킬 단계
            _runState.BasicSkillLevels.Clear();
            _runState.PickupSkillLevels.Clear();
            var myClass = _runState.CharacterClass;
            var upgrades = _reportedUpgrades.Find(u => u.Class == myClass);
            if (upgrades != null)
            {
                if (_runState.bStatsInitialized)
                {
                    (settings ?? GameData.Upgrades).ApplyToStats(upgrades, _runState.Stats);
                }
                _runState.BasicSkillLevels = new List<int>(upgrades.SkillLevels);
            }
            else
            {
                RunLog.Warn($"PS: {myClass} 행이 없어 런 스텟을 못 채움");
            }

            _runState.CurrentRoomId = -1;
            _runState.CurrentMapLevel = 0;
            _runState.SelectedRoomId = -1;
            _runState.VisitedRoomIds.Clear();
            _runState.EnhanceSkills.Clear();
            _runState.bStartSkillChosen = false;
            _runState.bStartRelicsChosen = false;
            _runState.Currency = 0;
            _runState.SavedHealth = -1;
            _runState.TempBuffs.Clear();
            _runState.TempRelics.Clear();
            _runState.ActiveQuest = new QuestProgress();
            _runState.FloorMonsterRooms = 0;
            _runState.FloorGuardianRooms = 0;
            _runState.QuestGold = 0;
            _runState.SeenRelics.Clear();
            _runState.StartRelics.Clear();
            _runState.BossRelic = "";
            _runState.BossSkill = "";

            // 직업 기본 유물 = 패시브. 데이터에 아직 없는 직업은 빈 채로
            _runState.Relics.Clear();
            var basicRelic = GetBasicRelicRow(_runState.CharacterClass);
            if (!Ids.IsNone(basicRelic))
            {
                _runState.Relics.Add(basicRelic);
            }

            Changed();
        }

        // 런 시작 강화 스킬 장착. "" 이면 고를 게 없어 건너뜀. 한 런에 한 번만
        public void ChooseStartSkill(string skillRow)
        {
            if (_runState.bStartSkillChosen) return;

            // 보유 여부는 각자 컴퓨터의 세이브라 서버가 확인할 수 없음 -> 장착 가능한 스킬인지만 봄
            if (!Ids.IsNone(skillRow))
            {
                if (ProfileService.IsEquippableSkill(skillRow, _runState.CharacterClass))
                {
                    _runState.EnhanceSkills.Clear();
                    _runState.EnhanceSkills.Add(skillRow);
                }
                else
                {
                    RunLog.Warn($"PS: {PlayerName} 가 장착할 수 없는 '{skillRow}' 를 골라 장착 없이 넘어감");
                }
            }

            _runState.bStartSkillChosen = true;
            Changed();
        }

        public bool HasChosenStartSkill => _runState.bStartSkillChosen;

        // [테스트] 강화 스킬 칸 첫 자리를 이 스킬로 (스킬 이펙트 시험)
        public void DebugSetEnhanceSkill(string skillRow)
        {
            _runState.EnhanceSkills.Clear();
            _runState.EnhanceSkills.Add(skillRow);
            Changed();
        }

        // [테스트] 시작 강화 스킬 고르기를 안 한 상태로 되돌림
        public void ResetStartSkill()
        {
            _runState.EnhanceSkills.Clear();
            _runState.bStartSkillChosen = false;
            Changed();
        }

        // 런 시작 때 창고에서 고른 유물 장착 (최대 MaxStartRelics 개, 비어도 됨). 한 런에 한 번
        public void ChooseStartRelics(IEnumerable<string> relicRows)
        {
            if (_runState.bStartRelicsChosen) return;

            // 보유 여부는 각자 컴퓨터의 창고라 서버가 확인할 수 없음 -> 보관 가능한 유물인지 / 이 직업이 가질 수 있는지만 봄
            int taken = 0;
            if (relicRows != null)
            {
                foreach (var row in relicRows)
                {
                    if (taken >= MaxStartRelics) break;
                    if (ProfileService.IsStorableRelic(row) && GainRelic(row))
                    {
                        if (!_runState.StartRelics.Contains(row)) _runState.StartRelics.Add(row);
                        ++taken;
                    }
                }
            }

            RunLog.Log($"[Relic] {PlayerName} 시작 유물 {taken}개 장착");

            _runState.bStartRelicsChosen = true;
            Changed();
        }

        public bool HasChosenStartRelics => _runState.bStartRelicsChosen;

        // 하드 모드 (방장이 정함. 주점이 파티 전원에게 같은 값을 넣음)
        public void SetHardMode(bool hardMode)
        {
            if (_runState.bHardMode == hardMode) return;

            _runState.bHardMode = hardMode;
            Changed();
        }

        public bool IsHardMode => _runState.bHardMode;

        // [테스트] 유물 고르기를 안 한 상태로 (이미 장착한 유물은 그대로)
        public void ResetStartRelics()
        {
            _runState.bStartRelicsChosen = false;
            Changed();
        }

        // ============================================================ 스킬

        // 전투 스킬 칸의 행 이름 (DT_Skill). 비어 있으면 ""
        public string GetCombatSkillRow(int slotIndex)
        {
            if (slotIndex >= 0 && slotIndex < NumBasicSkills)
            {
                // 기본 스킬은 직업으로 찾은 행 (FindBasicSkills 와 같은 순서: 공격 방어 특수)
                var basic = GameData.FindBasicSkills(_runState.CharacterClass);
                return slotIndex < basic.Count ? basic[slotIndex].RowName : "";
            }

            int enhanceIndex = slotIndex - NumBasicSkills;
            if (enhanceIndex >= 0 && enhanceIndex < NumEnhanceSkills && enhanceIndex < _runState.EnhanceSkills.Count)
            {
                return _runState.EnhanceSkills[enhanceIndex];
            }
            return "";
        }

        // 그 칸의 스킬. 비어 있으면 null (표의 원본 -> 고치려면 Clone)
        public SkillRow GetCombatSkill(int slotIndex)
        {
            var row = GetCombatSkillRow(slotIndex);
            return Ids.IsNone(row) ? null : GameData.FindSkillRow(row);
        }

        // 강화 스킬 칸 수 (캐릭터 EnhanceSlots, 최대 NumEnhanceSkills)
        public int GetEnhanceSlotCount()
        {
            // 런 스텟이 아직 없으면(던전 맵 바로 PIE) 기본 3칸
            // (언리얼 주석은 3칸이지만 코드는 NumEnhanceSkills = 5 를 씀. 그대로 옮김)
            int slots = _runState.bStatsInitialized ? _runState.Stats.EnhanceSlots : NumEnhanceSkills;
            return Math.Max(0, Math.Min(slots, NumEnhanceSkills));
        }

        /// <summary>
        /// 스킬 강화 단계를 더함. inOutSkill 을 직접 고치니 반드시 표 행의 Clone 을 넘길 것.
        /// 기본 스킬(0~2 칸)은 훈련소 강화(BaseValue 가 있으면 BaseValue, 없으면 StatusValue).
        /// 강화 스킬 칸(3~)은 픽업 스킬 강화 (상점): 공격 / 방어 스킬만 단계당 BaseValue +1 (특수는 강화 없음)
        /// </summary>
        public void ApplyBasicSkillUpgrade(int slotIndex, SkillRow inOutSkill)
        {
            if (slotIndex < 0 || inOutSkill == null) return;

            // 0~2 기본 스킬 = 훈련소 단계, 3~ 강화 스킬 칸 = 상점 픽업 스킬 단계
            bool basic = slotIndex < NumBasicSkills;
            int level = basic
                ? (slotIndex < _runState.BasicSkillLevels.Count ? _runState.BasicSkillLevels[slotIndex] : 0)
                : GetPickupSkillLevel(GetCombatSkillRow(slotIndex));
            if (level <= 0) return;

            // 픽업 스킬: 공격 / 방어만 BaseValue +단계 (특수는 강화 없음)
            if (!basic)
            {
                if (IsUpgradableSkill(inOutSkill)) inOutSkill.BaseValue += level;
                return;
            }

            if (inOutSkill.BaseValue != 0)
            {
                inOutSkill.BaseValue += level;
            }
            else
            {
                inOutSkill.StatusValue += level;
            }
        }

        public int GetPickupSkillLevel(string skillRow)
        {
            var entry = _runState.PickupSkillLevels.Find(e => e.Skill == skillRow);
            return entry != null ? entry.Level : 0;
        }

        // 픽업 스킬 강화 대상인가: 공격 / 방어만 (특수는 강화가 안 뜸)
        public static bool IsUpgradableSkill(SkillRow skill)
        {
            return skill != null && (skill.SkillType == SkillType.Attack || skill.SkillType == SkillType.Defense);
        }

        // 지금 한 단계 더 올릴 수 있나 (공격 / 방어 + 최대 단계 아님)
        public bool CanUpgradePickupSkill(string skillRow)
        {
            var skill = Ids.IsNone(skillRow) ? null : GameData.FindSkillRow(skillRow);
            return skill != null && IsUpgradableSkill(skill) && GetPickupSkillLevel(skillRow) < MaxPickupSkillLevel;
        }

        // 한 단계 올림. 최대면 false
        public bool UpgradePickupSkill(string skillRow)
        {
            if (!CanUpgradePickupSkill(skillRow)) return false;

            var entry = _runState.PickupSkillLevels.Find(e => e.Skill == skillRow);
            if (entry == null)
            {
                entry = new SkillLevelEntry { Skill = skillRow };
                _runState.PickupSkillLevels.Add(entry);
            }
            if (entry.Level >= MaxPickupSkillLevel) return false;

            ++entry.Level;
            Changed();
            return true;
        }

        // 강화 스킬 장착. 빈 칸이 있으면 거기에, 꽉 찼으면 replaceIndex 칸을 바꿈. 못 넣으면 false
        public bool EquipEnhanceSkill(string skillRow, int replaceIndex = -1)
        {
            if (GameData.FindSkillRow(skillRow) == null) return false;

            if (_runState.EnhanceSkills.Count < GetEnhanceSlotCount())
            {
                _runState.EnhanceSkills.Add(skillRow);
            }
            else if (replaceIndex >= 0 && replaceIndex < _runState.EnhanceSkills.Count)
            {
                RunLog.Log($"[Skill] {PlayerName}: 강화 칸 {replaceIndex} {_runState.EnhanceSkills[replaceIndex]} -> {skillRow}");
                _runState.EnhanceSkills[replaceIndex] = skillRow;
            }
            else
            {
                return false;
            }

            Changed();
            return true;
        }

        // 장착한 강화 스킬 하나를 뺌 (이벤트 대가 '스킬 소실'). 칸은 앞으로 당겨짐
        public bool UnequipEnhanceSkill(string skillRow)
        {
            if (!_runState.EnhanceSkills.Remove(skillRow)) return false;

            RunLog.Log($"[Skill] {PlayerName}: 강화 스킬 {skillRow} 잃음");
            Changed();
            return true;
        }

        // ============================================================ 유물

        // 직업 기본 유물 행 이름 (RLC_Fighter_001 등). 데이터에 없으면 ""
        public static string GetBasicRelicRow(CharacterClass c)
        {
            var row = $"RLC_{c}_001";
            return GameData.FindRelicRow(row) != null ? row : "";
        }

        // 이 직업이 가질 수 있는 유물인가 (공용이거나 그 직업 전용)
        public static bool CanClassHoldRelic(CharacterClass c, RelicRow relic)
        {
            if (relic.OwnerClass == SkillOwner.Shared) return true;
            return relic.OwnerClass.ToString() == c.ToString();
        }

        // 지금 가질 수 있는 유물 수 (런 스텟 MaxRelics, 없으면 기본 6)
        public int GetRelicCapacity()
        {
            int fromStats = _runState.Stats.MaxRelics > 0 ? _runState.Stats.MaxRelics : BaseRelicCapacity;
            return Math.Max(1, Math.Min(fromStats, MaxRelicCapacity));
        }

        public IReadOnlyList<string> Relics => _runState.Relics;

        // 이번 런에서 나온 적 있는 유물인가 (있으면 후보로 다시 안 나옴). 얻은 유물은 GainRelic 이 알아서 넣음
        public bool HasSeenRelic(string relicRow) => _runState.SeenRelics.Contains(relicRow);

        static bool IsBasicOf(string owned, SkillOwner owner)
        {
            var other = GameData.FindRelicRow(owned);
            return other != null && other.RelicTier == RelicTier.Basic && other.OwnerClass == owner;
        }

        /// <summary>
        /// 유물 얻기. 못 가지는 유물(다른 직업 / 몬스터 / 이미 있음 / 칸 가득)이면 false.
        /// Upgrade 등급이면 같은 직업의 Basic 유물을 뺌. OnGainRelic 효과(재화 / 최대 체력 등)는 여기서 바로 적용
        /// </summary>
        public bool GainRelic(string relicRow)
        {
            var relic = GameData.FindRelicRow(relicRow);
            if (relic == null)
            {
                RunLog.Warn($"[Relic] '{relicRow}' 는 DT_Relic 에 없음");
                return false;
            }

            // 가질 수 있는 유물인가: 공용이거나 내 직업 것. 이벤트 / 퀘스트 유물은 그쪽이 직접 주는 거라 누구나
            bool givenOnly = relic.OwnerClass == SkillOwner.Event || relic.OwnerClass == SkillOwner.Quest;
            if (!givenOnly && !CanClassHoldRelic(_runState.CharacterClass, relic))
            {
                RunLog.Warn($"[Relic] {relicRow} 는 다른 직업 / 몬스터 전용이라 가질 수 없음");
                return false;
            }

            if (_runState.Relics.Contains(relicRow)) return false;

            // 칸 수 확인. 업그레이드 유물은 기본 유물(패시브)을 대체하므로 꽉 차 있어도 됨
            bool replacesBasic = relic.RelicTier == RelicTier.Upgrade && _runState.Relics.Exists(owned => IsBasicOf(owned, relic.OwnerClass));
            if (!replacesBasic && _runState.Relics.Count >= GetRelicCapacity())
            {
                RunLog.Warn($"[Relic] 유물 칸이 가득 참 ({_runState.Relics.Count} / {GetRelicCapacity()})");
                return false;
            }

            // 업그레이드: 베이스가 되는 같은 직업의 Basic 유물은 사라짐 (노션 유물 페이지)
            if (relic.RelicTier == RelicTier.Upgrade)
            {
                _runState.Relics.RemoveAll(owned => IsBasicOf(owned, relic.OwnerClass));
            }

            _runState.Relics.Add(relicRow);
            if (!_runState.SeenRelics.Contains(relicRow)) _runState.SeenRelics.Add(relicRow);
            RunLog.Log($"[Relic] {PlayerName} 획득: {relic.RelicName}");

            // 일시적 이벤트 유물: 전투 RemainBattle 번이 끝나면 사라짐
            if (relic.RemainBattle > 0)
            {
                _runState.TempRelics.RemoveAll(e => e.Relic == relicRow);
                _runState.TempRelics.Add(new TempRelicEntry { Relic = relicRow, BattlesLeft = relic.RemainBattle });
            }

            // 얻는 순간 효과 (팔아도 유지되는 것들)
            if (relic.TriggerTiming == RelicTrigger.OnGainRelic)
            {
                ApplyRelicMetaEffect(relic);
            }

            Changed();
            Quests.OnRelicGained(this);
            return true;
        }

        // 후보로 보여준 유물 / 판 유물을 '나온 유물' 로
        public void MarkRelicsSeen(IEnumerable<string> relicRows)
        {
            bool changed = false;
            foreach (var row in relicRows)
            {
                if (!Ids.IsNone(row) && !_runState.SeenRelics.Contains(row))
                {
                    _runState.SeenRelics.Add(row);
                    changed = true;
                }
            }
            if (changed) Changed();
        }

        public bool RemoveRelic(string relicRow)
        {
            // 언리얼 TArray::Remove = 같은 값 전부 제거
            if (_runState.Relics.RemoveAll(r => r == relicRow) == 0) return false;

            _runState.TempRelics.RemoveAll(e => e.Relic == relicRow);
            Changed();
            return true;
        }

        // [이벤트] 유물 하나를 다른 유물로 (같은 자리). 새 유물을 못 얻으면 원래대로 두고 false
        public bool SwapRelic(string oldRelic, string newRelic)
        {
            int index = _runState.Relics.IndexOf(oldRelic);
            if (index < 0 || oldRelic == newRelic) return false;

            _runState.Relics.RemoveAt(index);
            if (!GainRelic(newRelic))
            {
                _runState.Relics.Insert(index, oldRelic);
                Changed();
                return false;
            }

            _runState.TempRelics.RemoveAll(e => e.Relic == oldRelic);

            // GainRelic 은 맨 뒤에 붙이니 원래 자리로
            _runState.Relics.RemoveAll(r => r == newRelic);
            _runState.Relics.Insert(Math.Min(index, _runState.Relics.Count), newRelic);
            Changed();
            return true;
        }

        // [이벤트] 이번 런 동안 스텟 올리기. statKind 0 = 최대 체력, 1 = 공격, 2 = 방어
        public void ApplyPermanentStat(int statKind, int amount)
        {
            if (amount == 0) return;

            // 런 스텟(다음 레벨에도 유지)과 지금 배틀러 둘 다
            if (statKind == 0)
            {
                _runState.Stats.MaxHealth = Math.Max(1, _runState.Stats.MaxHealth + amount);
                Battler?.ModifyMaxHealth(amount);
            }
            else if (statKind == 1)
            {
                _runState.Stats.Attack += amount;
                Battler?.ModifyAttackDefense(amount, 0);
            }
            else
            {
                // 언리얼도 1 이 아니면 전부 방어로 봄
                _runState.Stats.Defense += amount;
                Battler?.ModifyAttackDefense(0, amount);
            }

            Changed();
        }

        // [보스방] 보상으로 받은 유물 / 스킬 기록 (배신 결과용). "" 이면 그 칸은 그대로
        public void RecordBossReward(string skill, string relic)
        {
            if (!Ids.IsNone(skill)) _runState.BossSkill = skill;
            if (!Ids.IsNone(relic)) _runState.BossRelic = relic;
            Changed();
        }

        // [이벤트] 일시 버프 (다음 전투 몇 번). 전투가 시작될 때 상태로 걸리고, 끝날 때마다 ConsumeTempBuffBattle 로 1 씩 줄어듦
        public void AddTempBuff(TempStatBuff buff)
        {
            if (buff == null || buff.BattlesLeft <= 0) return;

            _runState.TempBuffs.Add(buff.Clone());
            Changed();
        }

        // 전투 한 번이 끝남: 일시 버프 / 일시적 이벤트 유물의 남은 전투 수를 1 씩 (0 이면 사라짐)
        public void ConsumeTempBuffBattle()
        {
            if (_runState.TempBuffs.Count == 0 && _runState.TempRelics.Count == 0) return;

            foreach (var buff in _runState.TempBuffs) --buff.BattlesLeft;
            _runState.TempBuffs.RemoveAll(b => b.BattlesLeft <= 0);

            // 일시적 이벤트 유물도 전투 한 번에 1 씩. 다 되면 유물째 사라짐
            for (int i = _runState.TempRelics.Count - 1; i >= 0; --i)
            {
                var entry = _runState.TempRelics[i];
                if (--entry.BattlesLeft > 0) continue;

                RunLog.Log($"[Relic] {PlayerName}: 일시적 유물 {GameData.GetRelicName(entry.Relic)} 소멸");
                _runState.Relics.RemoveAll(r => r == entry.Relic);
                _runState.TempRelics.RemoveAt(i);
            }
            Changed();
        }

        // 일시적 이벤트 유물의 남은 전투 수. 일시적 유물이 아니면 -1
        public int GetTempRelicBattlesLeft(string relicRow)
        {
            var entry = _runState.TempRelics.Find(e => e.Relic == relicRow);
            return entry != null ? entry.BattlesLeft : -1;
        }

        // 전투 밖에서도 의미 있는 유물 효과 (던전 재화 / 최대 체력 / 픽업 스킬 강화). 전투 중 발동도 여기로
        public void ApplyRelicMetaEffect(RelicRow relic)
        {
            if (relic == null) return;

            switch (relic.ActionKind)
            {
                case ActionKind.GainMoney:
                    AddCurrency(relic.BaseValue);
                    break;

                case ActionKind.MaxHPUp:
                case ActionKind.MaxHPDown:
                {
                    // 런 스텟(다음 레벨에도 유지)과 지금 배틀러 둘 다
                    int delta = relic.ActionKind == ActionKind.MaxHPUp ? relic.BaseValue : -relic.BaseValue;
                    _runState.Stats.MaxHealth = Math.Max(1, _runState.Stats.MaxHealth + delta);
                    Battler?.ModifyMaxHealth(delta);
                    Changed();
                    break;
                }

                case ActionKind.HealSelf:
                    Battler?.Heal(relic.BaseValue);
                    break;

                case ActionKind.RandomUpgrade:
                    // 장착한 픽업 스킬 중 BaseValue 번 랜덤 +1 (최대 단계인 건 빼고)
                    for (int i = 0; i < Math.Max(1, relic.BaseValue); ++i)
                    {
                        var candidates = UpgradableEnhanceSkills();
                        if (candidates.Count == 0) break;
                        UpgradePickupSkill(candidates[_rng.Next(0, candidates.Count)]);
                    }
                    break;
            }
        }

        // 장착한 픽업 스킬 중 지금 강화할 수 있는 것 (중복 없이)
        internal List<string> UpgradableEnhanceSkills()
        {
            var candidates = new List<string>();
            foreach (var skill in _runState.EnhanceSkills)
            {
                if (CanUpgradePickupSkill(skill) && !candidates.Contains(skill)) candidates.Add(skill);
            }
            return candidates;
        }

        internal Random Rng => _rng;

        // 던전 재화. 음수면 깎음 (0 아래로는 안 내려감)
        public void AddCurrency(int amount)
        {
            if (amount == 0) return;

            _runState.Currency = Math.Max(0, _runState.Currency + amount);
            Changed();

            if (amount > 0)
            {
                Quests.OnCurrencyEarned(this, amount);
            }
        }

        // 런 상태를 직접 고치고 복제 / 화면 갱신. 퀘스트처럼 칸이 여럿인 것을 한 번에
        public void ModifyRunState(Action<RunState> edit)
        {
            edit(_runState);
            Changed();
        }

        // ============================================================ 지도 진행

        // 방을 클리어하고 그 방으로 진행. 지도상 현재 위치와 진행 레벨을 갱신하고 선택을 비운다
        public void AdvanceToRoom(int targetRoomId, int targetRow)
        {
            _runState.SelectedRoomId = -1;
            _runState.CurrentRoomId = targetRoomId;
            _runState.CurrentMapLevel = targetRow + 1;   // 방을 클리어했으므로 다음 레벨 진입 가능하게 함
            _runState.VisitedRoomIds.Add(targetRoomId);  // 지도에 지나온 길 표시용
            Changed();
        }

        // 다음 층으로: 지도 위치 처음으로, 지나온 길 비움, 체력 전부 회복
        public void BeginFloor()
        {
            _runState.CurrentRoomId = -1;
            _runState.CurrentMapLevel = 0;
            _runState.SelectedRoomId = -1;
            _runState.VisitedRoomIds.Clear();
            _runState.BossRelic = "";
            _runState.BossSkill = "";
            _runState.FloorMonsterRooms = 0;   // 퀘스트 '한 층에서 N 번'
            _runState.FloorGuardianRooms = 0;
            Changed();

            // 기획: 층이 바뀌면 체력 전부 회복
            Battler?.ReviveFull();
        }

        public void SetSelectedRoomId(int roomId)
        {
            _runState.SelectedRoomId = roomId;
            Changed();
        }

        public void SetCurrentRoomInfo(int roomId, int mapLevel)
        {
            _runState.CurrentRoomId = roomId;
            _runState.CurrentMapLevel = mapLevel;
            Changed();
        }

        // 통째로 갈아끼움 (세이브 불러오기 / 복제 받은 값). 복사해서 들고 있음
        public void SetRunState(RunState state)
        {
            _runState = state != null ? state.Clone() : new RunState();
            Changed();
        }
    }
}
