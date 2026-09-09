#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CharacterTypes.generated.h"

class UTexture2D;

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
};
