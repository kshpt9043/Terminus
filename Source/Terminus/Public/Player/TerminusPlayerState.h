// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CharacterTypes.h"
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
	
	ECharacterClass GetCharacterClass() const { return PlayerCharacter; }
	
	void SetCharacterClass(ECharacterClass InClass);
	
	bool IsReady() const { return bReady; }
	
	void SetReady(bool bInReady);
	
protected:
	UPROPERTY(Replicated)
	ECharacterClass PlayerCharacter = ECharacterClass::Monk;
	
	// 준비 완료 여부. 서버만 바꾸고 클라는 복제로 받기만 함
	UPROPERTY(Replicated)
	bool bReady = false;
};
