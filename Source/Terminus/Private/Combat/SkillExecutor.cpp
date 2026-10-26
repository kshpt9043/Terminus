#include "Combat/SkillExecutor.h"
#include "Combat/CombatStatsComponent.h"

void FSkillExecutor::Execute(const FSkillRow& Skill,
							 UCombatStatsComponent* Caster,
							 const TArray<UCombatStatsComponent*>& Targets,
							 const TArray<UCombatStatsComponent*>& Opponents)
{
	if (!Caster)
	{
		return;
	}

	// ① 최종 수치. 스텟에 딱지까지 합산해서 Ratio 곱함
	const int32 Stat = GetEffectiveStat(Caster, Skill.ScalingStat);
	const int32 Amount = Skill.BaseValue + FMath::RoundToInt(Stat * Skill.ScalingRatio);

	// ② 무엇을 누구한테 할지
	const FRecipe Recipe = GetRecipe(Skill.ActionKind);
	const TArray<UCombatStatsComponent*>& AllOpponents = Opponents.Num() > 0 ? Opponents : Targets;

	TArray<UCombatStatsComponent*> Receivers = Targets;
	if (Recipe.bOnCaster)
	{
		Receivers.Reset();
		Receivers.Add(Caster);
	}

	// ③ BaseValue 적용. 흡혈은 체력을 깎은 타격 수를 셈
	int32 DrainHits = 0;
	for (UCombatStatsComponent* R : Receivers)
	{
		if (!R)
		{
			continue;
		}

		switch (Recipe.Use)
		{
		case EValueUse::Damage:
			for (int32 i = 0; i < FMath::Max(1, Skill.HitCount); ++i)
			{
				if (R->ApplyDamage(Amount, Caster) > 0)   // 시전자 = 때린 쪽 (공포 / 반격 / 타격 판정)
				{
					++DrainHits;
				}
			}
			break;
		case EValueUse::Shield:
			R->AddShield(Amount);
			break;
		case EValueUse::Heal:
			R->Heal(Amount);
			break;
		default:
			break;
		}
	}

	// ④ SecondaryValue: 광역 추가 피해는 공격, 시전자 보호막 / 회복은 방어 보정 ("(2+공격)", "(3+방어)")
	if (Recipe.bAoeDamage)
	{
		const int32 Aoe = Skill.SecondaryValue + GetEffectiveStat(Caster, EScalingStat::ATK);
		for (UCombatStatsComponent* O : AllOpponents)
		{
			if (O && !O->IsDead())
			{
				O->ApplyDamage(Aoe, Caster);
			}
		}
	}
	const int32 SecondaryDef = Skill.SecondaryValue + GetEffectiveStat(Caster, EScalingStat::DEF);
	if (Recipe.SecondaryOnCaster == EValueUse::Shield)
	{
		Caster->AddShield(SecondaryDef);
	}
	else if (Recipe.SecondaryOnCaster == EValueUse::Heal)
	{
		Caster->Heal(SecondaryDef);
	}
	if (Recipe.bDrain && DrainHits > 0)
	{
		Caster->Heal(Skill.SecondaryValue * DrainHits);
	}

	// 에너지 회복 (신의 축복 / 아드레날린). 최대치는 안 넘김
	if (Recipe.bGainEnergy)
	{
		Caster->AddEnergy(Amount);
	}

	// ⑤ 상태이상. 컬럼이 None 이 아니면 레시피가 말한 쪽한테
	TArray<UCombatStatsComponent*> StatusReceivers;
	switch (Recipe.StatusTo)
	{
	case EStatusTo::Caster:    StatusReceivers.Add(Caster); break;
	case EStatusTo::Opponents: StatusReceivers = AllOpponents; break;
	default:                   StatusReceivers = Targets; break;
	}

	auto ApplyTo = [&](EStatusEffect Type, int32 Value)
	{
		for (UCombatStatsComponent* T : StatusReceivers)
		{
			if (T && !T->IsDead())
			{
				T->ApplyStatus(ResolveFusion(Type), Value, Skill.StatusDuration);
			}
		}
	};

	if (Skill.StatusEffect != EStatusEffect::None)
	{
		// 반격은 반사 데미지가 SecondaryValue (+ 스텟 보정). Enum_State: "SecondaryValue 만큼의 데미지"
		int32 Value = Skill.StatusValue;
		if (Skill.StatusEffect == EStatusEffect::Counter)
		{
			Value = Skill.SecondaryValue + FMath::RoundToInt(Stat * Skill.ScalingRatio);
		}
		else if (Recipe.bScaleStatus)
		{
			Value = Skill.StatusValue + FMath::RoundToInt(Stat * Skill.ScalingRatio);
		}
		ApplyTo(Skill.StatusEffect, Value);
	}
	if (Recipe.ExtraStatus != EStatusEffect::None)
	{
		ApplyTo(Recipe.ExtraStatus, Skill.StatusValue);
	}
	if (Recipe.bStatusFromBase && Recipe.BaseStatus != EStatusEffect::None && Skill.BaseValue > 0)
	{
		Caster->ApplyStatus(Recipe.BaseStatus, Skill.BaseValue, Skill.StatusDuration);
	}

	// ⑥ 반동. 컬럼이 0 보다 크면 시전자가 받음
	if (Skill.SelfDamage > 0)
	{
		Caster->ApplyDamage(Skill.SelfDamage);
	}
}

