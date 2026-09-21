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
	
	// 시점 결정을 엔진 대신 우리가 함 -> 고정 카메라 레벨은 폰이 아니라 카메라를 봐야해서
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;
	
	// 그 레벨의 고정 카메라. 없으면 nullptr
	class ACameraActor* FindFixedCamera() const;
	
public:
	UFUNCTION(Server, Reliable)
	void Server_SelectCharacter(ECharacterClass InClass);
	
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);
	
	UFUNCTION(Server, Reliable)
	void Server_StartGame();
	
};
