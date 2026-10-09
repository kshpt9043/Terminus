#include "Data/UpgradeTypes.h"

#include "Player/TerminusPlayerState.h"

FText GetStatUpgradeName(EStatUpgrade Stat)
{
	switch (Stat)
	{
	case EStatUpgrade::Health:        return FText::FromString(TEXT("체력"));
	case EStatUpgrade::Attack:        return FText::FromString(TEXT("공격"));
	case EStatUpgrade::Defense:       return FText::FromString(TEXT("방어"));
	case EStatUpgrade::Evasion:       return FText::FromString(TEXT("회피"));
	case EStatUpgrade::Energy:        return FText::FromString(TEXT("에너지"));
	case EStatUpgrade::SkillEnergy:   return FText::FromString(TEXT("스킬 에너지"));
	case EStatUpgrade::RelicCapacity: return FText::FromString(TEXT("유물 최대치"));
	case EStatUpgrade::PickupSlots:   return FText::FromString(TEXT("픽업 스킬 칸"));
	default:                          return FText::GetEmpty();
	}
}

UTerminusUpgradeSettings::UTerminusUpgradeSettings()
{
	CategoryName = TEXT("Game");

	// 기획 연무장 시안 (최대 단계 / 첫 비용). 비용 증가 폭과 단계별 수치는 미정이라 임시
	auto Rule = [](int32 MaxLevel, int32 BaseCost, int32 CostStep, int32 Amount)
	{
		FStatUpgradeRule R;
		R.MaxLevel = MaxLevel;
		R.BaseCost = BaseCost;
		R.CostStep = CostStep;
		R.AmountPerLevel = Amount;
		return R;
	};

	StatRules.Add(EStatUpgrade::Health,        Rule(10, 1000,  500, 5));
	StatRules.Add(EStatUpgrade::Attack,        Rule(5,  1000,  500, 1));
	StatRules.Add(EStatUpgrade::Defense,       Rule(5,  1000,  500, 1));
	StatRules.Add(EStatUpgrade::Evasion,       Rule(3,  3000, 1500, 1));
	StatRules.Add(EStatUpgrade::Energy,        Rule(2,  3000, 1500, 1));
	StatRules.Add(EStatUpgrade::SkillEnergy,   Rule(3,  3000, 1500, 1));
	// 유물 칸: 기본 6 -> 최대 15 (사용자 결정). 중간 수치 미정이라 한 단계 +1, 9단계
	StatRules.Add(EStatUpgrade::RelicCapacity, Rule(ATerminusPlayerState::MaxRelicCapacity - ATerminusPlayerState::BaseRelicCapacity, 1000, 500, 1));
	StatRules.Add(EStatUpgrade::PickupSlots,   Rule(2, 10000, 5000, 1));
}

int32 UTerminusUpgradeSettings::GetStatCost(EStatUpgrade Stat, int32 CurrentLevel) const
{
	const FStatUpgradeRule* Rule = FindStatRule(Stat);
	if (!Rule || CurrentLevel >= Rule->MaxLevel) return -1;
	return Rule->BaseCost + Rule->CostStep * CurrentLevel;
}

int32 UTerminusUpgradeSettings::GetSkillCost(int32 CurrentLevel) const
{
	if (CurrentLevel >= SkillMaxLevel) return -1;
	return SkillBaseCost + SkillCostStep * CurrentLevel;
}

void UTerminusUpgradeSettings::ApplyToStats(const FClassUpgrades& Upgrades, FCharacterStats& InOutStats) const
{
	auto Bonus = [&](EStatUpgrade Stat)
	{
		const FStatUpgradeRule* Rule = FindStatRule(Stat);
		return Rule ? FMath::Min(Upgrades.GetStatLevel(Stat), Rule->MaxLevel) * Rule->AmountPerLevel : 0;
	};

	InOutStats.MaxHealth        += Bonus(EStatUpgrade::Health);
	InOutStats.Attack           += Bonus(EStatUpgrade::Attack);
	InOutStats.Defense          += Bonus(EStatUpgrade::Defense);
	InOutStats.Evasion          += Bonus(EStatUpgrade::Evasion);
	InOutStats.MaxEnergy        += Bonus(EStatUpgrade::Energy);
	InOutStats.StartSkillEnergy += Bonus(EStatUpgrade::SkillEnergy);
	InOutStats.EnhanceSlots     += Bonus(EStatUpgrade::PickupSlots);

	// 유물 칸: 데이터에 값이 없으면(0) 기본 6 에서
	const int32 BaseRelics = InOutStats.MaxRelics > 0 ? InOutStats.MaxRelics : ATerminusPlayerState::BaseRelicCapacity;
	InOutStats.MaxRelics = FMath::Min(BaseRelics + Bonus(EStatUpgrade::RelicCapacity), ATerminusPlayerState::MaxRelicCapacity);
}

FClassUpgrades UTerminusUpgradeSettings::Sanitize(const FClassUpgrades& In) const
{
	FClassUpgrades Out;
	Out.Class = In.Class;

	Out.StatLevels.SetNumZeroed(static_cast<int32>(EStatUpgrade::MAX));
	for (int32 i = 0; i < Out.StatLevels.Num(); ++i)
	{
		const FStatUpgradeRule* Rule = FindStatRule(static_cast<EStatUpgrade>(i));
		Out.StatLevels[i] = Rule ? FMath::Clamp(In.GetStatLevel(static_cast<EStatUpgrade>(i)), 0, Rule->MaxLevel) : 0;
	}

	Out.SkillLevels.SetNumZeroed(ATerminusPlayerState::NumBasicSkills);
	for (int32 i = 0; i < Out.SkillLevels.Num(); ++i)
	{
		Out.SkillLevels[i] = FMath::Clamp(In.GetSkillLevel(i), 0, SkillMaxLevel);
	}
	return Out;
}