int32 FSkillExecutor::PreviewAmount(const FSkillRow& Skill, const UCombatStatsComponent* Caster)
{
	if (!Caster)
	{
		return Skill.BaseValue;
	}

	// Execute 의 ① 과 같은 계산
	const int32 Stat = GetEffectiveStat(Caster, Skill.ScalingStat);
	return Skill.BaseValue + FMath::RoundToInt(Stat * Skill.ScalingRatio);
}

void FSkillExecutor::DescribeValueUse(EActionKind Kind, bool& bOutDamage, bool& bOutShield, bool& bOutHeal, bool& bOutOnCaster)
{
	const FRecipe Recipe = GetRecipe(Kind);
	bOutDamage = Recipe.Use == EValueUse::Damage;
	bOutShield = Recipe.Use == EValueUse::Shield;
	bOutHeal = Recipe.Use == EValueUse::Heal;
	bOutOnCaster = Recipe.bOnCaster;
}

FSkillExecutor::FRecipe FSkillExecutor::GetRecipe(EActionKind Kind)
{
	FRecipe R;

	switch (Kind)
	{
		// 피해 -> 대상, 상태 -> 대상
	case EActionKind::HitEnemy:
	case EActionKind::HitEnemyMulti:
	case EActionKind::HitAllEnemies:
	case EActionKind::HitAllEnemiesMulti:
	case EActionKind::HitEnemyMark:
	case EActionKind::HitEnemyMultiMark:
	case EActionKind::HitEnemyPoison:
	case EActionKind::HitEnemySelfDmg:
	case EActionKind::HitEnemyLava:
	case EActionKind::HitEnemyIce:
	case EActionKind::HitEnemyAcid:
	case EActionKind::HitEnemyFear:
	case EActionKind::HitEnemyInternalInjury:
	case EActionKind::HitEnemyDark:
	case EActionKind::HitEnemyFlooding:
	case EActionKind::HitEnemyContrarywind:
	case EActionKind::HitAllEnemiesMark:
	case EActionKind::HitAllEnemiesLavaAll:
	case EActionKind::HitAllEnemiesIceAll:
	case EActionKind::HitAllEnemiesPoisonAll:
	case EActionKind::HitAllEnemiesAcidAll:
	case EActionKind::HitAllEnemiesFearAll:
	case EActionKind::HitAllEnemiesFusionAll:
	case EActionKind::HitAllEnemiesInternalInjuryAll:
	case EActionKind::HitAllEnemiesDarkAll:
	case EActionKind::HitAllEnemiesFloodingAll:
	case EActionKind::HitAllEnemiesContrarywindAll:
		R.Use = EValueUse::Damage;
		break;

		// 주 대상 피해 + 상대 전체 (Secondary + 공격). 상태는 주 대상
	case EActionKind::HitEnemySingleAoe:
	case EActionKind::HitEnemySingleAoeLava:
	case EActionKind::HitEnemySingleAoeMark:
		R.Use = EValueUse::Damage;
		R.bAoeDamage = true;
		break;

		// 위 + 상대 전체 중독
	case EActionKind::HitEnemySingleAoePoison:
		R.Use = EValueUse::Damage;
		R.bAoeDamage = true;
		R.StatusTo = EStatusTo::Opponents;
		break;

		// 피해 -> 대상, 버프 -> 시전자
	case EActionKind::HitEnemyProtect:
	case EActionKind::HitEnemyEvasionSelf:
		R.Use = EValueUse::Damage;
		R.StatusTo = EStatusTo::Caster;
		break;

		// 피해 + 시전자 보호막 (Secondary + 방어)
	case EActionKind::HitEnemyShield:
	case EActionKind::HitAllEnemiesShield:
		R.Use = EValueUse::Damage;
		R.SecondaryOnCaster = EValueUse::Shield;
		break;

		// 피해 + 시전자 보호막 + 시전자 강철
	case EActionKind::HitEnemyShieldMetal:
	case EActionKind::HitAllEnemiesShieldMetal:
		R.Use = EValueUse::Damage;
		R.SecondaryOnCaster = EValueUse::Shield;
		R.StatusTo = EStatusTo::Caster;
		break;

		// 피해 + 시전자 회복 (Secondary + 방어). 얼음은 대상에게
	case EActionKind::HitEnemyHealSelf:
	case EActionKind::HitAllEnemiesHealSelf:
	case EActionKind::HitEnemyHealSelfIce:
	case EActionKind::HitAllEnemiesHealSelfIceAll:
		R.Use = EValueUse::Damage;
		R.SecondaryOnCaster = EValueUse::Heal;
		break;

		// 반동 + 피해 + 체력을 깎은 타격마다 Secondary 회복 (반동은 SelfDamage 칸)
	case EActionKind::HitEnemySelfDmgDrain:
	case EActionKind::HitEnemyMultiSelfDmgDrain:
	case EActionKind::HitAllSelfDmgDrain:
		R.Use = EValueUse::Damage;
		R.bDrain = true;
		break;

		// 보호막 -> 시전자 (상태는 대상: Self 스킬이면 시전자, 독 연막이면 적 전체)
	case EActionKind::ShieldSelf:
	case EActionKind::ShieldSelfCounter:
	case EActionKind::ShieldSelfPoisonAll:
	case EActionKind::ShieldSelfSolid:
	case EActionKind::ShieldSelfAbsorption:
	case EActionKind::ShieldSelfProtect:
	case EActionKind::ShieldSelfEvasion:
	case EActionKind::ShieldSelfIronWall:
	case EActionKind::ShieldSelfBraveSelf:
		R.Use = EValueUse::Shield;
		R.bOnCaster = true;
		break;

		// 보호막 -> 대상
	case EActionKind::ShieldAll:
	case EActionKind::ShieldAllSolidAll:
		R.Use = EValueUse::Shield;
		break;

		// 회복 -> 대상
	case EActionKind::HealAlly:
	case EActionKind::HealAll:
	case EActionKind::HealAllBraveAll:
		R.Use = EValueUse::Heal;
		break;

		// 회복 -> 시전자 (유물)
	case EActionKind::HealSelf:
	case EActionKind::HealSelfProtect:
	case EActionKind::HealSelfBraveSelf:
		R.Use = EValueUse::Heal;
		R.bOnCaster = true;
		break;

		// 에너지 회복 (+ 자신 버프)
	case EActionKind::GainEnergy:
	case EActionKind::GainEnergyProtectSelf:
	case EActionKind::GainEnergyIndomitableSelf:
	case EActionKind::GainEnergyBraveSelf:
	case EActionKind::GainEnergyBlindnessSelf:
		R.bGainEnergy = true;
		break;

		// 은신: (StatusValue + 방어) 만큼 민첩
	case EActionKind::EvasionSelf:
		R.bScaleStatus = true;
		break;

		// 시전자 민첩(BaseValue) + 적 전체 중독
	case EActionKind::EvasionSelfPoisonAll:
		R.bStatusFromBase = true;
		R.BaseStatus = EStatusEffect::Evasion;
		break;

		// 상태 두 개를 같은 수치로
	case EActionKind::BraveAllSolidAll:
		R.ExtraStatus = EStatusEffect::Solid;
		break;
	case EActionKind::EvasionSelfBraveSelf:
	case EActionKind::EvasionSelfCounterSelf:
		R.ExtraStatus = EStatusEffect::Evasion;
		break;
	case EActionKind::StatusMarkFearAll:
		R.ExtraStatus = EStatusEffect::Fear;
		break;

		// 나머지(상태만 거는 것, 반동만, 유물 전용)는 BaseValue 를 안 씀. 상태 / 반동은 컬럼이 따로 말해줌
	default:
		break;
	}

	return R;
}

