// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapGenerator.generated.h"

UENUM(BlueprintType)
enum class ERoomType : uint8
{
	MONSTER UMETA(DisplayName = "MONSTER"),
	BREAK UMETA(DisplayName = "BREAK"),
	STORE UMETA(DisplayName = "STORE"),
	GUARDIAN UMETA(DisplayName = "GUARDIAN"),
	EVENT UMETA(DisplayName = "EVENT"),
	BOSS UMETA(DisplayName = "BOSS")
};

USTRUCT(BlueprintType)
struct FRoomNode
{
	GENERATED_BODY()
    
	UPROPERTY(BlueprintReadWrite)
	int32 RoomId = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Row = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Col = 0;

	UPROPERTY(BlueprintReadWrite)
	ERoomType Type = ERoomType::MONSTER;
	
	UPROPERTY(BlueprintReadWrite)
	int32 MaxPlayers = 1;

	UPROPERTY(BlueprintReadWrite)
	TArray<int32> ConnectedRoomIds;
};

// 맵 데이터가 업데이트되었음을 UI 등에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMapGenerated, const TArray<FRoomNode>&, MapData);

UCLASS()
class TERMINUS_API AMapGenerator : public AActor
{
	GENERATED_BODY()
    
public: 
	AMapGenerator();

	UPROPERTY(ReplicatedUsing = OnRep_Rooms, BlueprintReadOnly, Category = "Map")
	TArray<FRoomNode> Rooms;

	UPROPERTY(BlueprintAssignable, Category = "Map")
	FOnMapGenerated OnMapGenerated;

	// 현재 게임 멀티플레이 참여 인원수 (서버에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 CurrentPlayerCount = 4;

	// 일반 방 비율 설정 (기본값: 몬스터 55%, 휴식 10%, 상점 10%, 가디언 10%, 이벤트 15%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	TMap<ERoomType, float> RoomTypeWeights;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Rooms();

public: 
	UFUNCTION(BlueprintCallable, Category = "Map")
	TArray<FRoomNode> GenerateMap();

private:
	FRoomNode CreateRoom(int32 RoomId, int32 Row, int32 Col, ERoomType Type = ERoomType::MONSTER, int32 MaxPlayers = 1);
    
	// 비율 기반 랜덤 방 타입 추첨 함수
	ERoomType GetWeightedRandomRoomType();

	// 1레벨 -> 보스방 경로 존재 검증 (BFS)
	bool ValidatePathToBoss(const TArray<FRoomNode>& InMap);

	// 몬스터/가디언 방 분기 시 입장 인원 합산 조건 검증
	bool ValidatePlayerCapacity(const TArray<FRoomNode>& InMap);
};