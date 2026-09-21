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
	static void Execute(const FSkillRow& Skill,
						UCombatStatsComponent* Caster,
						const TArray<UCombatStatsComponent*>& Targets);

private:
	// BaseValue 를 뭘로 쓰는지
	enum class EValueUse : uint8
	{
		None,
		Damage,
		Shield,
		Heal
	};

	struct FRecipe
	{
		EValueUse Use = EValueUse::None;
		bool bOnCaster = false;    // true 면 대상 말고 시전자한테
	};

	static FRecipe GetRecipe(EActionKind Kind);
	static int32 GetEffectiveStat(const UCombatStatsComponent* Who, EScalingStat Stat);
	static EStatusEffect ResolveFusion(EStatusEffect Type);
};