int32 FSkillExecutor::GetEffectiveStat(const UCombatStatsComponent* Who, EScalingStat Stat)
{
	const FCharacterStats& Base = Who->GetStats();

	switch (Stat)
	{
	case EScalingStat::ATK:
		// 얼음 그룹 = 허약(얼음 / 침수)
		return Base.Attack
			+ Who->GetStatusValue(EStatusEffect::Brave)
			- Who->GetStatusValue(EStatusEffect::Ice);

	case EScalingStat::DEF:
		// 가호와 견고는 같은 그룹이라 하나만 걸림 -> 한 번만 더함
		return Base.Defense
			+ Who->GetStatusValue(EStatusEffect::Protection)
			- Who->GetStatusValue(EStatusEffect::Lava);

	default:
		return 0;
	}
}

EStatusEffect FSkillExecutor::ResolveFusion(EStatusEffect Type)
{
	if (Type != EStatusEffect::Fusion)
	{
		return Type;
	}

	// 퓨전은 중독 용암 얼음 중 하나. 컴포넌트가 퓨전을 거절하니까 여기서 바꿔서 넘김
	switch (FMath::RandRange(0, 2))
	{
	case 0:  return EStatusEffect::Poison;
	case 1:  return EStatusEffect::Lava;
	default: return EStatusEffect::Ice;
	}
}