// 영구 데이터(ProfileSave) 창구 (언리얼 Game/TerminusProfileSubsystem 이식). 게임 켤 때 불러오고, 바뀔 때마다 바로 저장
// 언리얼 UGameInstanceSubsystem -> 일반 클래스. 유니티 쪽이 하나 만들어 들고 있다가 Load() 를 부름
// 콘솔 명령(Terminus.AddGold 등)은 안 옮김 -> 디버그 메뉴에서 AddGold / SetGoldForDebug / AddStoredRelic / AddOwnedSkill 를 부르면 됨
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Run
{
    public sealed class ProfileService
    {
        public const string ProfileSlotName = "Profile";
        public const string ProfileBackupSlotName = "Profile_Backup";     // 저장할 때마다 같이 씀 (원본이 깨졌을 때 복구용)
        public const string ProfileTamperedSlotName = "Profile_Tampered"; // 조작 / 손상된 원본 보관

        readonly ISaveStorage _storage;
        readonly ISaveSerializer _serializer;
        readonly Random _rng;
        readonly UpgradeSettings _settings;

        ProfileSave _profile = new ProfileSave();
        string _loadNotice = "";

        // 골드가 바뀜 (새 골드, 변화량). 메인 메뉴 등 표시 갱신용
        public event Action<int, int> GoldChanged;
        // 거점 강화가 바뀜
        public event Action UpgradesChanged;
        // 창고 내용(보유 스킬 / 보관 유물)이 바뀜. 창고 화면 갱신용
        public event Action StorageChanged;

        /// <param name="settings">null 이면 GameData.Upgrades</param>
        public ProfileService(ISaveStorage storage, ISaveSerializer serializer, Random rng, UpgradeSettings settings = null)
        {
            _storage = storage ?? throw new ArgumentNullException(nameof(storage));
            _serializer = serializer ?? throw new ArgumentNullException(nameof(serializer));
            _rng = rng ?? new Random();
            _settings = settings;
        }

        UpgradeSettings Settings => _settings ?? GameData.Upgrades;

        // 지금 들고 있는 프로필 (읽기 전용으로 쓸 것. 고치면 저장이 안 됨)
        public ProfileSave Profile => _profile;

        /// <summary>언리얼 Initialize. 암호화 세이브. 조작 / 손상이면 백업에서, 백업도 안 되면 새로</summary>
        public void Load()
        {
            var loaded = SaveCrypto.LoadFromSlot<ProfileSave>(_storage, _serializer, ProfileSlotName, out var result);
            bool needsSave = result == SaveLoadResult.Legacy;   // 예전 평문 -> 바로 암호화해서 다시 씀

            if (loaded == null && result == SaveLoadResult.Tampered)
            {
                SaveCrypto.CopySlot(_storage, ProfileSlotName, ProfileTamperedSlotName);

                loaded = SaveCrypto.LoadFromSlot<ProfileSave>(_storage, _serializer, ProfileBackupSlotName, out _);
                _loadNotice = loaded != null
                    ? "세이브 파일이 손상되어 백업에서 복구했습니다."
                    : "세이브 파일이 손상되었거나 변조되어 새로 시작합니다.";
                RunLog.Warn("[Profile] 세이브 조작 / 손상 -> " + (loaded != null ? "백업에서 복구" : "새로 시작"));
                needsSave = true;
            }

            _profile = loaded ?? new ProfileSave();
            Normalize(_profile);

            if (needsSave)
            {
                Save();
            }

            // 예전 세이브엔 같은 유물이 여러 개 있을 수 있음 -> 도감처럼 하나씩만
            var unique = new List<string>();
            foreach (var row in _profile.StoredRelics)
            {
                if (!unique.Contains(row)) unique.Add(row);
            }
            if (unique.Count != _profile.StoredRelics.Count)
            {
                _profile.StoredRelics = unique;
                Save();
            }
        }

        // 직렬화기가 빠진 칸을 null 로 둘 수 있어서
        static void Normalize(ProfileSave p)
        {
            if (p.OwnedSkills == null) p.OwnedSkills = new List<string>();
            if (p.StoredRelics == null) p.StoredRelics = new List<string>();
            if (p.Upgrades == null) p.Upgrades = new List<ClassUpgrades>();
            if (p.PendingSettlement == null) p.PendingSettlement = new PendingSettlement();
            var s = p.PendingSettlement;
            if (s.Relics == null) s.Relics = new List<string>();
            if (s.RelicGold == null) s.RelicGold = new List<int>();
            if (s.LostStoredRelics == null) s.LostStoredRelics = new List<string>();
            foreach (var u in p.Upgrades)
            {
                if (u.StatLevels == null) u.StatLevels = new List<int>();
                if (u.SkillLevels == null) u.SkillLevels = new List<int>();
            }
        }

        // 게임을 켤 때 세이브가 조작 / 손상돼서 백업으로 복구했거나 새로 시작했으면 그 안내. 꺼내면 비워짐 (메인 메뉴 팝업)
        public string ConsumeLoadNotice()
        {
            var output = _loadNotice;
            _loadNotice = "";
            return output;
        }

        void Save()
        {
            if (SaveCrypto.SaveToSlot(_storage, _serializer, _profile, ProfileSlotName))
            {
                SaveCrypto.SaveToSlot(_storage, _serializer, _profile, ProfileBackupSlotName);
            }
        }

        // ============================================================ 보유 스킬

        // 보유 스킬 전부 (저장된 그대로)
        public IReadOnlyList<string> OwnedSkills => _profile.OwnedSkills;

        // 보유 스킬 중 이 직업이 지금 강화 스킬로 장착할 수 있는 것만 (IsEquippableSkill)
        public List<string> GetUsableOwnedSkills(CharacterClass c)
        {
            var output = new List<string>();
            foreach (var row in _profile.OwnedSkills)
            {
                if (IsEquippableSkill(row, c) && !output.Contains(row)) output.Add(row);
            }
            return output;
        }

        // 영구 소유 추가. 이미 있거나 보유할 수 있는 스킬이 아니면 false
        public bool AddOwnedSkill(string skillRow)
        {
            if (!IsOwnableSkill(skillRow) || _profile.OwnedSkills.Contains(skillRow)) return false;

            _profile.OwnedSkills.Add(skillRow);
            Save();
            StorageChanged?.Invoke();
            return true;
        }

        public void ClearOwnedSkills()
        {
            _profile.OwnedSkills.Clear();
            Save();
            StorageChanged?.Invoke();
        }

        // 보유할 수 있는 스킬인가 (DT 에 있고, 기본 스킬 / 몬스터 스킬이 아님). 직업은 안 봄
        public static bool IsOwnableSkill(string skillRow)
        {
            var row = Ids.IsNone(skillRow) ? null : GameData.FindSkillRow(skillRow);
            return row != null
                && row.SkillCategory != SkillCategory.Basic
                && row.OwnerClass != SkillOwner.Monster;
        }

        // 이 직업이 런 시작 때 장착할 수 있는가
        // 기획(UI 레퍼런스 > 게임 시작): 카테고리가 Common / Event 면 누구나, Personal 이면 OwnerClass 가 같을 때만
        public static bool IsEquippableSkill(string skillRow, CharacterClass c)
        {
            if (!IsOwnableSkill(skillRow)) return false;

            var row = GameData.FindSkillRow(skillRow);
            if (row.SkillCategory != SkillCategory.Personal) return true;   // Common / Event

            switch (c)
            {
                case CharacterClass.Fighter: return row.OwnerClass == SkillOwner.Fighter;
                case CharacterClass.Engineer: return row.OwnerClass == SkillOwner.Engineer;
                case CharacterClass.Paladin: return row.OwnerClass == SkillOwner.Paladin;
                case CharacterClass.Assassin: return row.OwnerClass == SkillOwner.Assassin;
                default: return false;
            }
        }

        // ============================================================ 골드
        // 각자 자기 컴퓨터의 세이브에 있음. 멀티에서 정산하면 서버가 각 클라에 "얼마 얻었다"를 알려주고
        // 클라가 자기 세이브에 더하는 식으로 쓸 것 (서버가 남의 세이브를 만지지 않음)

        public int Gold => _profile.Gold;

        // 골드 얻기 (0 이하는 무시). 바로 저장
        public void AddGold(int amount)
        {
            if (amount <= 0) return;

            // int 넘침 방지
            long sum = (long)_profile.Gold + amount;
            _profile.Gold = (int)Math.Min(sum, int.MaxValue);
            Save();
            GoldChanged?.Invoke(_profile.Gold, amount);
        }

        // 골드 쓰기. 모자라면 false 하고 그대로. 바로 저장
        public bool TrySpendGold(int cost)
        {
            if (cost < 0 || _profile.Gold < cost) return false;
            if (cost == 0) return true;

            _profile.Gold -= cost;
            Save();
            GoldChanged?.Invoke(_profile.Gold, -cost);
            return true;
        }

        // [테스트] 골드를 이 값으로 (언리얼 Terminus.SetGold 콘솔 명령)
        public void SetGoldForDebug(int newGold)
        {
            int clamped = Math.Max(0, newGold);
            int delta = clamped - _profile.Gold;
            _profile.Gold = clamped;
            Save();
            GoldChanged?.Invoke(_profile.Gold, delta);
        }

        // ============================================================ 창고 유물

        public IReadOnlyList<string> StoredRelics => _profile.StoredRelics;

        // 보관할 수 있는 유물인가: DT 에 있고, 직업 기본 유물(Basic) / 몬스터 유물이 아님
        public static bool IsStorableRelic(string relicRow)
        {
            var relic = Ids.IsNone(relicRow) ? null : GameData.FindRelicRow(relicRow);
            return relic != null && relic.RelicTier != RelicTier.Basic && relic.OwnerClass != SkillOwner.Monster;
        }

        // 창고에 넣기. 개수 제한 없음, 같은 유물은 한 번만 (도감)
        // 보관할 수 없는 유물이거나 이미 있으면 false
        public bool AddStoredRelic(string relicRow)
        {
            if (!IsStorableRelic(relicRow) || _profile.StoredRelics.Contains(relicRow)) return false;

            _profile.StoredRelics.Add(relicRow);
            Save();
            StorageChanged?.Invoke();
            return true;
        }

        // 창고에서 하나 빼기 (던전에 들고 갈 때 등)
        public bool RemoveStoredRelic(string relicRow)
        {
            if (!_profile.StoredRelics.Remove(relicRow)) return false;

            Save();
            StorageChanged?.Invoke();
            return true;
        }

        public void ClearStoredRelics()
        {
            _profile.StoredRelics.Clear();
            Save();
            StorageChanged?.Invoke();
        }

        // ============================================================ 거점 강화 (직업마다 따로. 골드로 사고 바로 저장)

        // 이 직업의 강화 단계 (없으면 전부 0). 복사본
        public ClassUpgrades GetClassUpgrades(CharacterClass c)
        {
            foreach (var u in _profile.Upgrades)
            {
                if (u.Class == c) return u.Clone();
            }
            return new ClassUpgrades { Class = c };
        }

        // 모든 직업의 강화 (멀티에서 서버에 알릴 때). 복사본
        public List<ClassUpgrades> GetAllUpgrades() => _profile.Upgrades.ConvertAll(u => u.Clone());

        ClassUpgrades FindOrAddUpgrades(CharacterClass c)
        {
            foreach (var u in _profile.Upgrades)
            {
                if (u.Class == c) return u;
            }
            var added = new ClassUpgrades { Class = c };
            _profile.Upgrades.Add(added);
            return added;
        }

        // 연무장 한 단계 올리기. 골드가 모자라거나 최대면 false
        public bool TryUpgradeStat(CharacterClass c, StatUpgrade stat)
        {
            int index = (int)stat;
            if (index < 0 || index >= ClassUpgrades.StatCount) return false;

            var current = GetClassUpgrades(c);
            int cost = Settings.GetStatCost(stat, current.GetStatLevel(stat));
            if (cost < 0 || !TrySpendGold(cost)) return false;

            var upgrades = FindOrAddUpgrades(c);
            while (upgrades.StatLevels.Count < ClassUpgrades.StatCount) upgrades.StatLevels.Add(0);
            ++upgrades.StatLevels[index];

            Save();
            UpgradesChanged?.Invoke();
            return true;
        }

        // 훈련소 한 단계 올리기 (기본 스킬 칸 0~2). 골드가 모자라거나 최대면 false
        public bool TryUpgradeSkill(CharacterClass c, int skillIndex)
        {
            if (skillIndex < 0 || skillIndex >= ClassUpgrades.NumBasicSkills) return false;

            var current = GetClassUpgrades(c);
            int cost = Settings.GetSkillCost(current.GetSkillLevel(skillIndex));
            if (cost < 0 || !TrySpendGold(cost)) return false;

            var upgrades = FindOrAddUpgrades(c);
            while (upgrades.SkillLevels.Count < ClassUpgrades.NumBasicSkills) upgrades.SkillLevels.Add(0);
            ++upgrades.SkillLevels[skillIndex];

            Save();
            UpgradesChanged?.Invoke();
            return true;
        }

        // [테스트] 강화 전부 0 으로
        public void ResetUpgrades()
        {
            _profile.Upgrades.Clear();
            Save();
            UpgradesChanged?.Invoke();
        }

        // ============================================================ 정산
        // 던전 탈출 -> 정산 대기 저장 -> 메인 화면에서 유물 보관 / 판매 -> 골드

        /// <summary>
        /// 이번 런 결과로 정산 내용을 만듦: 고를 수 있는 유물(시작 유물 StartRelics 제외), 얻을 스킬 하나.
        /// bDeath = 전멸 (스킬 없음. 유물은 탈출과 똑같이 보관 / 판매)
        /// </summary>
        public PendingSettlement MakeSettlement(IList<string> runRelics, IList<string> startRelics, IList<string> enhanceSkills,
            string reason, string roomName, int floor, bool bDeath = false)
        {
            var output = new PendingSettlement
            {
                bValid = true,
                bDeath = bDeath,
                Reason = reason ?? "",
                RoomName = roomName ?? "",
                Floor = floor,
            };

            foreach (var row in runRelics)
            {
                // 직업 기본 유물 / 몬스터 유물 / 창고에서 들고 간 시작 유물은 후보가 아님
                if (!IsStorableRelic(row) || startRelics.Contains(row) || output.Relics.Contains(row)) continue;
                output.Relics.Add(row);

                // 안 고르면 이 값에 판매 (DT 정산 판매가 그대로, 못 파는 건 0)
                var relic = GameData.FindRelicRow(row);
                output.RelicGold.Add(relic != null && relic.CanSellAtSettlement() ? relic.SellPrice_Gold : 0);
            }

            // 사망: 강화 스킬은 사라짐 (가져가는 스킬 없음)
            if (bDeath)
            {
                return output;
            }

            // 장착 중인 픽업 스킬 중 아직 없는 것 하나 랜덤 (중복 X, 전부 있으면 건너뜀)
            var newSkills = new List<string>();
            foreach (var skill in enhanceSkills)
            {
                if (IsOwnableSkill(skill) && !_profile.OwnedSkills.Contains(skill) && !newSkills.Contains(skill)) newSkills.Add(skill);
            }
            if (newSkills.Count > 0)
            {
                output.KeptSkill = newSkills[_rng.Next(0, newSkills.Count)];
            }
            return output;
        }

        // 정산 대기로 저장 (이미 있으면 덮어씀)
        public void BeginSettlement(PendingSettlement settlement)
        {
            if (settlement == null) return;

            _profile.PendingSettlement = settlement.Clone();
            _profile.PendingSettlement.bValid = true;
            Save();
            RunLog.Log($"[Profile] 정산 대기: 유물 {settlement.Relics.Count}개, 스킬 {settlement.KeptSkill}");
        }

        public bool HasPendingSettlement => _profile.PendingSettlement != null && _profile.PendingSettlement.bValid;

        // 정산 대기 내용 (복사본)
        public PendingSettlement GetPendingSettlement() => (_profile.PendingSettlement ?? new PendingSettlement()).Clone();

        /// <summary>
        /// 정산 끝: 고른 유물들을 창고에, 스킬을 보유 스킬에, 골드(안 고른 유물 판매 + 퀘스트 / 배신 보너스)를 골드에. 정산 대기는 비움.
        /// 하드 모드 사망이면 LostStoredRelics 를 창고에서 지움
        /// </summary>
        public void FinishSettlement(IList<string> keptRelics, int goldEarned)
        {
            if (!HasPendingSettlement) return;

            var done = _profile.PendingSettlement;
            _profile.PendingSettlement = new PendingSettlement();   // 먼저 비워서 아래가 중간에 실패해도 두 번 받지 않게
            Save();

            // 하드 모드 사망: 들고 간 창고 유물을 창고에서 지움
            foreach (var row in done.LostStoredRelics)
            {
                RemoveStoredRelic(row);
            }

            // 고른 유물을 보관 (이미 있는 유물은 아무 일도 없음: 사용자 결정 10-06). 나머지는 판매되어 goldEarned 에 들어 있음
            foreach (var kept in keptRelics)
            {
                if (done.Relics.Contains(kept)) AddStoredRelic(kept);
            }
            if (!Ids.IsNone(done.KeptSkill))
            {
                AddOwnedSkill(done.KeptSkill);
            }
            AddGold(goldEarned);

            RunLog.Log($"[Profile] 정산 끝: 골드 +{goldEarned}, 보관 [{string.Join(", ", keptRelics)}], 스킬 {done.KeptSkill}");
        }
    }
}
