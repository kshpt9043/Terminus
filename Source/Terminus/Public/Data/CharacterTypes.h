#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/StatTypes.h"
#include "CharacterTypes.generated.h"

class UTexture2D;
class UPaperZDAnimInstance;

UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
	Fighter  UMETA(DisplayName = "무도가"),
	Engineer UMETA(DisplayName = "마도 공학자"),
	Paladin  UMETA(DisplayName = "성기사"),
	Assassin UMETA(DisplayName = "암살자"),

	// 나중에 클래스 늘어날 때 반복문 인덱스 동적으로 늘게 하기 위해서
	MAX      UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EFaction : uint8
{
	AdventurersGuild UMETA(DisplayName = "모험가 길드"),
	MageTower        UMETA(DisplayName = "마탑"),
	Religion         UMETA(DisplayName = "테르미누스"),
	Empire           UMETA(DisplayName = "황실")
};

// 화면에 띄울 세력 이름
// UMETA DisplayName 은 에디터 전용이라 패키징하면 코드 이름이 나옴 -> 위에 열거형 번역본임
inline FText GetFactionName(EFaction InFaction)
{
	switch (InFaction)
	{
	case EFaction::AdventurersGuild: return FText::FromString(TEXT("모험가 길드"));
	case EFaction::MageTower:        return FText::FromString(TEXT("마탑"));
	case EFaction::Religion:         return FText::FromString(TEXT("테르미누스"));   // 종교 세력 (기획 거점 시안에서 확정)
	case EFaction::Empire:           return FText::FromString(TEXT("황실"));
	default:                         return FText::GetEmpty();
	}
}

USTRUCT(BlueprintType)
struct FCharacterClassRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// 캐릭터 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECharacterClass Class = ECharacterClass::Fighter;

	// 캐릭터 표시 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 캐릭터 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Description;

	// 패시브 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText PassiveText;

	// 일러스트 -> 하드 참조시 일러스트 불필요하게 메모리 참조함
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Illustration;
	
	// 소속 세력
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFaction Faction = EFaction::AdventurersGuild;

	// 강화 전 기본 스텟. 던전에서 늘어난 값은 FRunState가 들고 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCharacterStats BaseStats;
	
	// ZD 애님블프.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<UPaperZDAnimInstance> AnimInstanceClass;
};
