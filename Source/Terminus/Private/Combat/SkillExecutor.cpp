#include "Combat/SkillExecutor.h"
#include "Combat/CombatStatsComponent.h"

void FSkillExecutor::Execute(const FSkillRow& Skill,
							 UCombatStatsComponent* Caster,
							 const TArray<UCombatStatsComponent*>& Targets)
{
	if (!Caster)
	{
		return;
	}

	// ① 최종 수치. 스텟에 딱지까지 합산해서 Ratio 곱함
	const int32 Stat = GetEffectiveStat(Caster, Skill.ScalingStat);
	const int32 Amount = Skill.BaseValue + FMath::RoundToInt(Stat * Skill.ScalingRatio);

	// ② BaseValue 를 뭘로 누구한테 쓸지
	const FRecipe Recipe = GetRecipe(Skill.ActionKind);

	TArray<UCombatStatsComponent*> Receivers = Targets;
	if (Recipe.bOnCaster)
	{
		Receivers.Reset();
		Receivers.Add(Caster);
	}

	// ③ BaseValue 적용
	for (UCombatStatsComponent* R : Receivers)
	{
		if (!R)
		{
			continue;
		}

		switch (Recipe.Use)
		{
		case EValueUse::Damage:
			for (int32 i = 0; i < Skill.HitCount; ++i)
			{
				R->ApplyDamage(Amount);
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

	// ④ 상태이상. 컬럼이 None 이 아니면 대상들한테
	if (Skill.StatusEffect != EStatusEffect::None)
	{
		for (UCombatStatsComponent* T : Targets)
		{
			if (T)
			{
				T->ApplyStatus(ResolveFusion(Skill.StatusEffect), Skill.StatusValue, Skill.StatusDuration);
			}
		}
	}

	// ⑤ 반동. 컬럼이 0 보다 크면 시전자가 받음
	if (Skill.SelfDamage > 0)
	{
		Caster->ApplyDamage(Skill.SelfDamage);
	}
}

FSkillExecutor::FRecipe FSkillExecutor::GetRecipe(EActionKind Kind)
{
	FRecipe R;

	switch (Kind)
	{
		// 피해 -> 대상
	case EActionKind::HitEnemy:
	case EActionKind::HitEnemyMulti:
	case EActionKind::HitEnemySingleAoe:
	case EActionKind::HitAllEnemies:
	case EActionKind::HitEnemyMark:
	case EActionKind::HitEnemyPoison:
	case EActionKind::HitEnemySelfDmg:
	case EActionKind::HitEnemyShield:
	case EActionKind::HitEnemyProtect:
	case EActionKind::HitAllEnemiesLavaAll:
	case EActionKind::HitAllEnemiesIceAll:
	case EActionKind::HitAllEnemiesPoisonAll:
	case EActionKind::HitAllEnemiesAcidAll:
	case EActionKind::HitAllEnemiesFearAll:
	case EActionKind::HitAllEnemiesFusionAll:
		R.Use = EValueUse::Damage;
		break;

		// 보호막 -> 시전자
	case EActionKind::ShieldSelf:
	case EActionKind::ShieldSelfCounter:
	case EActionKind::ShieldSelfPoisonAll:
		R.Use = EValueUse::Shield;
		R.bOnCaster = true;
		break;

		// 보호막 -> 대상
	case EActionKind::ShieldAll:
		R.Use = EValueUse::Shield;
		break;

		// 회복 -> 대상
	case EActionKind::HealAlly:
	case EActionKind::HealAll:
	case EActionKind::HealAllBraveAll:
		R.Use = EValueUse::Heal;
		break;

		// 나머지 22개는 BaseValue 를 안 씀. 상태이상 반동은 컬럼이 따로 말해줌
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
		return Base.Attack
			+ Who->GetStatusValue(EStatusEffect::Brave)
			- Who->GetStatusValue(EStatusEffect::Ice);

	case EScalingStat::DEF:
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