#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/MapManager.h"
#include "Data/RewardTypes.h"
#include "DungeonArea.generated.h"

class UCameraComponent;
class UChildActorComponent;
class ATerminusPlayerState;
class ADungeonAreaSet;
class UDungeonThemeData;
class UDungeonCombatComponent;

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

	// 이 구역에서 방 하나를 시작. 테마에 맞는 무대를 띄우고, 플레이어들을 슬롯으로 옮기고, 시점을 이 구역으로 돌린다
	// Theme 이 없으면 무대는 지금 떠 있는 것(레벨에서 지정한 미리보기) 그대로
	void BeginRoom(const FRoomNode& InRoom, const TArray<ATerminusPlayerState*>& InPlayers, const UDungeonThemeData* Theme);

	// 이 구역의 방을 끝냈다고 표시. 전 구역이 끝났는지는 서브시스템이 판단
	void MarkCleared();

	// 이 구역 전멸 (전투 패배). 구출 / 난입 / 런 끝은 서브시스템이 판단
	void MarkWiped();
	bool IsWiped() const { return bWiped; }

	// [난입] 다른 구역 사람들이 이 구역에 들어와 남은 몬스터와 이어서 싸움
	void BeginIntervention(const TArray<ATerminusPlayerState*>& Joiners);

	// [배신 전투] 팀은 플레이어 자리, 배신자는 몬스터 자리에 세우고 배신 전투 시작
	void BeginBetrayal(const TArray<ATerminusPlayerState*>& Team, ATerminusPlayerState* Betrayer, const UDungeonThemeData* Theme, float VictimHealthCut, int32 StatPerOpponent);

	// 구역 비우기. 배틀러를 원래 자리로 돌려놓고 시점을 지도로 돌린다
	void Release();

	// [휴식터] 휴식 / 탐색 중 하나만 (기획 10-07, 사용자 결정 10-08: 선택지 화면). 한 사람 한 번, 전원이 고르면 방이 끝남
	// 휴식: 회복 대상 고르기 (PC 의 Server_ChooseRestTarget). 같은 휴식터 사람만. 대상은 최대 체력의 RestHealRatio 만큼 회복
	void HandleRestChoice(ATerminusPlayerState* Chooser, ATerminusPlayerState* Target);

	// 탐색 (PC 의 Server_ChooseRestExplore): 던전 재화 RestExploreCurrency + RestExploreRelicChance 확률로 유물
	void HandleRestExplore(ATerminusPlayerState* Chooser);

	// 휴식터 회복량 (최대 체력 비율). 기획: 20%
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Rest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RestHealRatio = 0.2f;

	// 탐색 던전 재화 (최소, 최대). 기획: '소량' -> 수치 미정이라 임시 (몬스터방 10 의 절반쯤)
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Rest")
	FIntPoint RestExploreCurrency = FIntPoint(5, 10);

	// 탐색 유물 확률. 기획: 5%. 유물은 가디언 보상과 같은 조건 (공용 / 내 직업, 지금 계층, 이번 런에 안 나온 것)
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Rest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RestExploreRelicChance = 0.05f;

	// [이벤트] 후보 고르기 (PC 의 Server_ChooseEvent). 한 사람 한 번, 전원이 고르면 방이 끝남
	// ReplaceSlot = 강화 칸이 꽉 찼을 때 바꿀 칸, RelicRow = '유물 변화(선택)' 에서 바꿀 내 유물
	void HandleEventChoice(ATerminusPlayerState* Chooser, int32 Index, int32 ReplaceSlot, FName RelicRow);

	// 이벤트 후보 수 (사용자 결정: 3개, 각자 받음)
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event", meta = (ClampMin = "1"))
	int32 EventOptionCount = 3;

	// 아래 수치는 기획 미정이라 임시 (사용자 결정 2026-10-06: 제안값 그대로)
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	FIntPoint EventCurrency = FIntPoint(15, 30);

	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventPermanentHealth = 5;

	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventPermanentAttack = 1;

	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventPermanentDefense = 1;

	// 공용 일시 버프: 공격 +2, 방어 -1, 전투 3번
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventTempAttack = 2;

	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventTempDefensePenalty = 1;

	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event", meta = (ClampMin = "1"))
	int32 EventTempBattles = 3;

	// 테마 일시 버프: 공격 +3, 디버프 없음, 전투 EventTempBattles 번
	UPROPERTY(EditAnywhere, Category = "Dungeon Area|Event")
	int32 EventThemeAttack = 3;

	// -------------------------------------------------------------
	// [조회]
	// -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	bool IsInUse() const { return bInUse; }

	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	bool IsCleared() const { return bCleared; }

	// 싸움이 끝났는가 (클리어했거나, 승리 후 결과·보상 화면). 싸우는 중이거나 전멸해서 구출 / 난입을 기다리면 false
	bool IsFightOver() const;

	// 지금 이 구역에서 진행 중인 방. IsInUse 가 false 면 의미 없음
	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	const FRoomNode& GetRoom() const { return Room; }

	const TArray<TObjectPtr<ATerminusPlayerState>>& GetOccupants() const { return Occupants; }

	// 슬롯 월드 좌표. 슬롯은 구역 기준 상대 좌표로 저장돼 있음
	FVector GetPlayerSlotLocation(int32 SlotIndex) const;
	FVector GetMonsterSlotLocation(int32 SlotIndex) const;

	int32 GetNumPlayerSlots() const { return PlayerSlots.Num(); }
	int32 GetNumMonsterSlots() const { return MonsterSlots.Num(); }

	// 구역 기준 상대 좌표 그대로. 무대 세트의 에디터 미리보기가 씀
	const TArray<FVector>& GetPlayerSlots() const { return PlayerSlots; }
	const TArray<FVector>& GetMonsterSlots() const { return MonsterSlots; }
	const UCameraComponent* GetAreaCamera() const;

	// 이 구역의 전투 진행. 전투 방이 아니면 IsInCombat() == false
	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	UDungeonCombatComponent* GetCombat() const { return Combat; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// SetDisplay 를 구역 원점에 고정. 세트 좌표 = 구역 좌표라는 전제(슬롯 / 카메라 / 배경 맞춤)가 여기에 걸려 있음
	void PinSetDisplayToOrigin();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<USceneComponent> Root;

	// 이 구역을 비추는 카메라. 플레이어 시점이 이 액터로 바뀌면 이 카메라로 보게 됨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<UCameraComponent> AreaCamera;

	// 전투 진행 (몬스터 스폰, 사이클, 승패). 몬스터 클래스 / 보정값은 여기 디테일에서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<UDungeonCombatComponent> Combat;

	// 무대(구역 세트)가 뜨는 자리. 방이 시작되면 테마에서 고른 세트로 바뀜
	// 레벨에서 이 컴포넌트의 Child Actor Class 에 세트 BP 를 지정하면 에디터에서 미리 볼 수 있음
	// (슬롯 기즈모 / 카메라와 같이 보면서 소품 배치를 맞출 때)
	// 위치는 항상 구역 원점으로 고정됨. 무대를 옮기고 싶으면 세트 BP 안에서 옮길 것
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon Area")
	TObjectPtr<UChildActorComponent> SetDisplay;

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

	// 전멸해서 구출 / 난입을 기다리는 중 (서버만)
	bool bWiped = false;

	UPROPERTY(Replicated)
	FRoomNode Room;

	// 이 구역에 들어와 있는 플레이어들
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dungeon Area")
	TArray<TObjectPtr<ATerminusPlayerState>> Occupants;

	// 지금 띄울 무대. 서버가 고르고 클래스만 복제 -> 각 머신이 SetDisplay 로 로컬에 띄움
	// (무대는 연출 전용이라 액터 자체는 복제하지 않음)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSetClass)
	TSubclassOf<ADungeonAreaSet> CurrentSetClass;

	UFUNCTION()
	void OnRep_CurrentSetClass();

private:
	// 들어오기 전 배틀러 위치. 구역을 비울 때 지도 화면의 원래 자리로 돌려놓기 위함 (서버만)
	TMap<TWeakObjectPtr<APawn>, FVector> ReturnLocations;

	// 휴식터: 이미 고른 사람 (서버만)
	TSet<TWeakObjectPtr<ATerminusPlayerState>> RestChosen;
	bool bResting = false;
	FTimerHandle RestTimer;

	void BeginRest();
	void CheckRestDone();
	void FinishRest();

	// 이벤트: 각자 받은 후보 / 이미 고른 사람 (서버만)
	TMap<TWeakObjectPtr<ATerminusPlayerState>, TArray<FEventOption>> EventOffers;
	TSet<TWeakObjectPtr<ATerminusPlayerState>> EventChosen;
	bool bInEvent = false;
	FTimerHandle EventTimer;

	void BeginEvent();
	void CheckEventDone();
	void FinishEvent();
	TArray<FEventOption> MakeEventOptions(const ATerminusPlayerState* PS) const;
	bool ApplyEventOption(ATerminusPlayerState* PS, const FEventOption& Option, int32 ReplaceSlot, FName RelicRow, FString& OutResult);
};
