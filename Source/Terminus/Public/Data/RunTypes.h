#pragma once

#include "CoreMinimal.h"
#include "Data/CharacterTypes.h"
#include "RunTypes.generated.h"

// 던전을 돌 동안 유지되는 데이터 구조체
// 맵을 넘어가도 살아야함

USTRUCT(BlueprintType)
struct FRunState
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	ECharacterClass CharacterClass = ECharacterClass::Monk;
};
