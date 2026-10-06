#pragma once

#include "CoreMinimal.h"
#include "Map/MapManager.h"
#include "RewardTypes.generated.h"

/**
 * 방 클리어 보상 (서버 -> 플레이어 한 명). 사용자 결정 2026-10-06
 *  - 몬스터방: 던전 재화 + 픽업 스킬(스킬 에너지를 쓰는 스킬) 후보 중 하나
 *  - 가디언방: 유물 하나 (공용이거나 내 직업, 지금 계층 등급)
 *  - 보스방: 유물 하나 (같은 조건) + 픽업 스킬 하나
 * 스킬 / 유물은 각각 받거나 안 받을 수 있음. 칸이 꽉 차면 바꿀 칸을 고름
 */
USTRUCT(BlueprintType)
struct FRoomRewardOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ERoomType RoomType = ERoomType::MONSTER;

	// 이미 받은 던전 재화 (보여주기만)
	UPROPERTY(BlueprintReadOnly)
	int32 Currency = 0;

	// 픽업 스킬 후보 (하나만 고름)
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> SkillOffers;

	// 유물 후보 (하나만 고름)
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> RelicOffers;
};
