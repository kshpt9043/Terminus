#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Map/MapManager.h"
#include "DungeonThemeData.generated.h"

class ADungeonAreaSet;

// TMap 값에 배열을 바로 못 넣어서 감싸는 구조체
USTRUCT(BlueprintType)
struct FDungeonAreaSetList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TArray<TSubclassOf<ADungeonAreaSet>> Sets;
};

/**
 * 던전 테마 하나 (기획서의 테마. 예: 슬라임 왕국).
 * 이 테마의 방들이 어떤 무대(구역 세트)에서 펼쳐지는지 정한다
 *
 * 테마 추가 = 이 데이터 에셋 하나 + 구역 세트 BP 몇 개. 코드 수정 없음
 */
UCLASS(BlueprintType)
class TERMINUS_API UDungeonThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 테마 이름. 몬스터 DT 의 Theme_KR 과 같은 글자로 맞춰 둘 것 (나중에 몬스터 스폰이 이걸로 거름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FText DisplayName;

	// 이 테마의 기본 무대. 여러 개 넣으면 방마다 랜덤 -> 같은 테마라도 방마다 조금씩 다르게 보임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TArray<TSubclassOf<ADungeonAreaSet>> AreaSets;

	// 방 타입별로 다른 무대를 쓰고 싶을 때만 (예: 상점, 휴식터). 없는 타입은 AreaSets 에서 고름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TMap<ERoomType, FDungeonAreaSetList> RoomTypeAreaSets;

	// 이 방 타입에 쓸 무대 하나. 후보가 없으면 nullptr
	TSubclassOf<ADungeonAreaSet> PickAreaSet(ERoomType RoomType) const;
};
