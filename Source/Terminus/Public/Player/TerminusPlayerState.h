// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CharacterTypes.h"
#include "Data/RunTypes.h"
#include "TerminusPlayerState.generated.h"

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
	
protected:
	// 준비 완료 여부. 서버만 바꾸고 클라는 복제로 받기만 함
	// 준비는 처음에 들어갈 때만 중요한 요소이므로 RunState에는 안들어감
	UPROPERTY(Replicated)
	bool bReady = false;
	
	// 이동 간 유지되어야 할 데이터
	UPROPERTY(Replicated)
	FRunState RunState;
	
	UFUNCTION()
	void OnRep_RunState();
};
