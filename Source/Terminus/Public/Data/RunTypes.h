#pragma once

#include "CoreMinimal.h"
#include "Data/CharacterTypes.h"
#include "Data/StatTypes.h"
#include "RunTypes.generated.h"

// 일시 스탯 버프 (이벤트 방). 전투가 시작될 때 상태로 걸리고(공격 = 용기, 방어 감소 = 용암), 전투가 끝날 때마다 1 줄어듦
USTRUCT(BlueprintType)
struct FTempStatBuff
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Attack = 0;
	UPROPERTY(BlueprintReadOnly) int32 Defense = 0;        // 음수면 방어 감소
	UPROPERTY(BlueprintReadOnly) int32 BattlesLeft = 0;
	UPROPERTY(BlueprintReadOnly) FString Source;           // 어디서 받았는지 (표시용)
};

// 런이 끝날 때 서버가 정산에 더하거나 빼라고 알려 주는 것 (배신 결과)
USTRUCT(BlueprintType)
struct FSettlementAdjust
{
	GENERATED_BODY()

	// 정산 유물 후보에 더함 (배신 성공: 동료들의 보스 유물)
	UPROPERTY(BlueprintReadOnly) TArray<FName> ExtraRelics;

	// 정산에서 뺌 (배신에서 진 쪽: 보스전에서 얻은 유물 / 스킬)
	UPROPERTY(BlueprintReadOnly) TArray<FName> LostRelics;
	UPROPERTY(BlueprintReadOnly) TArray<FName> LostSkills;

	// 추가 골드와 그 이유 (배신자 패배: 배신자 보스 유물 판매 분배)
	UPROPERTY(BlueprintReadOnly) int32 BonusGold = 0;
	UPROPERTY(BlueprintReadOnly) FString BonusReason;
};

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

	// 장착한 강화 스킬 (DT_Skill 행 이름). 스킬 에너지로 씀. 최대 Stats.EnhanceSlots 칸
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> EnhanceSkills;

	// 런 시작 때 강화 스킬 고르기를 마쳤는지. 보유 스킬이 없어 건너뛴 경우도 true
	// false 인 동안은 지도에서 방을 못 고름
	UPROPERTY(BlueprintReadOnly)
	bool bStartSkillChosen = false;

	// 런 시작 때 창고 유물 고르기를 마쳤는지 (안 고르고 시작해도 true). false 인 동안은 방을 못 고름
	UPROPERTY(BlueprintReadOnly)
	bool bStartRelicsChosen = false;

	// 던전에 들어갈 때 창고에서 들고 간 유물. 정산에서 고르는 후보에서 뺌 (사용자 결정 10-08)
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> StartRelics;

	// 이번 층 보스방 보상으로 받은 유물 / 스킬 (배신 결과에 씀: 진 쪽은 이걸 못 가져감). 층이 바뀌면 비움
	UPROPERTY(BlueprintReadOnly)
	FName BossRelic;

	UPROPERTY(BlueprintReadOnly)
	FName BossSkill;

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

	// 이번 층에서 지나온 방들 (들어간 순서). 지도에 지나온 길을 진하게 그릴 때 씀. 층이 바뀌면 비움 (BeginFloor)
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> VisitedRoomIds;

	// 훈련소 기본 스킬 강화 단계 (0~2 칸). 런 시작 때 프로필에서 복사. 쓸 때 BaseValue(없으면 StatusValue)에 더함
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> BasicSkillLevels;

	// 이번 런에서 나한테 한 번이라도 나온 유물 (얻은 것 + 보상 / 이벤트 / 상점 후보로 보였던 것 + 판 것)
	// 사용자 결정 2026-10-06: 한 번 나온 유물은 그 런 동안 다시 안 나옴. 멀티는 사람마다 따로
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> SeenRelics;

	// 일시 스탯 버프 (이벤트 방). 남은 전투 수가 0 이 되면 사라짐
	UPROPERTY(BlueprintReadOnly)
	TArray<FTempStatBuff> TempBuffs;

	// 세이브에서 이어할 때의 체력. 던전에서 배틀러가 처음 빙의될 때 적용하고 -1 로 비움 (-1 = 최대 체력으로 시작)
	UPROPERTY(BlueprintReadOnly)
	int32 SavedHealth = -1;
};
