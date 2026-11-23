// 전투 중 몬스터 한 마리 (언리얼 ATerminusMonster 중 전투 규칙이 보는 부분: 행 이름 + 전투 스텟)
// 액터 / 플립북 / 숨김 연출은 Game 쪽. Game 은 DungeonCombat.MonsterSpawned 를 받아 SlotIndex 자리에 몬스터를 세우고
// Stats 의 OnDamaged / OnDied 를 직접 들어 피격 / 사망을 재생 (언리얼 ATerminusMonster::HandleDamaged / HandleDied 와 같음)
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;

namespace Terminus.Dungeon
{
    public sealed class DungeonMonster
    {
        public DungeonMonster(int id, int index, int slotIndex, string rowName, MonsterRow row, CombatStats stats)
        {
            Id = id;
            Index = index;
            SlotIndex = slotIndex;
            RowName = rowName ?? "";
            Row = row;
            Stats = stats;
        }

        // 이 전투 객체 안에서 겹치지 않는 번호 (네트워크 쪽이 몬스터를 가리킬 때)
        public int Id { get; }

        // Monsters 안의 순서 = 플레이어 스킬의 적 대상 인덱스
        public int Index { get; }

        // 구역 몬스터 자리 (언리얼 GetMonsterSlotLocation(i) 의 i. 스폰을 건너뛴 행이 있으면 Index 와 다를 수 있음)
        public int SlotIndex { get; }

        // DT_Monster 행 이름 (외형 DT_MonsterVisual 도 이 이름으로)
        public string RowName { get; }

        public MonsterRow Row { get; }

        public CombatStats Stats { get; }

        // HideAfterDeath 를 이미 요청했는가 (언리얼 bHideQueued / IsHidden)
        public bool bHideRequested { get; internal set; }

        public bool IsDead() => Stats == null || Stats.IsDead();

        public override string ToString() => $"{RowName}#{Id}";

        // ------------------------------------------------------------ 순수 규칙

        /// <summary>
        /// 몬스터의 유물: DT_Monster 의 PassiveID (세미콜론으로 여러 개). 유물 행 이름이 아닌 건 건너뜀
        /// </summary>
        public static List<string> GetPassiveRelics(MonsterRow row)
        {
            var relics = new List<string>();
            if (row == null || string.IsNullOrEmpty(row.PassiveID)) return relics;

            foreach (var raw in row.PassiveID.Split(';'))
            {
                var part = raw.Trim();
                if (part.Length == 0) continue;
                if (GameData.FindRelicRow(part) != null)
                {
                    relics.Add(part);
                }
                else
                {
                    DungeonCombat.LogInfo?.Invoke($"[Relic] {row.RowName} 의 PassiveID '{part}' 는 유물이 아님 (DT_Relic 에 없음)");
                }
            }
            return relics;
        }

        /// <summary>스킬 -> 머리 위 예고 아이콘 종류</summary>
        public static MonsterIntentKind ToIntentKind(SkillRow skill)
        {
            switch (skill.SkillType)
            {
                case SkillType.Attack: return MonsterIntentKind.Attack;
                case SkillType.Defense: return MonsterIntentKind.Defend;
                default:
                    // 특수: 자기편에게 걸면 버프, 상대에게 걸면 디버프
                    return (skill.TargetType == TargetType.SingleEnemy || skill.TargetType == TargetType.AllEnemies)
                        ? MonsterIntentKind.Debuff
                        : MonsterIntentKind.Buff;
            }
        }

        /// <summary>DT_Monster 의 스킬 칸 (행 이름 / 가중치). 비었거나 가중치 0 인 칸은 빠짐</summary>
        public static List<WeightedSkill> GetSkillSlots(MonsterRow row)
        {
            var slots = new List<WeightedSkill>();
            if (row == null) return slots;

            void Add(string skill, int weight)
            {
                if (!Ids.IsNone(skill) && weight > 0) slots.Add(new WeightedSkill(skill, weight));
            }
            Add(row.Skill1_ID, row.Skill1_Weight);
            Add(row.Skill2_ID, row.Skill2_Weight);
            Add(row.Skill3_ID, row.Skill3_Weight);
            return slots;
        }

        /// <summary>
        /// AppearFloor 글자의 앞 숫자 ("2층" -> 2, "심층" -> 0). 언리얼 FCString::Atoi 와 같음
        /// </summary>
        public static int ParseAppearFloor(string appearFloor)
        {
            if (string.IsNullOrEmpty(appearFloor)) return 0;

            int i = 0;
            while (i < appearFloor.Length && char.IsWhiteSpace(appearFloor[i])) ++i;

            bool negative = false;
            if (i < appearFloor.Length && (appearFloor[i] == '+' || appearFloor[i] == '-'))
            {
                negative = appearFloor[i] == '-';
                ++i;
            }

            long value = 0;
            while (i < appearFloor.Length && appearFloor[i] >= '0' && appearFloor[i] <= '9')
            {
                value = value * 10 + (appearFloor[i] - '0');
                if (value > int.MaxValue) break;
                ++i;
            }
            return (int)(negative ? -value : value);
        }
    }
}
