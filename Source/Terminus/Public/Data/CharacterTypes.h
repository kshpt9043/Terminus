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
	Monk     UMETA(DisplayName = "무도가"),
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
	Religion         UMETA(DisplayName = "종교 (이름 미정)"),
	Empire           UMETA(DisplayName = "황실")
};

USTRUCT(BlueprintType)
struct FCharacterClassRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// 캐릭터 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECharacterClass Class = ECharacterClass::Monk;

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
