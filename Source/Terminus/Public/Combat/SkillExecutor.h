#pragma once

#include "CoreMinimal.h"
#include "Data/SkillTypes.h"

class UCombatStatsComponent;

/**
 * 스킬 한 줄(FSkillRow)을 컴포넌트 동사로 바꿔주는 다리.
 * 들고 있는 값이 없어서 객체를 안 만들고 static 으로만 씀.
 * 그냥 C++파일임!!!!!!!!!!!!
 */
class TERMINUS_API FSkillExecutor
{
public:
	// Opponents = 시전자의 상대 편 전체. 광역 추가 피해(SingleAoe) / 적 전체 상태(SingleAoePoison) 가 씀. 비면 Targets 로 대신함
	static void Execute(const FSkillRow& Skill,
						UCombatStatsComponent* Caster,
						const TArray<UCombatStatsComponent*>& Targets,
						const TArray<UCombatStatsComponent*>& Opponents = TArray<UCombatStatsComponent*>());

	// 실행하지 않고 최종 수치만 (BaseValue + 스텟 보정). 몬스터 행동 예고에 "공격 7" 처럼 띄울 때
	static int32 PreviewAmount(const FSkillRow& Skill, const UCombatStatsComponent* Caster);

	// BaseValue 를 무엇으로 쓰는지만 알려 줌 (Execute 의 ② 와 같은 표). 행동 계획 공유가 "피해 6 / 회복 4" 를 띄울 때
	// 셋 다 false 면 수치를 안 쓰는 스킬. bOutOnCaster = 대상 말고 시전자에게
	static void DescribeValueUse(EActionKind Kind, bool& bOutDamage, bool& bOutShield, bool& bOutHeal, bool& bOutOnCaster);

private:
	// 수치를 뭘로 쓰는지
	enum class EValueUse : uint8
	{
		None,
		Damage,
		Shield,
		Heal
	};

	// 상태이상을 누구한테 거는지
	enum class EStatusTo : uint8
	{
		Targets,     // 고른 대상 (Self 스킬이면 시전자)
		Caster,      // 적을 때리고 자기한테 버프 (HitEnemyProtect 등)
		Opponents    // 상대 편 전체 (주 대상 + 광역 + 적 전체 중독)
	};

	// ActionKind 하나 = 원자 몇 개의 조합. Enum_Reference 시트의 "참조 ActionKind" 칸 그대로
	struct FRecipe
	{
		// BaseValue (+ 스텟)
		EValueUse Use = EValueUse::None;
		bool bOnCaster = false;          // true 면 대상 말고 시전자한테

		// SecondaryValue
		bool bAoeDamage = false;         // 상대 편 전체에 (Secondary + 공격) 추가 피해
		EValueUse SecondaryOnCaster = EValueUse::None;   // 시전자에게 (Secondary + 방어) 보호막 / 회복
		bool bDrain = false;             // 체력을 깎은 타격마다 시전자가 Secondary 만큼 회복

		bool bGainEnergy = false;        // BaseValue (+ 스텟) 만큼 시전자 에너지

		EStatusTo StatusTo = EStatusTo::Targets;
		EStatusEffect ExtraStatus = EStatusEffect::None;   // 같은 수치 / 지속으로 하나 더 (BraveAllSolidAll 의 견고 등)
		bool bScaleStatus = false;       // 상태 수치에도 스텟 보정 (은신: (1+방어) 만큼 민첩)
		bool bStatusFromBase = false;    // 상태 수치 = BaseValue (EvasionSelfPoisonAll 의 민첩)
		EStatusEffect BaseStatus = EStatusEffect::None;    // bStatusFromBase 로 거는 상태
	};

	static FRecipe GetRecipe(EActionKind Kind);
	static int32 GetEffectiveStat(const UCombatStatsComponent* Who, EScalingStat Stat);
	static EStatusEffect ResolveFusion(EStatusEffect Type);
};
