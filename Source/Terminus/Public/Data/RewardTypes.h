#pragma once

#include "CoreMinimal.h"
#include "Map/MapManager.h"
#include "RewardTypes.generated.h"

// 이벤트 방 후보 종류 (기획 '이벤트 방 밸런싱'. 수치는 기획 미정이라 임시)
UENUM(BlueprintType)
enum class EEventOptionType : uint8
{
	ClassSkill,        // 내 직업 / 공용 픽업 스킬
	OtherClassSkill,   // 다른 직업 픽업 스킬
	EventSkill,        // 이벤트 풀 스킬
	Currency,          // 던전 재화
	PermanentStat,     // 이번 런 동안 스탯 상승
	TempStat,          // 다음 전투 몇 번 동안 스탯 상승 (+ 디버프)
	ThemeTempStat,     // 테마 이벤트: 일시 스탯 상승 (공용보다 큼)
	RelicSwapRandom,   // 내 유물 하나(랜덤)를 같은 계층 다른 유물로
	RelicSwapChosen    // 내가 고른 유물 하나를 같은 계층 다른 유물로
};

// 이벤트 후보 하나. 서버가 만들어 보내고, 화면은 Title / Description 을 그대로 보여줌
USTRUCT(BlueprintType)
struct FEventOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EEventOptionType Type = EEventOptionType::Currency;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Description;

	// 스킬 후보면 그 스킬
	UPROPERTY(BlueprintReadOnly) FName SkillRow;

	// 재화 / 스탯 수치. 스탯 종류는 StatKind (0 = 체력, 1 = 공격, 2 = 방어)
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) int32 StatKind = 0;

	// 일시 버프: 방어 감소 / 전투 수
	UPROPERTY(BlueprintReadOnly) int32 Penalty = 0;
	UPROPERTY(BlueprintReadOnly) int32 Battles = 0;
};

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
