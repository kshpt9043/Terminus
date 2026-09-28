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

	// Stats 가 이번 런용으로 채워졌는지. 주점 출발(BeginRun) 때 true
	// false 면 주점을 안 거친 것(던전 맵 바로 PIE) -> 배틀러가 클래스 기본 스텟을 씀
	UPROPERTY(BlueprintReadOnly)
	bool bStatsInitialized = false;
	
	// 맵 진행 및 선택 관련 추가 데이터
	UPROPERTY(BlueprintReadWrite)
	int32 CurrentRoomId = -1; // 방금 클리어하고 나온 방 ID (-1: 시작 전)

	UPROPERTY(BlueprintReadWrite)
	int32 CurrentMapLevel = 0; // 현재 진행 레벨 (Row 0 ~ 11)

	UPROPERTY(BlueprintReadWrite)
	int32 SelectedRoomId = -1; // 선택한 다음 방 ID (-1: 선택 안 함)
};
