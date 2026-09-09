// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/CharacterTypes.h"
#include "TerminusPlayerController.generated.h"

class UTavernWidget;

/**
 * 
 */
UCLASS()
class TERMINUS_API ATerminusPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UTavernWidget> TavernWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UTavernWidget> TavernWidget;
	
public:
	UFUNCTION(Server, Reliable)
	void Server_SelectCharacter(ECharacterClass InClass);
	
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);
	
	UFUNCTION(Server, Reliable)
	void Server_StartGame();
	
};
