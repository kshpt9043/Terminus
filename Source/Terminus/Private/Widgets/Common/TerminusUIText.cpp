#include "Widgets/Common/TerminusUIText.h"

#include "Data/RelicTypes.h"
#include "Data/SkillTypes.h"
#include "Map/MapManager.h"

namespace TerminusUI
{
	FString SkillTypeLabel(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return TEXT("공격");
		case ESkillType::Defense: return TEXT("방어");
		default:                  return TEXT("특수");
		}
	}

	FString TargetTypeLabel(ETargetType Type)
	{
		switch (Type)
		{
		case ETargetType::SingleEnemy: return TEXT("적 1명");
		case ETargetType::AllEnemies:  return TEXT("적 전체");
		case ETargetType::SingleAlly:  return TEXT("아군 1명");
		case ETargetType::AllAllies:   return TEXT("아군 전체");
		default:                       return TEXT("자신");
		}
	}

	FString SkillOwnerLabel(ESkillOwner Owner)
	{
		switch (Owner)
		{
		case ESkillOwner::Fighter:  return TEXT("무도가");
		case ESkillOwner::Engineer: return TEXT("마도 공학자");
		case ESkillOwner::Paladin:  return TEXT("성기사");
		case ESkillOwner::Assassin: return TEXT("암살자");
		default:                    return TEXT("공용");
		}
	}

	FString RelicTierLabel(ERelicTier Tier)
	{
		switch (Tier)
		{
		case ERelicTier::Basic:   return TEXT("기본");
		case ERelicTier::Upgrade: return TEXT("업그레이드");
		case ERelicTier::Mid:     return TEXT("중층");
		case ERelicTier::Deep:    return TEXT("심층");
		default:                  return TEXT("표층");
		}
	}

	FString RoomTypeLabel(ERoomType Type)
	{
		switch (Type)
		{
		case ERoomType::MONSTER:  return TEXT("몬스터");
		case ERoomType::GUARDIAN: return TEXT("가디언");
		case ERoomType::BOSS:     return TEXT("보스");
		case ERoomType::BREAK:    return TEXT("휴식터");
		case ERoomType::EVENT:    return TEXT("이벤트");
		case ERoomType::STORE:    return TEXT("상점");
		case ERoomType::QUEST:    return TEXT("퀘스트");
		default:                  return TEXT("방");
		}
	}

	FString GoldText(int32 Gold)
	{
		return FText::AsNumber(Gold).ToString() + TEXT(" G");
	}

	FLinearColor RelicTierColor(ERelicTier Tier)
	{
		switch (Tier)
		{
		case ERelicTier::Basic:   return FLinearColor(0.55f, 0.55f, 0.55f);
		case ERelicTier::Upgrade: return FLinearColor(1.f, 0.8f, 0.3f);
		case ERelicTier::Mid:     return FLinearColor(0.35f, 0.6f, 1.f);
		case ERelicTier::Deep:    return FLinearColor(0.95f, 0.3f, 0.3f);
		default:                  return FLinearColor(0.85f, 0.85f, 0.85f);
		}
	}

	FLinearColor SkillTypeColor(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return FLinearColor(0.9f, 0.35f, 0.3f);
		case ESkillType::Defense: return FLinearColor(0.35f, 0.6f, 0.95f);
		default:                  return FLinearColor(0.7f, 0.45f, 0.95f);
		}
	}
}
