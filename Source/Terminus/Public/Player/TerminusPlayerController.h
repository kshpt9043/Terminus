// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UTavernWidget> TavernWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UTavernWidget> TavernWidget;
	
};
