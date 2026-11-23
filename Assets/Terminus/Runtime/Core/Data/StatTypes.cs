// 캐릭터 스텟 (언리얼 Data/StatTypes.h 이식)
// 최대치(CharacterStats, 클래스가 정하는 정의)와 현재치(CombatState, 전투 중에만 사는 상태)를 나눠 둠
// 언리얼에선 값 복사되는 구조체였음 -> C# 에선 class 라 복사할 땐 반드시 Clone()
using System.Collections.Generic;

namespace Terminus.Data
{
    /// <summary>캐릭터 클래스가 정하는 고정 스텟. 전투 중엔 안 바뀐다. 기본값은 무도가 기준</summary>
    [System.Serializable]
    public sealed class CharacterStats
    {
        public int MaxHealth = 70;
        // 공격 스킬 보정
        public int Attack = 0;
        // 보호막 생성과 체력 회복 보정
        public int Defense = 0;
        // 체술 효과 보정
        public int Evasion = 0;
        // 기본 스킬용 에너지 상한. 매 턴 이 값까지 회복하고 턴 끝나면 0이 됨
        public int MaxEnergy = 3;
        // 획득 스킬용 자원의 상한
        public int MaxSkillEnergy = 2;
        // 전투 시작 때 갖고 시작하는 스킬 에너지 (연무장 '스킬 에너지' 강화로 늘어남)
        public int StartSkillEnergy = 0;
        // 강화 스킬을 낄 수 있는 칸 수
        public int EnhanceSlots = 3;
        // 인게임 유물 보유 최대치. 0 이하면 기본 6 으로 취급
        public int MaxRelics = 6;

        public CharacterStats Clone() => (CharacterStats)MemberwiseClone();
    }

    [System.Serializable]
    public sealed class StatusInstance
    {
        public StatusEffect Type = StatusEffect.None;
        public int Value = 0;
        public int Duration = 0;
        // 주인의 자기 턴 중에 걸림 -> 그 턴 끝에는 안 줄어듦
        public bool bSkipNextTick = false;

        public StatusInstance Clone() => (StatusInstance)MemberwiseClone();
    }

    /// <summary>전투 중에만 사는 현재값. 전투가 끝나면 버린다. 서버가 바꾸고 클라는 받기만 함</summary>
    [System.Serializable]
    public sealed class CombatState
    {
        public int Health = 0;
        // 피해를 먼저 먹고 사라짐
        public int Shield = 0;
        public int Energy = 0;
        public int SkillEnergy = 0;
        // 에너지를 2회 쓸 때마다 스킬 에너지 1 회복 -> 나머지를 들고 있어야 해서 카운터가 따로 필요
        public int EnergySpentCounter = 0;
        public List<StatusInstance> Statuses = new List<StatusInstance>();

        public CombatState Clone()
        {
            var c = (CombatState)MemberwiseClone();
            c.Statuses = new List<StatusInstance>(Statuses.Count);
            foreach (var s in Statuses) c.Statuses.Add(s.Clone());
            return c;
        }
    }
}
