#pragma once

#include "CoreMinimal.h"
#include "StatTypes.generated.h"

// 캐릭터 스텟을 최대치와 현재치로 나눠서 담는 구조체들
// 기획안 데모의 플레이어 > 스테이터스 항목 기준
//
// 한 구조체에 안 몰아넣은 이유
// 1. 성격이 다름 -> 최대치는 클래스가 정하는 정의, 현재치는 전투 중에만 사는 상태
// 2. 복제 비용 -> 섞으면 체력 1 깎일 때마다 MaxHealth 랑 EnhanceSlots 까지 같이 날아감
// 3. FCharacterStats 는 FRunState 로 가야 하고 FCombatState 는 전투 컴포넌트에만 있으면 됨

/**
 * 캐릭터 클래스가 정하는 고정 스텟. 전투 중엔 안 바뀐다.
 * 나중에 DT_CharacterClass 행에 그대로 얹을 값들이라 EditAnywhere 로 둠.
 */
USTRUCT(BlueprintType)
struct FCharacterStats
{
	GENERATED_BODY()

	// 기본값은 무도가 기준
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxHealth = 70;

	// 공격 스킬 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Attack = 0;

	// 보호막 생성과 체력 회복 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Defense = 0;

	// 체술 효과 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Evasion = 0;

	// 기본 스킬용 에너지 상한. 매 턴 이 값까지 회복하고 턴 끝나면 0이 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxEnergy = 3;

	// 획득 스킬용 자원의 상한. 시작 보유량은 0이라 여기 최대치만 들어감
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxSkillEnergy = 2;

	// 강화 스킬을 낄 수 있는 칸 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 EnhanceSlots = 3;
};

/**
 * 전투 중에만 사는 현재값. 전투가 끝나면 버린다.
 * 서버가 바꾸고 클라는 받기만 할 거라 BlueprintReadOnly.
 */
USTRUCT(BlueprintType)
struct FCombatState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Health = 0;

	// 피해를 먼저 먹고 사라짐. 기획서 스테이터스엔 없는데 전투 상태로는 필요함
	UPROPERTY(BlueprintReadOnly)
	int32 Shield = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Energy = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SkillEnergy = 0;

	// 에너지를 2회 쓸 때마다 스킬 에너지 1 회복 -> 나머지를 들고 있어야 해서 카운터가 따로 필요
	UPROPERTY(BlueprintReadOnly)
	int32 EnergySpentCounter = 0;

	// 사이클 한정으로 올라가는 회피. 암살자 은신이랑 독 연막이 여기를 건드림
	UPROPERTY(BlueprintReadOnly)
	int32 BonusEvasion = 0;
};
