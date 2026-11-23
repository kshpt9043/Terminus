// 던전 전투 흐름이 쓰는 열거형 / 작은 구조체 (언리얼 Dungeon/DungeonCombatComponent.h 의 UENUM / USTRUCT)
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Dungeon
{
    public enum CombatPhase
    {
        None,          // 전투 없음
        PlayerTurn,    // 이 구역 플레이어들이 동시에 행동. 전원 턴 종료하면 몬스터 턴
        MonsterTurn,   // 몬스터들이 예고한 행동을 차례로
        Victory,       // 몬스터 전멸. 잠시 뒤 구역 클리어
        Defeat,        // 플레이어 전멸
    }

    // 머리 위 행동 예고 아이콘 종류 (기획: 공격 / 방어 / 버프 / 디버프)
    public enum MonsterIntentKind
    {
        Attack,
        Defend,
        Buff,
        Debuff,
    }

    /// <summary>
    /// 언리얼 StepTimer 하나에 걸리던 다음 일. 한 번에 하나만 (SetTimer 가 앞의 것을 덮어씀)
    /// Game 쪽은 Tick(now) 만 불러 주면 됨
    /// </summary>
    public enum CombatStep
    {
        None,
        MonsterAction,           // RunNextMonsterAction (MonsterActionInterval)
        ReportWipe,              // 전멸 알림 (CombatEndDelay)
        StartRoomRewards,        // 보상 화면 (RewardDelay)
        ClearArea,               // 구역 클리어 (CombatEndDelay / 보상 끝나고 RewardFinishDelay)
        ReportBetrayalResult,    // 배신 결과 (CombatEndDelay)
    }

    /// <summary>
    /// 같이 나오는 몬스터 규칙 (노션 던전 테마: 슬라임 킹 = 가디언 1, 리자드맨 족장 = 일반 몬스터와 함께 등)
    /// 몬스터 데이터에 칸이 없어서 전투 설정으로 둠 (언리얼 FMonsterCompanionRule)
    /// </summary>
    [System.Serializable]
    public sealed class MonsterCompanionRule
    {
        // 특정 몬스터(DT_Monster 행 이름). 비우면 아래 분류에서 같은 테마로 무작위
        public string FixedRow = "";

        public MonsterCategory Category = MonsterCategory.Normal;

        // 언리얼 ClampMin 1
        public int Count = 1;

        public MonsterCompanionRule() { }

        public MonsterCompanionRule(MonsterCategory category, int count, string fixedRow = "")
        {
            Category = category;
            Count = count;
            FixedRow = fixedRow ?? "";
        }

        public MonsterCompanionRule Clone() => (MonsterCompanionRule)MemberwiseClone();
    }

    /// <summary>몬스터 한 마리의 이번 사이클 행동. 사이클 시작 때 정해서 모두에게 보여줌 (언리얼 FMonsterIntent)</summary>
    public sealed class MonsterIntent
    {
        public DungeonMonster Monster;

        // DT_Skill 행 이름
        public string SkillRow = "";

        public MonsterIntentKind Kind = MonsterIntentKind.Attack;

        // 공격이면 한 대 피해량 (기획: 공격은 수치도 같이 표기)
        public int Amount = 0;

        public int HitCount = 1;

        // 몬스터는 같은 객체를 가리킴 (언리얼 TObjectPtr 복사)
        public MonsterIntent Clone() => (MonsterIntent)MemberwiseClone();
    }

    /// <summary>언리얼 FIntPoint (최소, 최대) 자리. X = 최소, Y = 최대</summary>
    [System.Serializable]
    public sealed class CombatIntRange
    {
        public int X;
        public int Y;

        public CombatIntRange() { }

        public CombatIntRange(int x, int y)
        {
            X = x;
            Y = y;
        }

        public CombatIntRange Clone() => (CombatIntRange)MemberwiseClone();
    }

    /// <summary>언리얼 FVector2D (최소, 최대) 자리</summary>
    [System.Serializable]
    public sealed class CombatFloatRange
    {
        public float X;
        public float Y;

        public CombatFloatRange() { }

        public CombatFloatRange(float x, float y)
        {
            X = x;
            Y = y;
        }

        public CombatFloatRange Clone() => (CombatFloatRange)MemberwiseClone();
    }

    /// <summary>
    /// 가중치 붙은 스킬 하나 (언리얼 TMap&lt;FName, int32&gt; 의 한 칸).
    /// TMap 은 넣은 순서대로 돌아서 가중치 굴림이 그 순서에 기댐 -> Dictionary 대신 목록
    /// </summary>
    [System.Serializable]
    public sealed class WeightedSkill
    {
        public string Skill = "";
        public int Weight = 0;

        public WeightedSkill() { }

        public WeightedSkill(string skill, int weight)
        {
            Skill = skill ?? "";
            Weight = weight;
        }

        public WeightedSkill Clone() => (WeightedSkill)MemberwiseClone();
    }

    /// <summary>
    /// 전투가 지도 / 구역 쪽에 물어보는 것 (언리얼에선 AMapManager / ADungeonArea / UTerminusRunSubsystem 을 직접 찾았음).
    /// 없으면(null) 언리얼에서 그 액터가 없을 때와 같은 기본값: 1층, 몬스터 자리 3, 가디언 중복 규칙 없음
    /// </summary>
    public interface IDungeonCombatHost
    {
        // AMapManager::CurrentFloor
        int CurrentFloor { get; }

        // ADungeonArea::GetNumMonsterSlots (구역 몬스터 자리 수)
        int NumMonsterSlots { get; }

        // 이번 런에 나온 가디언 (UTerminusRunSubsystem). null 이면 가디언도 그냥 무작위 1 마리
        Run.RunSession RunSession { get; }
    }

    /// <summary>필드로 채워 쓰는 기본 구현 (테스트 / 간단한 구역)</summary>
    public sealed class DungeonCombatHost : IDungeonCombatHost
    {
        public int CurrentFloor { get; set; } = 1;
        public int NumMonsterSlots { get; set; } = 3;
        public Run.RunSession RunSession { get; set; }
    }

    internal static class DungeonCombatListUtil
    {
        // 언리얼 TArray::SetNum(작게) 자리
        public static void Truncate<T>(List<T> list, int count)
        {
            if (count < list.Count) list.RemoveRange(count, list.Count - count);
        }
    }
}
