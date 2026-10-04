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

	// 장착한 강화 스킬 (DT_Skill 행 이름). 강화 에너지로 씀. 최대 Stats.EnhanceSlots 칸
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> EnhanceSkills;

	// 런 시작 때 강화 스킬 고르기를 마쳤는지. 보유 스킬이 없어 건너뛴 경우도 true
	// false 인 동안은 지도에서 방을 못 고름
	UPROPERTY(BlueprintReadOnly)
	bool bStartSkillChosen = false;

	// 런 시작 때 창고 유물 고르기를 마쳤는지 (안 고르고 시작해도 true). false 인 동안은 방을 못 고름
	UPROPERTY(BlueprintReadOnly)
	bool bStartRelicsChosen = false;

	// 하드 모드 (판 전체 규칙, 주점에서 방장이 정함). 켜면 런 시작 때 들고 간 창고 유물이 창고에서 사라짐
	// 파티 전원의 RunState 에 같은 값이 들어감. 런을 새로 시작해도 유지 (BeginRun 이 안 지움)
	UPROPERTY(BlueprintReadOnly)
	bool bHardMode = false;

	// 던전 재화. 런 동안만 유효 (몬스터방 보상으로 얻고 상점에서 씀)
	UPROPERTY(BlueprintReadOnly)
	int32 Currency = 0;

	// 보유 유물 (DT_Relic 행 이름). 처음엔 직업 기본 유물(RLC_<직업>_001) 하나
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> Relics;
	
	// 맵 진행 및 선택 관련 추가 데이터
	UPROPERTY(BlueprintReadWrite)
	int32 CurrentRoomId = -1; // 방금 클리어하고 나온 방 ID (-1: 시작 전)

	UPROPERTY(BlueprintReadWrite)
	int32 CurrentMapLevel = 0; // 현재 진행 레벨 (Row 0 ~ 11)

	UPROPERTY(BlueprintReadWrite)
	int32 SelectedRoomId = -1; // 선택한 다음 방 ID (-1: 선택 안 함)

	// 이번 층에서 지나온 방들 (들어간 순서). 지도에 지나온 길을 진하게 그릴 때 씀
	// TODO: 층을 내려갈 때(새 지도) 비울 것
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> VisitedRoomIds;
};
