#pragma once

#include "CoreMinimal.h"
#include "Data/CharacterTypes.h"
#include "Data/StatTypes.h"
#include "RunTypes.generated.h"

// 던전을 돌 동안 유지되는 데이터 구조체
// 맵을 넘어가도 살아야함

USTRUCT(BlueprintType)
struct FRunState
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	ECharacterClass CharacterClass = ECharacterClass::Fighter;
	
	UPROPERTY(BlueprintReadOnly)
	FCharacterStats Stats;
	
	// 맵 진행 및 선택 관련 추가 데이터
	UPROPERTY(BlueprintReadWrite)
	int32 CurrentRoomId = -1; // 방금 클리어하고 나온 방 ID (-1: 시작 전)

	UPROPERTY(BlueprintReadWrite)
	int32 CurrentMapLevel = 0; // 현재 진행 레벨 (Row 0 ~ 11)

	UPROPERTY(BlueprintReadWrite)
	int32 SelectedRoomId = -1; // 선택한 다음 방 ID (-1: 선택 안 함)
};
