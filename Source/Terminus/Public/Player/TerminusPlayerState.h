// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CharacterTypes.h"
#include "Data/RunTypes.h"
#include "TerminusPlayerState.generated.h"

class ADungeonArea;

/**
 * 
 */
UCLASS()
class TERMINUS_API ATerminusPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	ECharacterClass GetCharacterClass() const { return RunState.CharacterClass; }
	
	void SetCharacterClass(ECharacterClass InClass);
	
	bool IsReady() const { return bReady; }
	
	void SetReady(bool bInReady);
	
	// 주점에서 던전으로 출발할 때 서버가 부름. 클래스 기본 스텟을 런 스텟으로 복사하고 진행도 초기화
	void BeginRun();
	
	// 방을 클리어하고 그 방으로 진행. 지도상 현재 위치와 진행 레벨을 갱신하고 선택을 비운다
	void AdvanceToRoom(int32 TargetRoomId, int32 TargetRow);
	
	// Seamless Travel할 때 들고 가는게 아니라 복사시켜서 새로 만들어야 함
	virtual void CopyProperties(APlayerState* NewPS) override;
	
	// -------------------------------------------------------------
	// [Setter 함수들] - 오직 서버(Authority)에서만 동작
	// -------------------------------------------------------------
	void SetSelectedRoomId(int32 InRoomId);
	void SetCurrentRoomInfo(int32 InRoomId, int32 InMapLevel);
	void SetRunState(const FRunState& InRunState);
	
	// -------------------------------------------------------------
	// [Getter 함수들]
	// -------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE FRunState GetRunState() const { return RunState; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetSelectedRoomId() const { return RunState.SelectedRoomId; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetCurrentRoomId() const { return RunState.CurrentRoomId; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetCurrentMapLevel() const { return RunState.CurrentMapLevel; }
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunStateChanged, const FRunState&, NewRunState);
	UPROPERTY(BlueprintAssignable, Category = "Run")
	FOnRunStateChanged OnRunStateChanged;

	// -------------------------------------------------------------
	// [던전 구역]
	// -------------------------------------------------------------

	// 서버만. 구역 배정 / 해제 때 ADungeonArea 가 부름
	void SetCurrentArea(ADungeonArea* InArea);

	// 지금 들어가 있는 구역. 지도 화면이면 nullptr
	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	ADungeonArea* GetCurrentArea() const { return CurrentArea; }

protected:
	// 준비 완료 여부. 서버만 바꾸고 클라는 복제로 받기만 함
	// 준비는 처음에 들어갈 때만 중요한 요소이므로 RunState에는 안들어감
	UPROPERTY(Replicated)
	bool bReady = false;
	
	// 이동 간 유지되어야 할 데이터
	// ReplicatedUsing 이어야 클라에서 OnRep_RunState 가 불림 -> 지도 UI 갱신이 여기에 걸려 있음
	UPROPERTY(ReplicatedUsing = OnRep_RunState)
	FRunState RunState;
	
	UFUNCTION()
	void OnRep_RunState();

	// 지금 들어가 있는 던전 구역. 방 하나 동안만 의미 있는 값이라 RunState 에는 안 넣음
	// 모든 클라에 복제 -> 누가 어느 구역에 있는지 다 알 수 있음 (관전 / 난입용)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentArea)
	TObjectPtr<ADungeonArea> CurrentArea;

	// 내 것이면 시점을 이 구역(없으면 지도)으로 돌림
	UFUNCTION()
	void OnRep_CurrentArea();
};
