#pragma once

#include "CoreMinimal.h"

enum class ESkillType : uint8;
enum class ETargetType : uint8;
enum class ESkillOwner : uint8;
enum class ERelicTier : uint8;
enum class ERoomType : uint8;

/**
 * 화면에 보이는 이름 / 라벨 / 종류 색. 여러 위젯에 같은 switch 가 따로 있던 것을 모음
 * 용어를 바꿀 땐 여기만 고치면 됨
 * (스킬 / 유물 이름 찾기는 UTerminusDataSettings::GetSkillName / GetRelicName)
 */
namespace TerminusUI
{
	TERMINUS_API FString SkillTypeLabel(ESkillType Type);     // 공격 / 방어 / 특수
	TERMINUS_API FString TargetTypeLabel(ETargetType Type);   // 적 1명 / 적 전체 / ...
	TERMINUS_API FString SkillOwnerLabel(ESkillOwner Owner);  // 무도가 / ... / 공용
	TERMINUS_API FString RelicTierLabel(ERelicTier Tier);     // 기본 / 업그레이드 / 표층 / 중층 / 심층
	TERMINUS_API FString RoomTypeLabel(ERoomType Type);       // 몬스터 / 가디언 / 보스 / 휴식터 / ...

	// "1,234 G"
	TERMINUS_API FString GoldText(int32 Gold);

	// 칸 테두리 색: 유물은 등급, 스킬은 공격 / 방어 / 특수
	TERMINUS_API FLinearColor RelicTierColor(ERelicTier Tier);
	TERMINUS_API FLinearColor SkillTypeColor(ESkillType Type);
}
