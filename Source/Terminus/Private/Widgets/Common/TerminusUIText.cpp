#include "Widgets/Common/TerminusUIText.h"

#include "Data/RelicTypes.h"
#include "Data/SkillTypes.h"
#include "Map/MapManager.h"
#include "Widgets/TerminusUIColors.h"

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
		// 스타일 가이드 층 등급: 표층 흐림 / 중층 금 / 심층 경고
		case ERelicTier::Basic:   return Color::Line;
		case ERelicTier::Upgrade: return Color::GoldHover;
		case ERelicTier::Mid:     return Color::GoldBase;
		case ERelicTier::Deep:    return Color::Alert;
		default:                  return Color::TextFaint;
		}
	}

	FLinearColor SkillTypeColor(ESkillType Type)
	{
		switch (Type)
		{
		// 팔레트 안에서: 공격 경고색 / 방어 보조 행동 청록 / 특수 금
		case ESkillType::Attack:  return Color::Alert;
		case ESkillType::Defense: return Hex(TEXT("4A929A"));
		default:                  return Color::GoldBase;
		}
	}
}
