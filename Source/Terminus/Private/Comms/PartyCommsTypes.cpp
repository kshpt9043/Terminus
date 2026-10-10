#include "Comms/PartyCommsTypes.h"

#include "Combat/CombatStatsComponent.h"

#define LOCTEXT_NAMESPACE "PartyComms"

FText PartyComms::GetQuickChatText(EQuickChat Kind)
{
	switch (Kind)
	{
	case EQuickChat::Good:   return LOCTEXT("QuickGood", "좋아요!");
	case EQuickChat::Thanks: return LOCTEXT("QuickThanks", "고마워요!");
	case EQuickChat::Wait:   return LOCTEXT("QuickWait", "잠깐만요!");
	case EQuickChat::Sorry:  return LOCTEXT("QuickSorry", "미안해요!");
	case EQuickChat::Help:   return LOCTEXT("QuickHelp", "도와주세요!");
	case EQuickChat::Go:     return LOCTEXT("QuickGo", "가자!");
	default:                 return FText::GetEmpty();
	}
}

FText PartyComms::GetPingText(EPingKind Kind)
{
	switch (Kind)
	{
	case EPingKind::Focus:     return LOCTEXT("PingFocus", "집중 공격");
	case EPingKind::Danger:    return LOCTEXT("PingDanger", "위험");
	case EPingKind::NeedGuard: return LOCTEXT("PingGuard", "보호 필요");
	case EPingKind::NeedHeal:  return LOCTEXT("PingHeal", "회복 필요");
	default:                   return FText::GetEmpty();
	}
}

FLinearColor PartyComms::GetPingColor(EPingKind Kind)
{
	switch (Kind)
	{
	case EPingKind::Focus:     return FLinearColor(1.f, 0.35f, 0.25f);
	case EPingKind::Danger:    return FLinearColor(1.f, 0.8f, 0.2f);
	case EPingKind::NeedGuard: return FLinearColor(0.4f, 0.75f, 1.f);
	case EPingKind::NeedHeal:  return FLinearColor(0.4f, 1.f, 0.5f);
	default:                   return FLinearColor::White;
	}
}

bool PartyComms::IsEnemyPing(EPingKind Kind)
{
	return Kind == EPingKind::Focus || Kind == EPingKind::Danger;
}

int32 PartyComms::EstimateHit(int32 Amount, const UCombatStatsComponent* Caster, const UCombatStatsComponent* Target)
{
	if (Amount <= 0) return 0;

	if (Caster && Caster->HasStatus(EStatusEffect::Fear))
	{
		Amount = FMath::RoundToInt(Amount * 0.75f);
	}
	if (Target && Target->HasStatus(EStatusEffect::Mark))
	{
		Amount = FMath::RoundToInt(Amount * 1.25f);
	}
	if (Target && Amount > 0 && (Target->HasStatus(EStatusEffect::Indomitable) || Target->HasStatus(EStatusEffect::Metal)))
	{
		Amount = 1;
	}
	if (Target)
	{
		Amount = FMath::Max(0, Amount - Target->GetStatusValue(EStatusEffect::IronWall));
	}
	return Amount;
}

#undef LOCTEXT_NAMESPACE
