#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/MapManager.h"
#include "DungeonArea.generated.h"

class UCameraComponent;
class ATerminusPlayerState;

/**
 * 던전 구역. 지도의 방 하나가 실제로 펼쳐지는 무대.
 *
 * 멀티에서 파티가 서로 다른 방에 들어갈 수 있어서, 레벨 하나에 구역을 여러 개(최대 인원 수만큼) 두고
 * 방마다 구역 하나를 배정한다. 같은 방을 고른 사람들은 같은 구역에 들어감.
 * 전투 방만 쓰는 게 아니라 상점/휴식터/이벤트 방도 구역을 하나 받는다
 *
 * 방(FRoomNode) = 지도 위의 칸, 구역(DungeonArea) = 그 방이 펼쳐지는 장소
 *
 * 배정/종료 관리는 UDungeonAreaSubsystem 이 함. 구역은 자기 무대만 책임짐
 */
UCLASS()
class TERMINUS_API ADungeonArea : public AActor
{
	GENERATED_BODY()

public:
	ADungeonArea();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 구역 번호. 배정은 번호가 작은 구역부터. 레벨에 놓을 때 0 ~ 3 으로 겹치지 않게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	int32 AreaIndex = 0;

	// -------------------------------------------------------------
	// [서버 전용]
	// -------------------------------------------------------------

	// 이 구역에서 방 하나를 시작. 플레이어들을 슬롯으로 옮기고 시점을 이 구역으로 돌린다
	void BeginRoom(const FRoomNode& InRoom, const TArray<ATerminusPlayerState*>& InPlayers);

	// 이 구역의 방을 끝냈다고 표시. 전 구역이 끝났는지는 서브시스템이 판단
	void MarkCleared();

	// 구역 비우기. 배틀러를 원래 자리로 돌려놓고 시점을 지도로 돌린다
	void Release();

	// -------------------------------------------------------------
	// [조회]
	// -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	bool IsInUse() const { return bInUse; }

	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	bool IsCleared() const { return bCleared; }

	// 지금 이 구역에서 진행 중인 방. IsInUse 가 false 면 의미 없음
	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	const FRoomNode& GetRoom() const { return Room; }

	const TArray<TObjectPtr<ATerminusPlayerState>>& GetOccupants() const { return Occupants; }

	// 슬롯 월드 좌표. 슬롯은 구역 기준 상대 좌표로 저장돼 있음
	FVector GetPlayerSlotLocation(int32 SlotIndex) const;
	FVector GetMonsterSlotLocation(int32 SlotIndex) const;

	int32 GetNumPlayerSlots() const { return PlayerSlots.Num(); }
	int32 GetNumMonsterSlots() const { return MonsterSlots.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<USceneComponent> Root;

	// 이 구역을 비추는 카메라. 플레이어 시점이 이 액터로 바뀌면 이 카메라로 보게 됨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<UCameraComponent> AreaCamera;

	// 플레이어 서는 자리 (구역 기준 상대 좌표). 뷰포트에서 기즈모로 끌어서 옮길 수 있음
	// 0번이 맨 왼쪽. 구역에 들어온 순서(PlayerArray 순)대로 채움
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Slots", meta = (MakeEditWidget = true))
	TArray<FVector> PlayerSlots;

	// 몬스터 서는 자리. 기획상 한 방에 1 ~ 3 마리
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Slots", meta = (MakeEditWidget = true))
	TArray<FVector> MonsterSlots;

	// ---- 복제되는 진행 상태. 클라의 HUD / 관전이 보고 쓸 것

	UPROPERTY(Replicated)
	bool bInUse = false;

	UPROPERTY(Replicated)
	bool bCleared = false;

	UPROPERTY(Replicated)
	FRoomNode Room;

	// 이 구역에 들어와 있는 플레이어들
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dungeon Area")
	TArray<TObjectPtr<ATerminusPlayerState>> Occupants;

private:
	// 들어오기 전 배틀러 위치. 구역을 비울 때 지도 화면의 원래 자리로 돌려놓기 위함 (서버만)
	TMap<TWeakObjectPtr<APawn>, FVector> ReturnLocations;
};
